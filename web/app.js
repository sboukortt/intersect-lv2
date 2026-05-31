/**
 * Intersect web demo — decode stereo audio, process with WASM, play L/R and center separately.
 */

const fileInput = document.getElementById('file-input');
const fileInfo = document.getElementById('file-info');
const fftSizeInput = document.getElementById('fft-size');
const overlapInput = document.getElementById('overlap');
const processBtn = document.getElementById('process-btn');
const statusEl = document.getElementById('status');
const progressEl = document.getElementById('progress');

const lrAudio = document.getElementById('lr-audio');
const centerAudio = document.getElementById('center-audio');
const lrPlay = document.getElementById('lr-play');
const lrPause = document.getElementById('lr-pause');
const centerPlay = document.getElementById('center-play');
const centerPause = document.getElementById('center-pause');
const lrVolume = document.getElementById('lr-volume');
const centerVolume = document.getElementById('center-volume');
const lrSeek = document.getElementById('lr-seek');
const centerSeek = document.getElementById('center-seek');
const lrTime = document.getElementById('lr-time');
const centerTime = document.getElementById('center-time');

/** @type {AudioBuffer | null} */
let sourceBuffer = null;
/** @type {import('./intersect.js').default | null} */
let wasmModule = null;

const CHUNK_FRAMES = 8192;

function formatTime(seconds) {
	if (!Number.isFinite(seconds) || seconds < 0) {
		return '0:00';
	}
	const m = Math.floor(seconds / 60);
	const s = Math.floor(seconds % 60);
	return `${m}:${String(s).padStart(2, '0')}`;
}

function setStatus(message, { processing = false } = {}) {
	statusEl.textContent = message;
	statusEl.classList.toggle('processing', processing);
}

function enablePlayback(enabled) {
	for (const el of [
		lrPlay, lrPause, centerPlay, centerPause, lrSeek, centerSeek,
	]) {
		el.disabled = !enabled;
	}
}

/** Minimal Wasm module that uses v128 (simd128); validates SIMD support. */
const WASM_SIMD_PROBE = new Uint8Array([
	0, 97, 115, 109, 1, 0, 0, 0, 1, 5, 1, 96, 0, 1, 123, 3, 2, 1, 0, 10, 10,
	1, 8, 0, 65, 0, 253, 15, 253, 98, 11,
]);

function wasmSimdSupported() {
	try {
		return WebAssembly.validate(WASM_SIMD_PROBE);
	} catch {
		return false;
	}
}

async function loadWasm() {
	if (wasmModule) {
		return wasmModule;
	}
	if (!wasmSimdSupported()) {
		throw new Error(
			'This browser does not support WebAssembly SIMD (required by intersect.wasm).',
		);
	}
	if (typeof IntersectWasmModule !== 'function') {
		throw new Error(
			'WASM module not loaded. Run web/build.sh and serve this folder over HTTP.',
		);
	}
	wasmModule = await IntersectWasmModule({
		locateFile: (path) => path,
	});
	return wasmModule;
}

/**
 * @param {AudioBuffer} buffer
 * @returns {{ left: Float32Array, right: Float32Array, sampleRate: number }}
 */
function stereoChannels(buffer) {
	const channels = buffer.numberOfChannels;
	const length = buffer.length;
	const left = new Float32Array(length);
	const right = new Float32Array(length);
	const ch0 = buffer.getChannelData(0);
	if (channels === 1) {
		left.set(ch0);
		right.set(ch0);
	} else {
		left.set(ch0);
		right.set(buffer.getChannelData(1));
	}
	return { left, right, sampleRate: buffer.sampleRate };
}

/**
 * @param {Float32Array} left
 * @param {Float32Array} right
 * @param {number} fftSize
 * @param {number} overlap
 * @param {(ratio: number) => void} onProgress
 */
async function processIntersect(left, right, fftSize, overlap, onProgress) {
	const mod = await loadWasm();
	const create = mod.cwrap('intersect_wasm_create', 'number', ['number', 'number']);
	const destroy = mod.cwrap('intersect_wasm_destroy', null, ['number']);
	const activate = mod.cwrap('intersect_wasm_activate', 'number', ['number']);
	const process = mod.cwrap('intersect_wasm_process', 'number', [
		'number', 'number', 'number', 'number', 'number', 'number', 'number',
	]);

	const n = left.length;
	const outLeft = new Float32Array(n);
	const outRight = new Float32Array(n);
	const outCenter = new Float32Array(n);

	const handle = create(fftSize, overlap);
	if (!handle) {
		throw new Error('Failed to create Intersect processor');
	}

	try {
		const latency = activate(handle);
		if (latency < 0) {
			throw new Error('Failed to activate processor');
		}

		const ptrInL = mod._malloc(n * 4);
		const ptrInR = mod._malloc(n * 4);
		const ptrOutL = mod._malloc(n * 4);
		const ptrOutR = mod._malloc(n * 4);
		const ptrOutC = mod._malloc(n * 4);

		try {
			mod.HEAPF32.set(left, ptrInL >> 2);
			mod.HEAPF32.set(right, ptrInR >> 2);

			let offset = 0;
			while (offset < n) {
				const count = Math.min(CHUNK_FRAMES, n - offset);
				process(
					handle,
					ptrInL + offset * 4,
					ptrInR + offset * 4,
					ptrOutL + offset * 4,
					ptrOutR + offset * 4,
					ptrOutC + offset * 4,
					count,
				);
				offset += count;
				onProgress(offset / n);
			}

			outLeft.set(mod.HEAPF32.subarray(ptrOutL >> 2, (ptrOutL >> 2) + n));
			outRight.set(mod.HEAPF32.subarray(ptrOutR >> 2, (ptrOutR >> 2) + n));
			outCenter.set(mod.HEAPF32.subarray(ptrOutC >> 2, (ptrOutC >> 2) + n));
		} finally {
			mod._free(ptrInL);
			mod._free(ptrInR);
			mod._free(ptrOutL);
			mod._free(ptrOutR);
			mod._free(ptrOutC);
		}

		if (latency > 0 && latency < n) {
			outLeft.copyWithin(0, latency);
			outRight.copyWithin(0, latency);
			outCenter.copyWithin(0, latency);
			outLeft.fill(0, n - latency);
			outRight.fill(0, n - latency);
			outCenter.fill(0, n - latency);
		}

		return { outLeft, outRight, outCenter, latency };
	} finally {
		destroy(handle);
	}
}

/**
 * @param {Float32Array} left
 * @param {Float32Array} right
 * @param {number} sampleRate
 */
function buildLrWavBlob(left, right, sampleRate) {
	return encodeWav([left, right], sampleRate);
}

/**
 * @param {Float32Array} center
 * @param {number} sampleRate
 */
function buildCenterWavBlob(center, sampleRate) {
	return encodeWav([center], sampleRate);
}

/**
 * @param {Float32Array[]} channels
 * @param {number} sampleRate
 */
function encodeWav(channels, sampleRate) {
	const numChannels = channels.length;
	const length = channels[0].length;
	const bytesPerSample = 2;
	const blockAlign = numChannels * bytesPerSample;
	const dataSize = length * blockAlign;
	const buffer = new ArrayBuffer(44 + dataSize);
	const view = new DataView(buffer);

	const writeString = (offset, str) => {
		for (let i = 0; i < str.length; i++) {
			view.setUint8(offset + i, str.charCodeAt(i));
		}
	};

	writeString(0, 'RIFF');
	view.setUint32(4, 36 + dataSize, true);
	writeString(8, 'WAVE');
	writeString(12, 'fmt ');
	view.setUint32(16, 16, true);
	view.setUint16(20, 1, true);
	view.setUint16(22, numChannels, true);
	view.setUint32(24, sampleRate, true);
	view.setUint32(28, sampleRate * blockAlign, true);
	view.setUint16(32, blockAlign, true);
	view.setUint16(34, 16, true);
	writeString(36, 'data');
	view.setUint32(40, dataSize, true);

	let offset = 44;
	for (let i = 0; i < length; i++) {
		for (let c = 0; c < numChannels; c++) {
			const sample = Math.max(-1, Math.min(1, channels[c][i]));
			view.setInt16(
				offset,
				sample < 0 ? sample * 0x8000 : sample * 0x7fff,
				true,
			);
			offset += 2;
		}
	}

	return new Blob([buffer], { type: 'audio/wav' });
}

function wirePlayer(audio, playBtn, pauseBtn, volumeSlider, seekSlider, timeEl) {
	let objectUrl = null;

	const updateTime = () => {
		const cur = audio.currentTime || 0;
		const dur = audio.duration || 0;
		timeEl.textContent = `${formatTime(cur)} / ${formatTime(dur)}`;
		if (dur > 0) {
			seekSlider.value = String(Math.round((cur / dur) * 1000));
		}
	};

	playBtn.addEventListener('click', () => {
		void audio.play();
	});
	pauseBtn.addEventListener('click', () => {
		audio.pause();
	});
	volumeSlider.addEventListener('input', () => {
		audio.volume = Number(volumeSlider.value);
	});
	seekSlider.addEventListener('input', () => {
		if (audio.duration) {
			audio.currentTime = (Number(seekSlider.value) / 1000) * audio.duration;
		}
	});
	audio.addEventListener('timeupdate', updateTime);
	audio.addEventListener('loadedmetadata', updateTime);

	return {
		setSource(blob) {
			if (objectUrl) {
				URL.revokeObjectURL(objectUrl);
			}
			objectUrl = URL.createObjectURL(blob);
			audio.src = objectUrl;
			audio.load();
		},
	};
}

const lrPlayer = wirePlayer(lrAudio, lrPlay, lrPause, lrVolume, lrSeek, lrTime);
const centerPlayer = wirePlayer(
	centerAudio,
	centerPlay,
	centerPause,
	centerVolume,
	centerSeek,
	centerTime,
);

fileInput.addEventListener('change', async () => {
	const file = fileInput.files?.[0];
	sourceBuffer = null;
	enablePlayback(false);
	processBtn.disabled = true;

	if (!file) {
		fileInfo.textContent = 'No file loaded.';
		return;
	}

	setStatus('Decoding audio…', { processing: true });
	try {
		const arrayBuffer = await file.arrayBuffer();
		const ctx = new AudioContext();
		sourceBuffer = await ctx.decodeAudioData(arrayBuffer.slice(0));
		await ctx.close();

		const duration = sourceBuffer.duration;
		fileInfo.textContent = `${file.name} — ${sourceBuffer.numberOfChannels} channel(s), ${sourceBuffer.sampleRate} Hz, ${formatTime(duration)}`;
		processBtn.disabled = false;
		setStatus('Ready to process.');
	} catch (err) {
		setStatus(`Could not decode file: ${err.message}`);
	}
});

processBtn.addEventListener('click', async () => {
	if (!sourceBuffer) {
		return;
	}

	processBtn.disabled = true;
	progressEl.hidden = false;
	progressEl.value = 0;
	setStatus('Loading processor…', { processing: true });

	try {
		await loadWasm();
		const { left, right, sampleRate } = stereoChannels(sourceBuffer);
		const fftSize = Number(fftSizeInput.value) || 4096;
		const overlap = Number(overlapInput.value) || 128;

		setStatus('Processing…', { processing: true });
		const { outLeft, outRight, outCenter } = await processIntersect(
			left,
			right,
			fftSize,
			overlap,
			(ratio) => {
				progressEl.value = Math.round(ratio * 100);
			},
		);

		const lrBlob = buildLrWavBlob(outLeft, outRight, sampleRate);
		const centerBlob = buildCenterWavBlob(outCenter, sampleRate);
		lrPlayer.setSource(lrBlob);
		centerPlayer.setSource(centerBlob);
		enablePlayback(true);
		setStatus('Done. Use the players below to listen.');
	} catch (err) {
		setStatus(`Error: ${err.message}`);
		enablePlayback(false);
	} finally {
		progressEl.hidden = true;
		processBtn.disabled = !sourceBuffer;
	}
});

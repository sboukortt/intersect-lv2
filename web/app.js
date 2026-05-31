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
/** @type {Worker | null} */
let processorWorker = null;
/** @type {Promise<Worker> | null} */
let processorWorkerReady = null;

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

/**
 * @param {DataView} view
 * @param {number} offset
 */
function readFourCC(view, offset) {
	let s = '';
	for (let i = 0; i < 4; i++) {
		s += String.fromCharCode(view.getUint8(offset + i));
	}
	return s;
}

/**
 * Best-effort native sample rate before decode (WAV / FLAC). MP3 and others fall back.
 * @param {ArrayBuffer} arrayBuffer
 * @returns {number | null}
 */
function sniffSampleRate(arrayBuffer) {
	if (arrayBuffer.byteLength < 44) {
		return null;
	}
	const view = new DataView(arrayBuffer);

	if (readFourCC(view, 0) === 'RIFF' && readFourCC(view, 8) === 'WAVE') {
		let offset = 12;
		while (offset + 8 <= arrayBuffer.byteLength) {
			const chunkId = readFourCC(view, offset);
			const chunkSize = view.getUint32(offset + 4, true);
			if (chunkId === 'fmt ' && chunkSize >= 16 && offset + 16 <= arrayBuffer.byteLength) {
				const rate = view.getUint32(offset + 12, true);
				return rate > 0 ? rate : null;
			}
			offset += 8 + chunkSize + (chunkSize % 2);
		}
	}

	if (readFourCC(view, 0) === 'fLaC' && arrayBuffer.byteLength >= 21) {
		// STREAMINFO is metadata block 0; sample rate is a 20-bit field at streaminfo+10.
		const sampleRate =
			(view.getUint8(18) << 12) |
			(view.getUint8(19) << 4) |
			(view.getUint8(20) >> 4);
		return sampleRate > 0 ? sampleRate : null;
	}

	return null;
}

/**
 * Decode without tying to the audio output device sample rate.
 * @param {ArrayBuffer} arrayBuffer
 * @param {number} contextSampleRate from file header, or 44100 when unknown
 */
async function decodeAudioFile(arrayBuffer, contextSampleRate) {
	const offline = new OfflineAudioContext({
		numberOfChannels: 2,
		length: 1,
		sampleRate: contextSampleRate,
	});
	return offline.decodeAudioData(arrayBuffer.slice(0));
}

function getProcessorWorker() {
	if (processorWorkerReady) {
		return processorWorkerReady;
	}
	if (!wasmSimdSupported()) {
		return Promise.reject(
			new Error(
				'This browser does not support WebAssembly SIMD (required by intersect.wasm).',
			),
		);
	}
	processorWorkerReady = new Promise((resolve, reject) => {
		const worker = new Worker('processor-worker.js');
		const onStartup = (event) => {
			const { type, message } = event.data;
			if (type === 'ready') {
				worker.removeEventListener('message', onStartup);
				processorWorker = worker;
				resolve(worker);
				return;
			}
			if (type === 'error') {
				worker.removeEventListener('message', onStartup);
				reject(new Error(message ?? 'Worker failed to start'));
			}
		};
		worker.addEventListener('message', onStartup);
		worker.onerror = () => {
			reject(new Error('Processor worker failed to load. Run web/build.sh.'));
		};
	});
	return processorWorkerReady;
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
function processIntersect(left, right, fftSize, overlap, onProgress) {
	return getProcessorWorker().then(
		(worker) =>
			new Promise((resolve, reject) => {
				const leftIn = new Float32Array(left);
				const rightIn = new Float32Array(right);

				const onMessage = (event) => {
					const msg = event.data;
					switch (msg.type) {
						case 'progress':
							onProgress(msg.ratio);
							if (msg.phase === 'plan') {
								setStatus('Planning FFT…', { processing: true });
							}
							break;
						case 'done':
							worker.removeEventListener('message', onMessage);
							resolve({
								outLeft: new Float32Array(msg.outLeft),
								outRight: new Float32Array(msg.outRight),
								outCenter: new Float32Array(msg.outCenter),
							});
							break;
						case 'error':
							worker.removeEventListener('message', onMessage);
							reject(new Error(msg.message ?? 'Processing failed'));
							break;
						default:
							break;
					}
				};

				worker.addEventListener('message', onMessage);
				worker.postMessage(
					{
						type: 'process',
						left: leftIn.buffer,
						right: rightIn.buffer,
						fftSize,
						overlap,
					},
					[leftIn.buffer, rightIn.buffer],
				);
			}),
	);
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
		const sniffed = sniffSampleRate(arrayBuffer);
		sourceBuffer = await decodeAudioFile(arrayBuffer, sniffed ?? 44100);

		const duration = sourceBuffer.duration;
		const rateMismatch =
			sniffed != null && sniffed !== sourceBuffer.sampleRate
				? ` (header ${sniffed} Hz)`
				: '';
		fileInfo.textContent = `${file.name} — ${sourceBuffer.numberOfChannels} channel(s), ${sourceBuffer.sampleRate} Hz${rateMismatch}, ${formatTime(duration)}`;
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

/**
 * Runs Intersect WASM off the main thread; posts progress and results back.
 */
/* global IntersectWasmModule */

importScripts('intersect.js');

const CHUNK_FRAMES = 8192;

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

/** @type {Awaited<ReturnType<typeof IntersectWasmModule>> | null} */
let mod = null;

async function ensureModule() {
	if (mod) {
		return mod;
	}
	if (!wasmSimdSupported()) {
		throw new Error('WebAssembly SIMD is not supported in this browser.');
	}
	if (typeof IntersectWasmModule !== 'function') {
		throw new Error('intersect.js failed to load in the worker.');
	}
	mod = await IntersectWasmModule({ locateFile: (path) => path });
	return mod;
}

/**
 * @param {Float32Array} left
 * @param {Float32Array} right
 * @param {number} fftSize
 * @param {number} overlap
 */
function runProcess(left, right, fftSize, overlap) {
	const create = mod.cwrap('intersect_wasm_create', 'number', ['number', 'number']);
	const destroy = mod.cwrap('intersect_wasm_destroy', null, ['number']);
	const activate = mod.cwrap('intersect_wasm_activate', 'number', ['number']);
	const flush = mod.cwrap('intersect_wasm_flush', null, ['number']);
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
		self.postMessage({ type: 'progress', ratio: 0, phase: 'plan' });
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
				self.postMessage({ type: 'progress', ratio: offset / n, phase: 'process' });
			}

			flush(handle);

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

		return { outLeft, outRight, outCenter };
	} finally {
		destroy(handle);
	}
}

ensureModule()
	.then(() => {
		self.postMessage({ type: 'ready' });
	})
	.catch((err) => {
		self.postMessage({
			type: 'error',
			message: err?.message ?? String(err),
		});
	});

self.onmessage = (event) => {
	const msg = event.data;
	if (msg.type !== 'process') {
		return;
	}

	(async () => {
		try {
			await ensureModule();
			const left = new Float32Array(msg.left);
			const right = new Float32Array(msg.right);
			const { outLeft, outRight, outCenter } = runProcess(
				left,
				right,
				msg.fftSize,
				msg.overlap,
			);
			self.postMessage(
				{
					type: 'done',
					outLeft,
					outRight,
					outCenter,
				},
				[outLeft.buffer, outRight.buffer, outCenter.buffer],
			);
		} catch (err) {
			self.postMessage({
				type: 'error',
				message: err?.message ?? String(err),
			});
		}
	})();
};

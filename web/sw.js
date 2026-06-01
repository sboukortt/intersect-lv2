/* Intersect web demo — offline cache for static assets (including WASM). */

const CACHE = 'intersect-web-v1';

/** Paths relative to this script (serve the whole web/ directory). */
const ASSETS = [
	'./',
	'index.html',
	'style.css',
	'app.js',
	'processor-worker.js',
	'intersect.js',
	'intersect.wasm',
	'manifest.webmanifest',
	'icon.svg',
	'NOTICES.txt',
	'sw.js',
];

self.addEventListener('install', (event) => {
	event.waitUntil(
		(async () => {
			const cache = await caches.open(CACHE);
			await Promise.allSettled(
				ASSETS.map(async (path) => {
					try {
						await cache.add(new Request(path, { cache: 'reload' }));
					} catch {
						/* intersect.js / intersect.wasm missing until ./build.sh */
					}
				}),
			);
			await self.skipWaiting();
		})(),
	);
});

self.addEventListener('activate', (event) => {
	event.waitUntil(
		(async () => {
			const keys = await caches.keys();
			await Promise.all(
				keys.filter((key) => key !== CACHE).map((key) => caches.delete(key)),
			);
			await self.clients.claim();
		})(),
	);
});

self.addEventListener('fetch', (event) => {
	if (event.request.method !== 'GET') {
		return;
	}
	const url = new URL(event.request.url);
	if (url.origin !== self.location.origin) {
		return;
	}

	event.respondWith(
		(async () => {
			const cached = await caches.match(event.request);
			if (cached) {
				return cached;
			}
			try {
				const response = await fetch(event.request);
				if (response.ok && response.type === 'basic') {
					const cache = await caches.open(CACHE);
					void cache.put(event.request, response.clone());
				}
				return response;
			} catch {
				if (event.request.mode === 'navigate') {
					return (await caches.match('index.html')) ?? Response.error();
				}
				return Response.error();
			}
		})(),
	);
});

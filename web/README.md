# Intersect

Browser UI for the Intersect stereo → L/R/center algorithm. Audio stays on your machine: decoding and playback use the Web Audio API; processing uses WebAssembly compiled from the same C++ core as the LV2 plugin.

## Build

Requirements:

- [Emscripten](https://emscripten.org/) (`emcc` on your `PATH`)
- `curl`, `tar`, `make`
- Meson (once) to fetch the Highway subproject: `meson setup build` from the repo root

Then:

```bash
./web/build.sh
```

This produces `web/intersect.js` and `web/intersect.wasm`. The web build uses
**PFFFT** (vendored under ``third_party/pffft/``) instead of FFTW, built with **Wasm SIMD128**
(`-msimd128`) for the FFT kernels. The plugin core also uses Highway with the same flag.
Ordered spectra use PFFFT’s native layout ``[DC, Nyquist, re₁, im₁, …]`` (see ``src/intersect.cc``).
Planning is instant.
Browsers without WebAssembly SIMD cannot run this module.

FFT window sizes must be valid for PFFFT: ``N = 2^a × 3^b × 5^c`` with ``a ≥ 5``
(e.g. 32, 48, 64, …, 4096, 8192). Other values are rounded up to the nearest
valid size.

To force a clean rebuild, remove ``web/build/``.

## Licenses

The web demo footer credits `pffft.wasm` (BSD-2-Clause) and upstream PFFFT
(FFTPACK-derived terms). ``NOTICES.txt`` includes the full text for both, for
binary redistribution (``intersect.wasm`` embeds the vendored PFFFT sources from
``third_party/pffft/``, originally packaged at
https://github.com/JorenSix/pffft.wasm).

## Run locally

Browsers block `file://` loading of WASM, so serve the `web/` directory over HTTP:

```bash
cd web && python3 -m http.server 8080
```

Open http://localhost:8080/

## Offline use

The demo is a small progressive web app:

- ``manifest.webmanifest`` — installable metadata and theme colors
- ``sw.js`` — caches HTML, CSS, JS, the processor worker, ``intersect.js``, and ``intersect.wasm``

After you open the app once while online (with ``./build.sh`` already run), those assets stay available offline. Processing still starts when you click **Process**; the WASM module is loaded from the cache instead of the network.

Service workers require a secure context (``https://`` or ``http://localhost``). They do not run on ``file://`` URLs.

To pick up a new build after ``./build.sh``, bump the ``CACHE`` name in ``sw.js`` or clear the site data in your browser.

## Usage

Decoded audio uses `OfflineAudioContext` (not the device `AudioContext`) so files are
not resampled to the output device rate before processing. WAV and FLAC sample rates
are read from the file header; other formats default to 44100 Hz for decode.

1. Choose a stereo audio file.
2. Optionally adjust FFT window size and overlap (defaults match the LV2 plugin).
3. Click **Process** (WASM runs in a Web Worker so the page stays responsive).
4. Use the **Left & right** and **Center** players independently (play, pause, volume, seek).
5. After processing, **Save WAV** writes 16-bit stereo L/R or mono center locally (e.g. ``mytrack-left-right.wav``, ``mytrack-center.wav``).

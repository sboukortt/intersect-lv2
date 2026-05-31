# Intersect Web Demo

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

This produces `web/intersect.js` and `web/intersect.wasm`. The build uses
`-msimd128` so the plugin core runs with Highway’s WASM SIMD target (faster
than the previous scalar-only build). Browsers without WebAssembly SIMD cannot
run this module; all current Chromium, Firefox, and Safari versions support it.

To force a clean rebuild after changing SIMD settings, remove `web/build/`.

## Run locally

Browsers block `file://` loading of WASM, so serve the `web/` directory over HTTP:

```bash
cd web && python3 -m http.server 8080
```

Open http://localhost:8080/

## Usage

Decoded audio uses `OfflineAudioContext` (not the device `AudioContext`) so files are
not resampled to the output device rate before processing. WAV and FLAC sample rates
are read from the file header; other formats default to 44100 Hz for decode.

1. Choose a stereo audio file.
2. Optionally adjust FFT window size and overlap (defaults match the LV2 plugin).
3. Click **Process** (WASM runs in a Web Worker so the page stays responsive).
   FFT plans use `FFTW_ESTIMATE` in the browser build; the desktop plugin still
   uses patient planning for higher quality plans.
4. Use the **Left & right** and **Center** players independently (play, pause, volume, seek).

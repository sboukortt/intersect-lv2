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

This produces `web/intersect.js` and `web/intersect.wasm`.

## Run locally

Browsers block `file://` loading of WASM, so serve the `web/` directory over HTTP:

```bash
cd web && python3 -m http.server 8080
```

Open http://localhost:8080/

## Usage

1. Choose a stereo audio file.
2. Optionally adjust FFT window size and overlap (defaults match the LV2 plugin).
3. Click **Process**.
4. Use the **Left & right** and **Center** players independently (play, pause, volume, seek).

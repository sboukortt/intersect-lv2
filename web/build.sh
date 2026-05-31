#!/usr/bin/env bash
# Build Intersect WebAssembly module (intersect.js + intersect.wasm).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WEB="$ROOT/web"
BUILD="$WEB/build"
SRC="$ROOT/src"
FFTW_VERSION="3.3.10"
FFTW_DIR="$BUILD/fftw-${FFTW_VERSION}"

# WebAssembly SIMD (fixed-width 128-bit). Requires a browser with Wasm SIMD support.
SIMD_FLAGS=(-msimd128)

if ! command -v emcc >/dev/null 2>&1; then
	echo "Emscripten (emcc) not found. Install the emsdk and run: source emsdk_env.sh" >&2
	exit 1
fi

mkdir -p "$BUILD"

HWY_DIR="$ROOT/subprojects/highway"
if [[ ! -d "$HWY_DIR/hwy" ]]; then
	HWY_DIR="$ROOT/build/subprojects/highway"
fi
if [[ ! -d "$HWY_DIR/hwy" ]]; then
	echo "Highway sources not found. Run: meson setup build" >&2
	exit 1
fi

HWY_LIB="$BUILD/libhwy-simd128.a"
HWY_STAMP="$BUILD/.hwy-simd128.stamp"
if [[ ! -f "$HWY_LIB" ]] || [[ ! -f "$HWY_STAMP" ]]; then
	HWY_OBJECTS=()
	for src in abort aligned_allocator nanobenchmark per_target perf_counters print profiler targets timer; do
		obj="$BUILD/hwy_${src}.o"
		emcc -c "$HWY_DIR/hwy/${src}.cc" -o "$obj" \
			"${SIMD_FLAGS[@]}" \
			-I"$HWY_DIR" \
			-O3 -std=c++17
		HWY_OBJECTS+=("$obj")
	done
	emar rcs "$HWY_LIB" "${HWY_OBJECTS[@]}"
	date -u +%Y-%m-%dT%H:%M:%SZ >"$HWY_STAMP"
fi

FFTW_LIB="$BUILD/libfftw3f.a"
if [[ ! -f "$FFTW_LIB" ]]; then
	if [[ ! -d "$FFTW_DIR" ]]; then
		archive="$BUILD/fftw-${FFTW_VERSION}.tar.gz"
		if [[ ! -f "$archive" ]]; then
			curl -fsSL "http://www.fftw.org/fftw-${FFTW_VERSION}.tar.gz" -o "$archive"
		fi
		tar -xzf "$archive" -C "$BUILD"
	fi
	pushd "$FFTW_DIR" >/dev/null
	if [[ ! -f Makefile ]]; then
		emconfigure ./configure \
			--enable-float \
			--enable-static \
			--disable-shared \
			--disable-fortran \
			--disable-doc \
			--disable-threads \
			--with-our-malloc \
			CFLAGS="${SIMD_FLAGS[*]} -O3"
	fi
	emmake make -j"$(nproc 2>/dev/null || echo 2)"
	popd >/dev/null
	cp -f "$FFTW_DIR"/.libs/libfftw3f.a "$FFTW_LIB"
fi

COMMON_FLAGS=(
	-O3
	-std=c++17
	"${SIMD_FLAGS[@]}"
	-I"$SRC"
	-I"$HWY_DIR"
	-I"$FFTW_DIR/api"
)

SOURCES=(
	"$WEB/wasm_api.cc"
	"$SRC/engine.cc"
	"$SRC/intersect.cc"
)

emcc "${SOURCES[@]}" \
	"${COMMON_FLAGS[@]}" \
	"$HWY_LIB" \
	"$FFTW_LIB" \
	-o "$WEB/intersect.js" \
	-s MODULARIZE=1 \
	-s EXPORT_NAME=IntersectWasmModule \
	-s EXPORTED_FUNCTIONS='["_intersect_wasm_create","_intersect_wasm_destroy","_intersect_wasm_activate","_intersect_wasm_process","_intersect_wasm_flush","_intersect_wasm_get_latency","_malloc","_free"]' \
	-s EXPORTED_RUNTIME_METHODS='["cwrap","getValue","setValue","HEAPF32"]' \
	-s ALLOW_MEMORY_GROWTH=1 \
	-s ENVIRONMENT=web,worker \
	-s FILESYSTEM=0

echo "Built $WEB/intersect.js and $WEB/intersect.wasm (WebAssembly SIMD enabled)"

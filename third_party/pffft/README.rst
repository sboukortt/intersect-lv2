Vendored from https://github.com/JorenSix/pffft.wasm (BSD-2-Clause; see that
repository's LICENSE), which packages PFFFT (Pretty Fast FFT) by Julien Pommier
(FFTPACK-derived license in ``pffft.h``).

Wasm SIMD128 shuffles in ``pffft.c`` were corrected (``VSWAPHL``, ``VTRANSPOSE4``)
so ``pffft_transform_ordered`` matches the scalar layout on Emscripten.

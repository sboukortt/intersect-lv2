#ifndef H_INTERSECT_TYPES
#define H_INTERSECT_TYPES

/*  Copyright 2015 Sami Boukortt

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <fftw3.h>
#include <hwy/aligned_allocator.h>

enum {
	FFT_SIZE,
	OVERLAP_FACTOR,
	INPUT_CHANNEL_LEFT,
	INPUT_CHANNEL_RIGHT,
	LATENCY,
	OUTPUT_CHANNEL_LEFT,
	OUTPUT_CHANNEL_RIGHT,
	OUTPUT_CHANNEL_CENTER,
};

enum class Effect {
	Intersect,
	SymmetricDifference,
	Upmix,
};

enum {LEFT, RIGHT, CENTER};

struct Intersect {
	float* input[2];
	float* output[3];

	float* fft_size_hint;
	float* overlap_factor_hint;
	float* latency;

	int fft_size, overlap_factor, fft_jump_size;
	float normalization_factor;

	/* deviation from the “stable state” where there are `fft_jump_size`
	   samples in `output_buffer` and `fft_size` samples in `input_buffer`

	   When `deviation` = `fft_jump_size`, the output buffer is empty and
	   the input buffers are full. Time to refill!
	 */
	int deviation;

	float* input_buffer[2];
	float* ifft_result;
	hwy::AlignedFreeUniquePtr<float[]> output_buffer[3];

	fftwf_complex* transformed[2];
	fftwf_complex* pre_output;

	fftwf_plan plan_r2c, plan_c2r;
};

#endif

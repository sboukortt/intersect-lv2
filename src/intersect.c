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

#include "intersect.h"

#include <fftw3.h>
#include <string.h>
#include "types.h"
#include "util.h"

static float amplitude_squared(const float c[2]) {
	return c[0] * c[0] + c[1] * c[1];
}

static void run(LV2_Handle handle, uint32_t sample_count, Effect effect) {
	Intersect *intersect = handle;
	float *cursor_input [2] = {intersect->input [LEFT], intersect->input [RIGHT]},
	      *cursor_output[3] = {intersect->output[LEFT], intersect->output[RIGHT], intersect->output[CENTER]};

	while (sample_count > 0) {
		const uint32_t block_size = min(sample_count, intersect->fft_jump_size - intersect->deviation);
		uint32_t i;

		for (i = 0; i < block_size; ++i) {
			cursor_output[0][i] = intersect->output_buffer[CENTER][i + intersect->deviation] * intersect->normalization_factor;
		}

		switch (effect) {
			case INTERSECT: break;

			case UPMIX:
				memcpy(cursor_output[CENTER], cursor_output[0], block_size * sizeof(float));

				/* falltrough */
			case SYMMETRIC_DIFFERENCE: {
				int c_;
				for (c_ = 0; c_ < 2; ++c_) {
					const int c = 1 - c_;
					for (i = 0; i < block_size; ++i) {
						cursor_output[c][i] = intersect->output_buffer[c][i + intersect->deviation] - cursor_output[0][i];
					}
				}
				break;
			}
		}

		for (i = 0; i < 2; ++i) {
			memcpy(
				intersect->input_buffer[i] + intersect->fft_size - intersect->fft_jump_size + intersect->deviation,
				cursor_input[i],
				block_size * sizeof(float)
			);
		}

		intersect->deviation += block_size;

		if (intersect->deviation == intersect->fft_jump_size) {
			memmove(intersect->output_buffer[CENTER], intersect->output_buffer[CENTER] + intersect->fft_jump_size, (intersect->fft_size - intersect->fft_jump_size) * sizeof(float));
			memset(intersect->output_buffer[CENTER] + (intersect->fft_size - intersect->fft_jump_size), 0, intersect->fft_jump_size * sizeof(float));

			for (i = 0; i < 2; ++i) {
				fftwf_execute(intersect->plan_r2c[i]);
			}

			for (i = 0; i < intersect->fft_size / 2 + 1; ++i) {
				const float * const left   = intersect->transformed[LEFT] [i],
				            * const right  = intersect->transformed[RIGHT][i],
				            * const winner = (amplitude_squared(left) < amplitude_squared(right)) ? left : right;
				memcpy(intersect->pre_output[i], winner, 2 * sizeof(float));
			}

			fftwf_execute(intersect->plan_c2r);

			for (i = 0; i < intersect->fft_size; ++i) {
				intersect->output_buffer[CENTER][i] += intersect->ifft_result[i];
			}
			for (i = 0; i < 2; ++i) {
				memcpy(intersect->output_buffer[i], intersect->input_buffer[i], intersect->fft_jump_size * sizeof(float));
				memmove(intersect->input_buffer[i], intersect->input_buffer[i] + intersect->fft_jump_size, (intersect->fft_size - intersect->fft_jump_size) * sizeof(float));
			}

			intersect->deviation = 0;
		}

		cursor_input[LEFT]    += block_size;
		cursor_input[RIGHT]   += block_size;
		cursor_output[LEFT]   += block_size;
		cursor_output[RIGHT]  += block_size;
		cursor_output[CENTER] += block_size;
		sample_count          -= block_size;
	}

	if (intersect->latency != NULL) {
		*intersect->latency = intersect->fft_size;
	}
}

void intersect_run(LV2_Handle handle, uint32_t sample_count) {
	run(handle, sample_count, INTERSECT);
}

void symmetric_difference_run(LV2_Handle handle, uint32_t sample_count) {
	run(handle, sample_count, SYMMETRIC_DIFFERENCE);
}

void upmix_run(LV2_Handle handle, uint32_t sample_count) {
	run(handle, sample_count, UPMIX);
}

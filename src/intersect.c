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

#define INTERSECT_URI "https://sami.boukortt.com/plugins/intersect"

#include <fftw3.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "lv2/lv2plug.in/ns/lv2core/lv2.h"
#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) > (b) ? (a) : (b))

#ifdef _WIN32
	#define DLLEXPORT __declspec(dllexport)
#else
	#define DLLEXPORT
#endif

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

enum {
	INTERSECT,
	SYMMETRIC_DIFFERENCE,
	UPMIX,
}
typedef Effect;

enum {LEFT, RIGHT, CENTER};

struct {
	float *input[2], *output[3];

	float *fft_size_hint, *overlap_factor_hint;
	float *latency;

	uint32_t fft_size, overlap_factor, fft_jump_size;
	float normalization_factor;

	/* deviation from the “stable state” where there are `fft_jump_size`
	   samples in `output_buffer` and `fft_size` samples in `input_buffer`

	   When `deviation` = `fft_jump_size`, the output buffer is empty and
	   the input buffers are full. Time to refill!
	 */
	uint32_t deviation;

	float *input_buffer[2],
	      *ifft_result,
	      *output_buffer[3];

	fftwf_complex *transformed[2],
	              *pre_output;

	fftwf_plan plan_r2c[2],
	           plan_c2r;
}
typedef Intersect;

static LV2_Handle intersect_instantiate(const LV2_Descriptor *descriptor,
                                        double sample_rate,
                                        const char *bundle_path,
                                        const LV2_Feature *const *features)
{
	return calloc(1, sizeof(Intersect));
}

static void intersect_cleanup(LV2_Handle handle) {
	free(handle);
}

static void intersect_activate(LV2_Handle handle) {
	Intersect *intersect = handle;
	int i;

	intersect->fft_size = max(1, *intersect->fft_size_hint + .5f);
	if (intersect->fft_size % 2 != 0) {
		++intersect->fft_size;
	}
	intersect->overlap_factor = max(1, min(intersect->fft_size, *intersect->overlap_factor_hint + .5f));
	intersect->fft_jump_size = intersect->fft_size / intersect->overlap_factor;
	intersect->normalization_factor = 1.f / (intersect->fft_size * intersect->overlap_factor);

	intersect->deviation = 0;

	intersect->input_buffer[LEFT]  = fftwf_alloc_real(intersect->fft_size);
	intersect->input_buffer[RIGHT] = fftwf_alloc_real(intersect->fft_size);

	intersect->ifft_result = fftwf_alloc_real(intersect->fft_size);

	intersect->output_buffer[LEFT]   = calloc(intersect->fft_jump_size, sizeof(float));
	intersect->output_buffer[RIGHT]  = calloc(intersect->fft_jump_size, sizeof(float));
	intersect->output_buffer[CENTER] = calloc(intersect->fft_size, sizeof(float));

	intersect->transformed[LEFT]  = fftwf_alloc_complex(intersect->fft_size / 2 + 1);
	intersect->transformed[RIGHT] = fftwf_alloc_complex(intersect->fft_size / 2 + 1);
	intersect->pre_output         = fftwf_alloc_complex(intersect->fft_size / 2 + 1);

	for (i = 0; i < 2; ++i) {
		memset(intersect->input_buffer[i], 0, intersect->fft_size * sizeof(float));
		intersect->plan_r2c[i] = fftwf_plan_dft_r2c_1d(intersect->fft_size, intersect->input_buffer[i], intersect->transformed[i], FFTW_MEASURE | FFTW_DESTROY_INPUT);
	}
	intersect->plan_c2r = fftwf_plan_dft_c2r_1d(intersect->fft_size, intersect->pre_output, intersect->ifft_result, FFTW_MEASURE | FFTW_DESTROY_INPUT);
}

static void intersect_deactivate(LV2_Handle handle) {
	Intersect *intersect = handle;

	fftwf_free(intersect->input_buffer[LEFT]);
	fftwf_free(intersect->input_buffer[RIGHT]);
	fftwf_free(intersect->ifft_result);
	free(intersect->output_buffer[LEFT]);
	free(intersect->output_buffer[RIGHT]);
	free(intersect->output_buffer[CENTER]);

	fftwf_free(intersect->transformed[LEFT]);
	fftwf_free(intersect->transformed[RIGHT]);
	fftwf_free(intersect->pre_output);

	fftwf_destroy_plan(intersect->plan_r2c[LEFT]);
	fftwf_destroy_plan(intersect->plan_r2c[RIGHT]);
	fftwf_destroy_plan(intersect->plan_c2r);

	fftwf_cleanup();
}

static void intersect_connect_port(LV2_Handle handle, uint32_t port, void *data_location) {
	Intersect *intersect = handle;
	switch (port) {
		case FFT_SIZE:
			intersect->fft_size_hint = data_location;
			break;

		case OVERLAP_FACTOR:
			intersect->overlap_factor_hint = data_location;
			break;

		case INPUT_CHANNEL_LEFT:
			intersect->input[LEFT] = data_location;
			break;

		case INPUT_CHANNEL_RIGHT:
			intersect->input[RIGHT] = data_location;
			break;

		case LATENCY:
			intersect->latency = data_location;
			break;

		case OUTPUT_CHANNEL_LEFT:
			intersect->output[LEFT] = data_location;
			break;

		case OUTPUT_CHANNEL_RIGHT:
			intersect->output[RIGHT] = data_location;
			break;

		case OUTPUT_CHANNEL_CENTER:
			intersect->output[CENTER] = data_location;
			break;
	}
}

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

static void intersect_run(LV2_Handle handle, uint32_t sample_count) {
	run(handle, sample_count, INTERSECT);
}

static void symmetric_difference_run(LV2_Handle handle, uint32_t sample_count) {
	run(handle, sample_count, SYMMETRIC_DIFFERENCE);
}

static void upmix_run(LV2_Handle handle, uint32_t sample_count) {
	run(handle, sample_count, UPMIX);
}

DLLEXPORT const LV2_Descriptor *lv2_descriptor(uint32_t i) {
	static const LV2_Descriptor intersect_descriptor = {
		.URI = INTERSECT_URI "#Intersect",
		.instantiate = intersect_instantiate,
		.connect_port = intersect_connect_port,
		.activate = intersect_activate,
		.run = intersect_run,
		.deactivate = intersect_deactivate,
		.cleanup = intersect_cleanup,
		.extension_data = NULL,
	};

	static const LV2_Descriptor symmetric_difference_descriptor = {
		.URI = INTERSECT_URI "#SymmetricDifference",
		.instantiate = intersect_instantiate,
		.connect_port = intersect_connect_port,
		.activate = intersect_activate,
		.run = symmetric_difference_run,
		.deactivate = intersect_deactivate,
		.cleanup = intersect_cleanup,
		.extension_data = NULL,
	};

	static const LV2_Descriptor upmix_descriptor = {
		.URI = INTERSECT_URI "#Upmix",
		.instantiate = intersect_instantiate,
		.connect_port = intersect_connect_port,
		.activate = intersect_activate,
		.run = upmix_run,
		.deactivate = intersect_deactivate,
		.cleanup = intersect_cleanup,
		.extension_data = NULL,
	};

	switch (i) {
		case 0:
			return &intersect_descriptor;

		case 1:
			return &symmetric_difference_descriptor;

		case 2:
			return &upmix_descriptor;

		default:
			return NULL;
	}
}

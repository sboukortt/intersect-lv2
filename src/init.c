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

#include "init.h"

#include <fftw3.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "util.h"

LV2_Handle intersect_instantiate(const LV2_Descriptor *descriptor,
                                 double sample_rate,
                                 const char *bundle_path,
                                 const LV2_Feature *const *features)
{
	return calloc(1, sizeof(Intersect));
}

void intersect_cleanup(LV2_Handle handle) {
	free(handle);
}

void intersect_activate(LV2_Handle handle) {
	Intersect *intersect = handle;
	int i;

	intersect->fft_size = max(1, *intersect->fft_size_hint);
	if (intersect->fft_size % 2 != 0) {
		++intersect->fft_size;
	}
	intersect->overlap_factor = max(1, min(intersect->fft_size, *intersect->overlap_factor_hint));
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

void intersect_deactivate(LV2_Handle handle) {
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

void intersect_connect_port(LV2_Handle handle, uint32_t port, void *data_location) {
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

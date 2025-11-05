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

#include <algorithm>
#include <cstring>
#include <fftw3.h>
#include "types.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "init.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

namespace {

namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

HWY_ATTR size_t pad_floats(const size_t count) {
	HWY_FULL(float) d;
	return (count + (MaxLanes(d) - 1)) / MaxLanes(d) * MaxLanes(d);
}

HWY_ATTR void zero_fill(float* const HWY_RESTRICT array, const size_t n) {
	HWY_FULL(float) d;
	for (size_t i = 0; i < n; i += Lanes(d)) {
		hn::Store(hn::Zero(d), d, &array[i]);
	}
}

}

#if HWY_ONCE

HWY_EXPORT(pad_floats);
HWY_EXPORT(zero_fill);

#endif

}

#if HWY_ONCE

LV2_Handle intersect_instantiate(const LV2_Descriptor* descriptor,
                                 double sample_rate,
                                 const char* bundle_path,
                                 const LV2_Feature* const * features)
{
	return new Intersect;
}

void intersect_cleanup(LV2_Handle handle) {
	auto* intersect = static_cast<Intersect*>(handle);
	delete intersect;
}

void intersect_activate(LV2_Handle handle) {
	auto* intersect = static_cast<Intersect*>(handle);

	const auto pad = HWY_DYNAMIC_DISPATCH(pad_floats);
	const auto zero_fill = HWY_DYNAMIC_DISPATCH(zero_fill);

	intersect->fft_size = std::max<uint32_t>(1, *intersect->fft_size_hint);
	if (intersect->fft_size % 2 != 0) {
		++intersect->fft_size;
	}
	intersect->overlap_factor = std::clamp<uint32_t>(*intersect->overlap_factor_hint, 1, intersect->fft_size);
	intersect->fft_jump_size = intersect->fft_size / intersect->overlap_factor;
	intersect->normalization_factor = 1.f / (intersect->fft_size * intersect->overlap_factor);

	intersect->deviation = 0;

	intersect->input_buffer[LEFT]  = fftwf_alloc_real(intersect->fft_size);
	intersect->input_buffer[RIGHT] = fftwf_alloc_real(intersect->fft_size);

	intersect->ifft_result = fftwf_alloc_real(pad(intersect->fft_size));

	intersect->output_buffer[LEFT]  = hwy::AllocateAligned<float>(pad(intersect->fft_jump_size));
	zero_fill(intersect->output_buffer[LEFT].get(), intersect->fft_jump_size);
	intersect->output_buffer[RIGHT] = hwy::AllocateAligned<float>(pad(intersect->fft_jump_size));
	zero_fill(intersect->output_buffer[RIGHT].get(), intersect->fft_jump_size);
	intersect->output_buffer[CENTER]= hwy::AllocateAligned<float>(pad(intersect->fft_size));
	zero_fill(intersect->output_buffer[CENTER].get(), intersect->fft_size);

	intersect->transformed[LEFT]  = fftwf_alloc_complex(intersect->fft_size / 2 + 1);
	intersect->transformed[RIGHT] = fftwf_alloc_complex(intersect->fft_size / 2 + 1);
	intersect->pre_output         = fftwf_alloc_complex(intersect->fft_size / 2 + 1);

	for (int i = 0; i < 2; ++i) {
		memset(intersect->input_buffer[i], 0, intersect->fft_size * sizeof(float));
	}
	intersect->plan_r2c = fftwf_plan_many_dft_r2c(
		/*rank=*/1,
		/*n=*/&intersect->fft_size,
		/*howmany=*/2,
		/*in*/intersect->input_buffer[LEFT],
		/*inembed=*/NULL,
		/*istride=*/1,
		/*idist=*/intersect->input_buffer[RIGHT] - intersect->input_buffer[LEFT],
		/*out=*/intersect->transformed[LEFT],
		/*onembed=*/NULL,
		/*ostride=*/1,
		/*odist=*/intersect->transformed[RIGHT] - intersect->transformed[LEFT],
		FFTW_PATIENT
	);
	intersect->plan_c2r = fftwf_plan_dft_c2r_1d(intersect->fft_size, intersect->pre_output, intersect->ifft_result, FFTW_MEASURE | FFTW_DESTROY_INPUT);
}

void intersect_deactivate(LV2_Handle handle) {
	auto* intersect = static_cast<Intersect*>(handle);

	fftwf_free(intersect->input_buffer[LEFT]);
	fftwf_free(intersect->input_buffer[RIGHT]);
	fftwf_free(intersect->ifft_result);

	for (auto& output_buffer: intersect->output_buffer) {
		output_buffer.reset();
	}

	fftwf_free(intersect->transformed[LEFT]);
	fftwf_free(intersect->transformed[RIGHT]);
	fftwf_free(intersect->pre_output);

	fftwf_destroy_plan(intersect->plan_r2c);
	fftwf_destroy_plan(intersect->plan_c2r);

	fftwf_cleanup();
}

void intersect_connect_port(LV2_Handle handle, uint32_t port, void* data) {
	auto* intersect = static_cast<Intersect*>(handle);
	auto* data_location = static_cast<float*>(data);
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

#endif

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

#include "engine.h"

#include <algorithm>
#include <cstring>
#include <vector>
#include "fft.h"
#include "intersect.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "engine.cc"
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

void intersect_engine_activate(Intersect* intersect) {
	const auto pad = HWY_DYNAMIC_DISPATCH(pad_floats);
	const auto zero_fill = HWY_DYNAMIC_DISPATCH(zero_fill);

	intersect->fft_size = std::max<uint32_t>(1, static_cast<uint32_t>(*intersect->fft_size_hint));
	if (intersect->fft_size % 2 != 0) {
		++intersect->fft_size;
	}
#if defined(INTERSECT_USE_PFFFT)
	intersect->fft_size = intersect_pffft_snap_size(intersect->fft_size);
#endif
	intersect->overlap_factor = std::clamp<uint32_t>(
		static_cast<uint32_t>(*intersect->overlap_factor_hint),
		1u,
		static_cast<uint32_t>(intersect->fft_size)
	);
	intersect->fft_jump_size = intersect->fft_size / intersect->overlap_factor;
	intersect->normalization_factor = 1.f / (intersect->fft_size * intersect->overlap_factor);

	intersect->deviation = 0;

	intersect->input_buffer[LEFT]  = intersect_alloc_real_buffer(intersect->fft_size);
	intersect->input_buffer[RIGHT] = intersect_alloc_real_buffer(intersect->fft_size);

	intersect->ifft_result = intersect_alloc_real_buffer(pad(intersect->fft_size));

	intersect->output_buffer[LEFT]  = hwy::AllocateAligned<float>(pad(intersect->fft_jump_size));
	zero_fill(intersect->output_buffer[LEFT].get(), intersect->fft_jump_size);
	intersect->output_buffer[RIGHT] = hwy::AllocateAligned<float>(pad(intersect->fft_jump_size));
	zero_fill(intersect->output_buffer[RIGHT].get(), intersect->fft_jump_size);
	intersect->output_buffer[CENTER]= hwy::AllocateAligned<float>(pad(intersect->fft_size));
	zero_fill(intersect->output_buffer[CENTER].get(), intersect->fft_size);

	for (int i = 0; i < 2; ++i) {
		memset(intersect->input_buffer[i], 0, intersect->fft_size * sizeof(float));
	}

	intersect_fft_activate(intersect);
}

void intersect_engine_deactivate(Intersect* intersect) {
	intersect_fft_deactivate(intersect);

	intersect_free_real_buffer(intersect->input_buffer[LEFT]);
	intersect_free_real_buffer(intersect->input_buffer[RIGHT]);
	intersect_free_real_buffer(intersect->ifft_result);

	for (auto& output_buffer: intersect->output_buffer) {
		output_buffer.reset();
	}
}

uint32_t intersect_engine_latency(const Intersect* intersect) {
	return static_cast<uint32_t>(intersect->fft_size);
}

void intersect_engine_process_upmix(
	Intersect* intersect,
	const float* input_left,
	const float* input_right,
	float* output_left,
	float* output_right,
	float* output_center,
	uint32_t sample_count)
{
	intersect->input[LEFT]  = const_cast<float*>(input_left);
	intersect->input[RIGHT] = const_cast<float*>(input_right);
	intersect->output[LEFT]  = output_left;
	intersect->output[RIGHT] = output_right;
	intersect->output[CENTER] = output_center;
	intersect->latency = nullptr;

	upmix_run(intersect, sample_count);
}

void intersect_engine_flush_upmix(Intersect* intersect) {
	const uint32_t latency = intersect_engine_latency(intersect);
	if (latency == 0) {
		return;
	}

	std::vector<float> silence(latency, 0.f);
	std::vector<float> discard_left(latency);
	std::vector<float> discard_right(latency);
	std::vector<float> discard_center(latency);
	intersect->input[LEFT]  = silence.data();
	intersect->input[RIGHT] = silence.data();
	intersect->output[LEFT]  = discard_left.data();
	intersect->output[RIGHT] = discard_right.data();
	intersect->output[CENTER] = discard_center.data();
	intersect->latency = nullptr;
	upmix_run(intersect, latency);
}

#endif

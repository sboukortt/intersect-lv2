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

#include <algorithm>
#include <complex>
#include <fftw3.h>
#include <string.h>
#include "types.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "intersect.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include <hwy/contrib/algo/transform-inl.h>

namespace {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

static float magnitude_squared(const float c[2]) {
	return c[0] * c[0] + c[1] * c[1];
}

HWY_ATTR void run(LV2_Handle handle, uint32_t sample_count, Effect effect) {
	Intersect *intersect = static_cast<Intersect*>(handle);
	HWY_FULL(float) d;
	float *cursor_input [2] = {intersect->input [LEFT], intersect->input [RIGHT]},
	      *cursor_output[3] = {intersect->output[LEFT], intersect->output[RIGHT], intersect->output[CENTER]};

	while (sample_count > 0) {
		const auto block_size = std::min<int>(sample_count, intersect->fft_jump_size - intersect->deviation);

		/* For Intersect (center-only) and Upmix (everything), storing the center channel where it’s
		   going to end up is a no-brainer, but for SymmetricDifference (left and right but *not*
		   center), we can also avoid having to allocate an extra buffer by temporarily storing it
		   where the right channel will go, then subtract it from the left and right output as we write
		   them (effectively “replacing” center with right).
		*/
		const int store_center =
			effect == Effect::Intersect
				? 0
				: effect == Effect::SymmetricDifference
					? 1
					: 2;

		const auto normalization_factor = hn::Set(d, intersect->normalization_factor);
		hn::Transform1(d, cursor_output[store_center], block_size, &intersect->output_buffer[CENTER][intersect->deviation], [&normalization_factor](auto d, auto unused, auto output) HWY_ATTR {
			return hn::Mul(output, normalization_factor);
		});

		if (effect != Effect::Intersect) {
			for (int c = 0; c < 2; ++c) {
				if (c == store_center) {
					hn::Transform1(d, cursor_output[c], block_size, &intersect->output_buffer[c][intersect->deviation], [](auto d, auto center, auto output) HWY_ATTR {
						return hn::Sub(output, center);
					});
				}
				else {
					hn::Transform2(d, cursor_output[c], block_size, &intersect->output_buffer[c][intersect->deviation], cursor_output[store_center], [](auto d, auto unused, auto output, auto center) HWY_ATTR {
						return hn::Sub(output, center);
					});
				}
			}
		}

		for (int i = 0; i < 2; ++i) {
			memcpy(
				intersect->input_buffer[i] + intersect->fft_size - intersect->fft_jump_size + intersect->deviation,
				cursor_input[i],
				block_size * sizeof(float)
			);
		}

		intersect->deviation += block_size;

		if (intersect->deviation == intersect->fft_jump_size) {
			memmove(intersect->output_buffer[CENTER].get(), intersect->output_buffer[CENTER].get() + intersect->fft_jump_size, (intersect->fft_size - intersect->fft_jump_size) * sizeof(float));
			memset(intersect->output_buffer[CENTER].get() + (intersect->fft_size - intersect->fft_jump_size), 0, intersect->fft_jump_size * sizeof(float));

			fftwf_execute(intersect->plan_r2c);

			if (Lanes(d) == 1) {
				for (int i = 0; i < intersect->fft_size / 2 + 1; ++i) {
					const float* const left   = intersect->transformed[LEFT] [i];
					const float* const right  = intersect->transformed[RIGHT][i];
					const float* const winner = magnitude_squared(left) < magnitude_squared(right) ? left : right;
					intersect->pre_output[i][0] = winner[0];
					intersect->pre_output[i][1] = winner[1];
				}
			}
			else {
				hn::Transform2(
					d,
					reinterpret_cast<float*>(intersect->pre_output),
					intersect->fft_size + 2,
					reinterpret_cast<const float*>(intersect->transformed[LEFT]),
					reinterpret_cast<const float*>(intersect->transformed[RIGHT]),
					[](auto d, auto unused, auto left, auto right) HWY_ATTR {
						const auto left_squared = left * left;
						const auto right_squared = right * right;
						const auto left_magnitudes_squared = hn::PairwiseAdd(d, left_squared, left_squared);
						const auto right_magnitudes_squared = hn::PairwiseAdd(d, right_squared, right_squared);
						return hn::IfThenElse(hn::Lt(left_magnitudes_squared, right_magnitudes_squared), left, right);
					}
				);
			}

			fftwf_execute(intersect->plan_c2r);

			for (int i = 0; i < intersect->fft_size; i += Lanes(d)) {
				const auto output = hn::Load(d, &intersect->output_buffer[CENTER][i]);
				const auto ifft = hn::LoadU(d, &intersect->ifft_result[i]);
				hn::Store(hn::Add(output, ifft), d, &intersect->output_buffer[CENTER][i]);
			}
			for (int i = 0; i < 2; ++i) {
				memcpy(intersect->output_buffer[i].get(), intersect->input_buffer[i], intersect->fft_jump_size * sizeof(float));
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

	if (intersect->latency != nullptr) {
		*intersect->latency = intersect->fft_size;
	}
}

}

#if HWY_ONCE

HWY_EXPORT(run);

#endif

}

#if HWY_ONCE

void intersect_run(LV2_Handle handle, uint32_t sample_count) {
	HWY_DYNAMIC_DISPATCH(run)(handle, sample_count, Effect::Intersect);
}

void symmetric_difference_run(LV2_Handle handle, uint32_t sample_count) {
	HWY_DYNAMIC_DISPATCH(run)(handle, sample_count, Effect::SymmetricDifference);
}

void upmix_run(LV2_Handle handle, uint32_t sample_count) {
	HWY_DYNAMIC_DISPATCH(run)(handle, sample_count, Effect::Upmix);
}

#endif

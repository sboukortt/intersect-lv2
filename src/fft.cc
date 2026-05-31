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

#include "fft.h"

#include <cstring>
#include "types.h"

#if defined(INTERSECT_USE_PFFFT)

#include "pffft.h"

float* intersect_alloc_real_buffer(const int samples) {
	return static_cast<float*>(pffft_aligned_malloc(static_cast<size_t>(samples) * sizeof(float)));
}

void intersect_free_real_buffer(float* buffer) {
	pffft_aligned_free(buffer);
}

namespace {

bool pffft_factor_valid(int n) {
	if (n < 32) {
		return false;
	}
	int twos = 0;
	int x = n;
	while (x % 2 == 0) {
		++twos;
		x /= 2;
	}
	if (twos < 5) {
		return false;
	}
	while (x % 3 == 0) {
		x /= 3;
	}
	while (x % 5 == 0) {
		x /= 5;
	}
	return x == 1;
}

float* alloc_buffer(const int samples) {
	return static_cast<float*>(pffft_aligned_malloc(static_cast<size_t>(samples) * sizeof(float)));
}

void free_buffer(float* buffer) {
	pffft_aligned_free(buffer);
}

}  // namespace

bool intersect_pffft_size_valid(const int size) {
	return pffft_factor_valid(size);
}

int intersect_pffft_snap_size(int size) {
	if (size < 32) {
		return 32;
	}
	if (pffft_factor_valid(size)) {
		return size;
	}
	for (int candidate = size; candidate <= size + 2048; ++candidate) {
		if (pffft_factor_valid(candidate)) {
			return candidate;
		}
	}
	return 32;
}

void intersect_fft_activate(Intersect* intersect) {
	const int spectrum_samples = intersect->fft_size;

	intersect->pffft_setup = pffft_new_setup(intersect->fft_size, PFFFT_REAL);
	intersect->pffft_work = alloc_buffer(intersect->fft_size);

	intersect->transformed[LEFT]  = alloc_buffer(spectrum_samples);
	intersect->transformed[RIGHT] = alloc_buffer(spectrum_samples);
	intersect->pre_output         = alloc_buffer(spectrum_samples);

	memset(intersect->transformed[LEFT], 0, static_cast<size_t>(spectrum_samples) * sizeof(float));
	memset(intersect->transformed[RIGHT], 0, static_cast<size_t>(spectrum_samples) * sizeof(float));
	memset(intersect->pre_output, 0, static_cast<size_t>(spectrum_samples) * sizeof(float));
}

void intersect_fft_deactivate(Intersect* intersect) {
	free_buffer(intersect->transformed[LEFT]);
	free_buffer(intersect->transformed[RIGHT]);
	free_buffer(intersect->pre_output);
	free_buffer(intersect->pffft_work);
	intersect->transformed[LEFT]  = nullptr;
	intersect->transformed[RIGHT] = nullptr;
	intersect->pre_output         = nullptr;
	intersect->pffft_work         = nullptr;

	if (intersect->pffft_setup != nullptr) {
		pffft_destroy_setup(intersect->pffft_setup);
		intersect->pffft_setup = nullptr;
	}
}

void intersect_fft_forward(Intersect* intersect) {
	pffft_transform_ordered(
		intersect->pffft_setup,
		intersect->input_buffer[LEFT],
		intersect->transformed[LEFT],
		intersect->pffft_work,
		PFFFT_FORWARD
	);
	pffft_transform_ordered(
		intersect->pffft_setup,
		intersect->input_buffer[RIGHT],
		intersect->transformed[RIGHT],
		intersect->pffft_work,
		PFFFT_FORWARD
	);
}

void intersect_fft_inverse(Intersect* intersect) {
	pffft_transform_ordered(
		intersect->pffft_setup,
		intersect->pre_output,
		intersect->ifft_result,
		intersect->pffft_work,
		PFFFT_BACKWARD
	);
}

#else  // INTERSECT_USE_PFFFT

#include <fftw3.h>

float* intersect_alloc_real_buffer(const int samples) {
	return fftwf_alloc_real(samples);
}

void intersect_free_real_buffer(float* buffer) {
	fftwf_free(buffer);
}

void intersect_fft_activate(Intersect* intersect) {
	const unsigned plan_r2c_flags = FFTW_PATIENT;
	const unsigned plan_c2r_flags = FFTW_MEASURE | FFTW_DESTROY_INPUT;

	intersect->transformed[LEFT]  = reinterpret_cast<float*>(
		fftwf_alloc_complex(intersect->fft_size / 2 + 1)
	);
	intersect->transformed[RIGHT] = reinterpret_cast<float*>(
		fftwf_alloc_complex(intersect->fft_size / 2 + 1)
	);
	intersect->pre_output         = reinterpret_cast<float*>(
		fftwf_alloc_complex(intersect->fft_size / 2 + 1)
	);

	intersect->plan_r2c = fftwf_plan_many_dft_r2c(
		/*rank=*/1,
		/*n=*/&intersect->fft_size,
		/*howmany=*/2,
		/*in*/intersect->input_buffer[LEFT],
		/*inembed=*/NULL,
		/*istride=*/1,
		/*idist=*/intersect->input_buffer[RIGHT] - intersect->input_buffer[LEFT],
		/*out=*/reinterpret_cast<fftwf_complex*>(intersect->transformed[LEFT]),
		/*onembed=*/NULL,
		/*ostride=*/1,
		/*odist=*/reinterpret_cast<fftwf_complex*>(intersect->transformed[RIGHT]) -
			reinterpret_cast<fftwf_complex*>(intersect->transformed[LEFT]),
		plan_r2c_flags
	);
	intersect->plan_c2r = fftwf_plan_dft_c2r_1d(
		intersect->fft_size,
		reinterpret_cast<fftwf_complex*>(intersect->pre_output),
		intersect->ifft_result,
		plan_c2r_flags
	);
}

void intersect_fft_deactivate(Intersect* intersect) {
	fftwf_free(reinterpret_cast<fftwf_complex*>(intersect->transformed[LEFT]));
	fftwf_free(reinterpret_cast<fftwf_complex*>(intersect->transformed[RIGHT]));
	fftwf_free(reinterpret_cast<fftwf_complex*>(intersect->pre_output));
	intersect->transformed[LEFT]  = nullptr;
	intersect->transformed[RIGHT] = nullptr;
	intersect->pre_output         = nullptr;

	fftwf_destroy_plan(intersect->plan_r2c);
	fftwf_destroy_plan(intersect->plan_c2r);
	intersect->plan_r2c = nullptr;
	intersect->plan_c2r = nullptr;
}

void intersect_fft_forward(Intersect* intersect) {
	fftwf_execute(intersect->plan_r2c);
}

void intersect_fft_inverse(Intersect* intersect) {
	fftwf_execute(intersect->plan_c2r);
}

#endif  // INTERSECT_USE_PFFFT

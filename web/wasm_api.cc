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

#include <cstdint>
#include <cstring>
#include <new>
#include <vector>
#include <emscripten.h>
#include <fftw3.h>

#include "../src/engine.h"
#include "../src/types.h"

struct IntersectWasm {
	Intersect intersect;
	float fft_size_hint = 4096.f;
	float overlap_factor_hint = 128.f;
	float latency_report = 0.f;
	bool active = false;
};

extern "C" {

EMSCRIPTEN_KEEPALIVE
IntersectWasm* intersect_wasm_create(uint32_t fft_window_size, uint32_t overlap_factor) {
	auto* handle = new (std::nothrow) IntersectWasm;
	if (handle == nullptr) {
		return nullptr;
	}
	handle->fft_size_hint = static_cast<float>(fft_window_size);
	handle->overlap_factor_hint = static_cast<float>(overlap_factor);
	handle->intersect.fft_size_hint = &handle->fft_size_hint;
	handle->intersect.overlap_factor_hint = &handle->overlap_factor_hint;
	handle->intersect.latency = &handle->latency_report;
	return handle;
}

EMSCRIPTEN_KEEPALIVE
void intersect_wasm_destroy(IntersectWasm* handle) {
	if (handle == nullptr) {
		return;
	}
	if (handle->active) {
		intersect_engine_deactivate(&handle->intersect);
	}
	delete handle;
}

EMSCRIPTEN_KEEPALIVE
int intersect_wasm_activate(IntersectWasm* handle) {
	if (handle == nullptr) {
		return -1;
	}
	if (handle->active) {
		intersect_engine_deactivate(&handle->intersect);
		handle->active = false;
	}
	intersect_engine_activate(&handle->intersect);
	handle->active = true;
	return static_cast<int>(intersect_engine_latency(&handle->intersect));
}

EMSCRIPTEN_KEEPALIVE
uint32_t intersect_wasm_process(
	IntersectWasm* handle,
	const float* input_left,
	const float* input_right,
	float* output_left,
	float* output_right,
	float* output_center,
	uint32_t sample_count)
{
	if (handle == nullptr || !handle->active) {
		return 0;
	}
	intersect_engine_process_upmix(
		&handle->intersect,
		input_left,
		input_right,
		output_left,
		output_right,
		output_center,
		sample_count
	);
	return sample_count;
}

EMSCRIPTEN_KEEPALIVE
void intersect_wasm_flush(IntersectWasm* handle) {
	if (handle == nullptr || !handle->active) {
		return;
	}
	intersect_engine_flush_upmix(&handle->intersect);
}

EMSCRIPTEN_KEEPALIVE
uint32_t intersect_wasm_get_latency(IntersectWasm* handle) {
	if (handle == nullptr || !handle->active) {
		return 0;
	}
	return intersect_engine_latency(&handle->intersect);
}

}

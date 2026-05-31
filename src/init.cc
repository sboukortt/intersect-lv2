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
#include "engine.h"
#include "types.h"

LV2_Handle intersect_instantiate(const LV2_Descriptor* descriptor,
                                 double sample_rate,
                                 const char* bundle_path,
                                 const LV2_Feature* const * features)
{
	fftwf_make_planner_thread_safe();
	return new Intersect;
}

void intersect_cleanup(LV2_Handle handle) {
	auto* intersect = static_cast<Intersect*>(handle);
	delete intersect;
}

void intersect_activate(LV2_Handle handle) {
	intersect_engine_activate(static_cast<Intersect*>(handle));
}

void intersect_deactivate(LV2_Handle handle) {
	intersect_engine_deactivate(static_cast<Intersect*>(handle));
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

#ifndef H_INTERSECT_ENGINE
#define H_INTERSECT_ENGINE

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
#include "types.h"

void intersect_engine_activate(Intersect* intersect);
void intersect_engine_deactivate(Intersect* intersect);

void intersect_engine_process_upmix(
	Intersect* intersect,
	const float* input_left,
	const float* input_right,
	float* output_left,
	float* output_right,
	float* output_center,
	uint32_t sample_count);

uint32_t intersect_engine_latency(const Intersect* intersect);

#endif

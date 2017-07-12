#ifndef H_INTERSECT_INTERSECT
#define H_INTERSECT_INTERSECT

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

#include <lv2/lv2plug.in/ns/lv2core/lv2.h>
#include <stdint.h>

void intersect_run(LV2_Handle handle, uint32_t sample_count);
void symmetric_difference_run(LV2_Handle handle, uint32_t sample_count);
void upmix_run(LV2_Handle handle, uint32_t sample_count);

#endif

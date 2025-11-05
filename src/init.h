#ifndef H_INTERSECT_INIT
#define H_INTERSECT_INIT

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
#include <lv2/core/lv2.h>

LV2_Handle intersect_instantiate(const LV2_Descriptor* descriptor,
                                 double sample_rate,
                                 const char* bundle_path,
                                 const LV2_Feature* const * features);
void intersect_cleanup(LV2_Handle handle);
void intersect_activate(LV2_Handle handle);
void intersect_deactivate(LV2_Handle handle);
void intersect_connect_port(LV2_Handle handle, uint32_t port, void* data_location);

#endif

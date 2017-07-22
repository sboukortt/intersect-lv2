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
#include <stddef.h>
#include <stdint.h>
#include "init.h"
#include "intersect.h"

#define INTERSECT_URI "https://sami.boukortt.com/plugins/intersect"

LV2_SYMBOL_EXPORT
const LV2_Descriptor *lv2_descriptor(uint32_t i) {
	static const LV2_Descriptor intersect_descriptor = {
		.URI = INTERSECT_URI "#Intersect",
		.instantiate = intersect_instantiate,
		.connect_port = intersect_connect_port,
		.activate = intersect_activate,
		.run = intersect_run,
		.deactivate = intersect_deactivate,
		.cleanup = intersect_cleanup,
		.extension_data = NULL,
	};

	static const LV2_Descriptor symmetric_difference_descriptor = {
		.URI = INTERSECT_URI "#SymmetricDifference",
		.instantiate = intersect_instantiate,
		.connect_port = intersect_connect_port,
		.activate = intersect_activate,
		.run = symmetric_difference_run,
		.deactivate = intersect_deactivate,
		.cleanup = intersect_cleanup,
		.extension_data = NULL,
	};

	static const LV2_Descriptor upmix_descriptor = {
		.URI = INTERSECT_URI "#Upmix",
		.instantiate = intersect_instantiate,
		.connect_port = intersect_connect_port,
		.activate = intersect_activate,
		.run = upmix_run,
		.deactivate = intersect_deactivate,
		.cleanup = intersect_cleanup,
		.extension_data = NULL,
	};

	switch (i) {
		case 0:
			return &intersect_descriptor;

		case 1:
			return &symmetric_difference_descriptor;

		case 2:
			return &upmix_descriptor;

		default:
			return NULL;
	}
}

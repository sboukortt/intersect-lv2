#ifndef H_INTERSECT_FFT
#define H_INTERSECT_FFT

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

struct Intersect;

#if defined(INTERSECT_USE_PFFFT)
int intersect_pffft_snap_size(int size);
bool intersect_pffft_size_valid(int size);
#endif

float* intersect_alloc_real_buffer(int samples);
void intersect_free_real_buffer(float* buffer);

void intersect_fft_activate(Intersect* intersect);
void intersect_fft_deactivate(Intersect* intersect);
void intersect_fft_forward(Intersect* intersect);
void intersect_fft_inverse(Intersect* intersect);

#endif

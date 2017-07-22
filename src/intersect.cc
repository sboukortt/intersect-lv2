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

#include <algorithm>
#include <Eigen/Core>
#include <lvtk/plugin.hpp>
#include <unsupported/Eigen/FFT>

#define INTERSECT_URI "https://sami.boukortt.com/plugins/intersect"

enum {
	FFT_SIZE,
	OVERLAP_FACTOR,
	INPUT_CHANNEL_LEFT,
	INPUT_CHANNEL_RIGHT,
	LATENCY,
	OUTPUT_CHANNEL_LEFT,
	OUTPUT_CHANNEL_RIGHT,
	OUTPUT_CHANNEL_CENTER,
	NUM_PORTS
};

enum class Effect {
	Intersect,
	SymmetricDifference,
	Upmix,
};

enum {LEFT, RIGHT, CENTER};

template <Effect effect>
class IntersectBase: public lvtk::Plugin<IntersectBase<effect>> {
public:
	IntersectBase(double sample_rate): lvtk::Plugin<IntersectBase<effect>>(NUM_PORTS) {}

	void activate();

	void run(uint32_t sample_count);

private:
	float *input[2], *output[3];

	uint32_t fft_size, overlap_factor, fft_jump_size;
	float normalization_factor;

	/* deviation from the “stable state” where there are `fft_jump_size`
	   samples in `output_buffer` and `fft_size` samples in `input_buffer`

	   When `deviation` = `fft_jump_size`, the output buffer is empty and
	   the input buffers are full. Time to refill!
	 */
	uint32_t deviation;

	Eigen::MatrixX2f input_buffer;
	Eigen::MatrixX3f output_buffer;

	Eigen::FFT<float> fft;
};

static const auto _ = {
	IntersectBase<Effect::Upmix>::register_class(INTERSECT_URI "#Upmix"),
	IntersectBase<Effect::SymmetricDifference>::register_class(INTERSECT_URI "#SymmetricDifference"),
	IntersectBase<Effect::Intersect>::register_class(INTERSECT_URI "#Intersect"),
};

template <Effect effect>
void IntersectBase<effect>::activate() {
	input[LEFT]  = this->p(INPUT_CHANNEL_LEFT);
	input[RIGHT] = this->p(INPUT_CHANNEL_RIGHT);

	output[LEFT]   = this->p(OUTPUT_CHANNEL_LEFT);
	output[RIGHT]  = this->p(OUTPUT_CHANNEL_RIGHT);
	output[CENTER] = this->p(OUTPUT_CHANNEL_CENTER);

	fft_size = std::max<uint32_t>(1, *this->p(FFT_SIZE) + .5f);
	if (fft_size % 2 != 0) {
		++fft_size;
	}
	overlap_factor = std::clamp<uint32_t>(*this->p(OVERLAP_FACTOR) + .5f, 1, fft_size);
	fft_jump_size = fft_size / overlap_factor;
	normalization_factor = 1.f / (fft_size * overlap_factor);

	deviation = 0;

	 input_buffer = Eigen::MatrixX2f::Zero(fft_size, 2);
	output_buffer = Eigen::MatrixX3f::Zero(fft_size, 3);

	fft.SetFlag(Eigen::FFT<float>::Unscaled);
	fft.SetFlag(Eigen::FFT<float>::HalfSpectrum);
}

template <Effect effect>
void IntersectBase<effect>::run(uint32_t sample_count) {
	float *cursor_input [2] = {input [LEFT], input [RIGHT]},
	      *cursor_output[3] = {output[LEFT], output[RIGHT], output[CENTER]};

	while (sample_count > 0) {
		const uint32_t block_size = std::min(sample_count, fft_jump_size - deviation);

		Eigen::VectorXf center = output_buffer.col(CENTER).segment(deviation, block_size) * normalization_factor;

		switch (effect) {
			case Effect::Intersect:
				Eigen::Map<Eigen::VectorXf>(cursor_output[0], block_size) = center;
				break;

			case Effect::Upmix:
				Eigen::Map<Eigen::VectorXf>(cursor_output[CENTER], block_size) = center;

				/* falltrough */
			case Effect::SymmetricDifference: {
				for (int c = 0; c < 2; ++c) {
					Eigen::Map<Eigen::VectorXf>(cursor_output[c], block_size) = output_buffer.col(c).segment(deviation, block_size) - center;
				}
				break;
			}
		}

		for (int i = 0; i < 2; ++i) {
			input_buffer.col(i).segment(fft_size - fft_jump_size + deviation, block_size) = Eigen::Map<Eigen::ArrayXf>(cursor_input[i], block_size);
		}

		deviation += block_size;

		if (deviation == fft_jump_size) {
			output_buffer.col(CENTER).head(fft_size - fft_jump_size) = output_buffer.col(CENTER).tail(fft_size - fft_jump_size);
			output_buffer.col(CENTER).tail(fft_jump_size).setZero();

			Eigen::MatrixX2cf fft_result{fft_size / 2 + 1, 2};

			for (int i = 0; i < input_buffer.cols(); ++i) {
				fft_result.col(i) = fft.fwd(input_buffer.col(i));
			}

			Eigen::VectorXcf center_fft =
				(fft_result.array().col(LEFT).abs2() < fft_result.array().col(RIGHT).abs2())
				.select(
					fft_result.col(LEFT),
					fft_result.col(RIGHT)
				);

			Eigen::VectorXf center = fft.inv(center_fft);
			output_buffer.col(CENTER) += center;

			output_buffer.topLeftCorner(fft_jump_size, 2) = input_buffer.topRows(fft_jump_size);
			input_buffer.topRows(fft_size - fft_jump_size) = input_buffer.bottomRows(fft_size - fft_jump_size);

			deviation = 0;
		}

		cursor_input[LEFT]    += block_size;
		cursor_input[RIGHT]   += block_size;
		cursor_output[LEFT]   += block_size;
		cursor_output[RIGHT]  += block_size;
		cursor_output[CENTER] += block_size;
		sample_count          -= block_size;
	}

	if (this->p(LATENCY) != nullptr) {
		*this->p(LATENCY) = fft_size;
	}
}

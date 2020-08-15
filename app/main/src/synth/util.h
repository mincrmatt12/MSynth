#pragma once
// Utilities for synthesis

#include <math.h>
#include <stdint.h>

namespace ms::synth {
	inline float midi_to_freq(uint8_t midi_note) {
		return powf(2, static_cast<float>(midi_note - 69) / 12) * 440.f;
	}
}

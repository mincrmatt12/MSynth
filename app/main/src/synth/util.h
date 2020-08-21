#pragma once
// Utilities for synthesis

#include <math.h>
#include <stdint.h>

namespace ms::synth {
	inline float midi_to_freq(uint8_t midi_note) {
		return powf(2.f, static_cast<float>(midi_note - 69.f) / 12.f) * 440.f;
	}

	inline float offset_scale_from_semitones(float semitones) {
		return powf(2.f, (semitones / 24.f));
	}

	// sin, but it's scaled to 0-1 as a repeating wave.
	//
	// and also uses a lookup table for high speed
	float wavesin(float x);
}

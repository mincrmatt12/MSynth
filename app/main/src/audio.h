#pragma once
// Main audio management system:
// Keeps track of the soundgenerators and effectbus crap and sends it to the right place.
//
// Importantly, this doesn't do anything related to lifetime management, simply holds references to,
// collects, and outputs sound.

#include "synth/playback.h"
#include <cstddef>

namespace ms::audio {
	
	// Overloaded for various types of audio
	void add_source(synth::playback::AudioGenerator *generator);

	void stop();
	void start();

	void init();

	void set_volume(int16_t max_level);

	// DMA for audio should be routed here
	void dma_interrupt();
}

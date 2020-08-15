#pragma once
// Basic interfaces for synth audio generation.

#include <stdint.h>

namespace ms::synth::playback {
	struct AudioGenerator {
		virtual int16_t generate();
	};
}

#pragma once

#include <stdint.h>

// defined in the linker.
extern "C" uint8_t framebuffer_data[272][480];

namespace lcd {
	// Sets up the LTDC in **GRAYSCALE** mode, with normal framebuffer stuff
	//
	// For color, you must modify the required registers manually. (v-blank autofill must be implemented manually, for example)
	//
	// This also initializes the GPIO correctly.
	void init();
}

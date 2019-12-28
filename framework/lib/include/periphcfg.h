#pragma once
// periphcfg.h -- sets up peripherals in a standard-ish way
#include <stdint.h>

namespace periph {
	void setup_dbguart(); // USART3; default location for _write
	void setup_midiuart();
	void setup_ui();    // UI - buttonpad + led + knobs
	void setup_blpwm(); // BACKLIGHT

	// ACCESSOR FUNCS
	namespace bl {
		// Set backlight level
		void set(uint8_t level);
	}

	namespace ui {
		// Poll for buttonpresses
		
	}
}

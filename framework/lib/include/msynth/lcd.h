#pragma once

#include <stdint.h>

// defined in the linker.
extern "C" uint8_t framebuffer_data[272][480];

namespace lcd {
	// Sets up the LTDC in **QUARTER-DIRECT** mode, with normal framebuffer stuff
	//
	// The CLUT will be enabled, and the upper 64 values will be direct color (RGB222)
	// The rest of the CLUT is set to black
	//
	// This also initializes the GPIO correctly.
	//
	// Also sets up the SPI for the touch controller
	// NOTE: The touch controller signals it's readiness through an IRQ. That will trigger
	// an EXTI interrupt on line 1. To enable that, use init_touch_irq();
	void init();

	// Enables EXTI interrupt for the touch controller
	void init_touch_irq();

	// Poll the touch controller
	bool poll(uint16_t &xout, uint16_t &yout);
}

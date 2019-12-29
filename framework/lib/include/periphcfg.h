#pragma once
// periphcfg.h -- sets up peripherals in a standard-ish way
#include <stdint.h>

namespace periph {
	// Turn on the DEBUG uart. (@115200 baud)
	void setup_dbguart(); // USART3; default location for _write
	// Turn on the MIDI uart.
	void setup_midiuart();

	// Setup the GPIO/ADC for the UI
	void setup_ui();    // UI - buttonpad + led + knobs
	// Setup the GPIO/TIM for the backlight
	// This reserves TIM2
	void setup_blpwm(); // BACKLIGHT

	// ACCESSOR FUNCS
	namespace bl {
		// Set backlight level
		void set(uint8_t level);
	}

	namespace ui {
		enum struct led : int8_t {
			PATCH = 0,
			REC,
			PLAY,
			F1,
			F2,
			F3,
			F4,
			ALL = -1
		};

		enum struct knob : int8_t {
			VOLUME = 0,
			FX1,
			FX2
		};

		extern uint32_t buttons_pressed, buttons_held;

		// Real ordering:
		// N1 = 0,
		// N2,
		// N3,
		// BKSP,
		// N4,
		// N5,
		// N6,
		// N7,
		// N8,
		// N9,
		// N0,
		// ENTER,
		// PATCH,
		// REC,
		// PLAY,
		// UP,
		// DOWN,
		// LEFT,
		// RIGHT,
		// F4,
		// F3,
		// F2,
		// F1

		enum struct button : uint32_t {
			N1 = 0,
			N2 = 5,
			N3 = 10,
			BKSP = 15,
			N4 = 20,
			N5 = 1,
			N6 = 6,
			N7 = 11,
			N8 = 16,
			N9 = 21,
			N0 = 2,
			ENTER = 7,
			PATCH = 12,
			REC = 17,
			PLAY = 22,
			UP = 3,
			DOWN = 8,
			LEFT = 13,
			RIGHT = 18,
			F4 = 23,
			F3 = 4,
			F2 = 9,
			F1 = 14
		};

		// Set the state of an LED
		void set(led which, bool state);

		// Poll buttons
		void poll();

		// Get state
		uint16_t get(knob which);
		inline static bool get(button which) {
			return buttons_held & (1 << (uint32_t)which);
		}

		inline static bool pressed(button which) {
			return buttons_pressed & (1 << (uint32_t)which);
		}

	}
}

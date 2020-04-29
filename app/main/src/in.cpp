#include "in.h"
#include "evt/dispatch.h"
#include "evt/events.h"

#include <cstdio>
#include <msynth/lcd.h>
#include <msynth/sd.h>
#include <msynth/periphcfg.h>

MsUSB ms::in::usb_host;

void ms::in::init() {
	// Start periph UI
	periph::setup_ui();
	// Start MIDI
	periph::setup_midiuart();
	// Init USB
	usb_host.init();
	// Start SD controller and enable EXTi
	sd::init(true, true);
}

namespace ms::in {
	void poll_touch() {
		static bool fall_edge = false;
		static uint16_t raw_x, raw_y; // Yes I know this is silly, but a) this is a non-reentrant function and b) I need to save these anyways, may as well do this.

		// Try to poll the touchscreen
		auto is_pressed = lcd::poll(raw_x, raw_y);

		// TODO: apply correction coeffs
		
		if (raw_x < 1350) raw_x = 1450;
		raw_x -= 1350;
		raw_x /= ((30000 - 1350) / 480);

		if (raw_x > 480) raw_x = 480;

		if (raw_y < 2940) raw_y = 2940;
		if (raw_y > 29400) raw_y = 29400;

		raw_y = 29400 - raw_y;
		raw_y /= ((29400 - 2940) / 272);

		if (raw_y > 272) raw_y = 272;

		// TODO: get pressure
		int16_t pressure = is_pressed ? 255 : 0;

		if (!is_pressed && fall_edge) {
			// emit a touch released event
			evt::TouchEvent evt{static_cast<int16_t>(raw_x), static_cast<int16_t>(raw_y), evt::TouchEvent::PressureRemovedTouch};
			evt::dispatch(evt);
			fall_edge = false;
		}

		fall_edge = is_pressed;

		if (is_pressed) {
			evt::TouchEvent evt{static_cast<int16_t>(raw_x), static_cast<int16_t>(raw_y), pressure};
			evt::dispatch(evt);
		}
	}
}

void ms::in::poll() {
	poll_touch();
}



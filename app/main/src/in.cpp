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
			evt::TouchEvent evt{static_cast<int16_t>(raw_x), static_cast<int16_t>(raw_y), 0, evt::TouchEvent::StateReleased};
			evt::dispatch(evt);
			fall_edge = false;
		}

		if (is_pressed) {
			evt::TouchEvent evt{static_cast<int16_t>(raw_x), static_cast<int16_t>(raw_y), pressure, fall_edge ? evt::TouchEvent::StateDragged : evt::TouchEvent::StatePressed};
			evt::dispatch(evt);
		}

		fall_edge = is_pressed;
	}

	void poll_buttons() {
		periph::ui::poll();
		for (int i = 0; i < 24; ++i) {
			if (periph::ui::buttons_pressed & (1 << i)) evt::dispatch(evt::KeyEvent{true, static_cast<periph::ui::button>(i)});
			if (periph::ui::buttons_released & (1 << i)) evt::dispatch(evt::KeyEvent{false, static_cast<periph::ui::button>(i)});
		}
	}

	void on_midi_data(void *src, uint8_t * data, size_t length) {
		// TODO: check src

		// TODO: actually delegate this and parse messages properly. perhaps nmfu might
		// be useful here?


	}

	void poll_usb() {
		static bool was_connected = false;
		// Check for connection
		if (!was_connected && usb_host.inserted()) {
			// Try to initialize it
			auto result = usb_host.init_periph();
			switch (result) {
				case usb::init_status::Ok:
					break;
				case usb::init_status::NotSupported:
				case usb::init_status::TxnErrorDuringEnumeration:
				case usb::init_status::Timeout:
				case usb::init_status::DeviceInitError:
					{
						printf("usb init err %d\n", result);
						// Fire off an event
						evt::dispatch(evt::HardwareFaultEvent{
							result == usb::init_status::NotSupported ? evt::HardwareFaultEvent::FaultUsbUnsupported : evt::HardwareFaultEvent::FaultUsbEnumerationError
						});
					}
				case usb::init_status::NotInserted:
					return;
			}
			// Get the device info
			usb::helper::DeviceInfo di = usb_host.info();
			// Initialization was OK, check which type of device it is
			if (usb_host.inserted<usb::MidiDevice>()) {
				// Fire off a devicestate event
				evt::DeviceStateEvent ev;
				ev.name = di.product_name;
				ev.vendor = di.vendor_name;
				ev.source = evt::DeviceStateEvent::SourceUSB;
				ev.target = evt::DeviceStateEvent::InputMIDI;
				evt::dispatch(ev);
				// Connect midi device callback
				usb_host.dev<usb::MidiDevice>().set_callback(usb::MidiDevice::EventCallback::create(on_midi_data));
			}
			else {
				// TODO
				puts("usb todo");
			}

			was_connected = true;
		}
		// Check for disconnection
		if (was_connected && !usb_host.inserted()) {
			// TODO
			was_connected = false;
			return;
		}
			
		if (!was_connected) return;
		if (usb_host.inserted<usb::MidiDevice>()) {
			// update midi data
			usb_host.dev<usb::MidiDevice>().update();
		}
	}
}

void ms::in::poll() {
	poll_touch();
	poll_buttons();
}



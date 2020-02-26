#include "usb.h"
#include <msynth/draw.h>
#include <msynth/fs.h>

extern usb::Host<usb::StaticStateHolder, usb::MidiDevice> usb_host;

void UsbTest::start() {
	uiFnt  = fs::open("fonts/djv_16.fnt");
	bigFnt = fs::open("fonts/lato_32.fnt");

	state = WaitingForInserted;
	usb_host.enable(); // cause major mayhem
}


TestState UsbTest::loop() {
	// Do display loop
	if (last_state != state) {
		switch (state) {
			case WaitingForInserted:
				draw::fill(0b11'11'00'00);
				draw::text(30, 100, "WaitingForInserted", bigFnt, 0xff);
				break;
			case DetectingPeripheral:
				draw::fill(0b11'00'11'00);
				draw::text(30, 100, "DetectingPeripheral", bigFnt, 0xff);
				break;
			case Ready:
				draw::fill(0b11'00'11'11);
				// TODO
				break;
			case UndefinedState:
				break;
		}

		last_state = state;
	}

	switch (state) {
		case WaitingForInserted:
			{
				if (usb_host.inserted()) {
					// Begin init
					state = DetectingPeripheral;
				}
			}
			break;
		case DetectingPeripheral:
			{
				init_error = usb_host.init_periph();
				if (init_error == usb::init_status::Ok) {
					state = Ready;
				}
				else {
					draw::fill(0b11'11'00'00);
					uint16_t cursor = draw::text(30, 200, "Fail: ", bigFnt, 0xff);
					switch (init_error) {
						case usb::init_status::NotInserted:
							draw::text(cursor, 200, "NotInserted", uiFnt, 0xfc);
							break;
						case usb::init_status::NotSupported:
							draw::text(cursor, 200, "NotSupported", uiFnt, 0xfc);
							break;
						default:
							draw::text(cursor, 200, "?? invalid retcode", uiFnt, 0xfc);
							break;
					}

					util::delay(2000);
					
					// disable usb

					usb_host.disable();
					return Fail;
				}
			}
		case Ready:
			break;
		case UndefinedState:
			break;
	}

	return InProgress;
}

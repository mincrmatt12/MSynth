#include "sd.h"
#include <msynth/draw.h>
#include <msynth/fs.h>
#include <msynth/periphcfg.h>

void SdTest::start() {
	state = (sd::inserted() ? WaitingForStartInserted : WaitingForStartEjected);
	uiFnt  = fs::open("fonts/djv_16.fnt");
	bigFnt = fs::open("fonts/lato_32.fnt");
}

TestState SdTest::loop() {
	if (last_state != state) {
		last_state = state;

		switch (state) {
			case WaitingForStartInserted:
				draw::fill(0b11'00'00'11);
				draw::text(20, 120, "WaitingForStartInserted", bigFnt, 0b11'11'11'11);
				draw::text(90, 140, "press ENTER", uiFnt, 0b11'10'10'10);
				break;
			case WaitingForStartEjected:
				draw::fill(0b11'11'00'00);
				draw::text(20, 120, "WaitingForStartEjected", bigFnt, 0b11'11'11'11);
				draw::text(90, 140, "press ENTER", uiFnt, 0b11'10'10'10);
				break;
			default:
				break;
		}
	}

	periph::ui::poll();

	switch (state) {
		case WaitingForStartInserted:
		case WaitingForStartEjected:
			state = sd::inserted() ? WaitingForStartInserted : WaitingForStartEjected;
			if (periph::ui::pressed(periph::ui::button::ENTER)) state = RunningSdInit;
			return InProgress;
		case RunningSdInit:
			if (sd_init_error != sd::init_status::Ok) state = ShowingInitError;
			else state = WaitingForActionSelection;
			return InProgress;
		default:
			return Fail;
	}
}

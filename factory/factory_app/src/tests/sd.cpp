#include "sd.h"
#include <msynth/draw.h>
#include <msynth/fs.h>
#include <msynth/periphcfg.h>
#include <msynth/util.h>

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
				draw::text(80, 120, "WaitingForStartInserted", bigFnt, 0b11'11'11'11);
				draw::text(100, 140, "press ENTER", uiFnt, 0b11'10'10'10);
				break;
			case WaitingForStartEjected:
				draw::fill(0b11'11'00'00);
				draw::text(80, 120, "WaitingForStartEjected", bigFnt, 0b11'11'11'11);
				draw::text(100, 140, "press ENTER", uiFnt, 0b11'10'10'10);
				break;
			case RunningSdInit:
				draw::fill(0b11'01'01'11);
				draw::text(80, 120, "RunningSdInit", bigFnt, 0b11'11'11'11);
				sd_init_error = sd::init_card();
				break;
			case ShowingInitError:
				{
					draw::fill(0b11'11'01'01);
					uint16_t cursor = draw::text(50, 120, "ERR: ", bigFnt, 0b11'11'11'10);
					switch (sd_init_error) {
						case sd::init_status::CardUnusable:
							draw::text(cursor, 120, "CardUnusable", bigFnt, 0xff);
							break;
						case sd::init_status::CardNotInserted:
							draw::text(cursor, 120, "CardNotInserted", bigFnt, 0xff);
							break;
						case sd::init_status::MultipleCardsDetected:
							draw::text(cursor, 120, "MultipleCardsDetected", bigFnt, 0xff);
							break;
						case sd::init_status::CardNotResponding:
							draw::text(cursor, 120, "CardNotResponding", bigFnt, 0xff);
							break;
						case sd::init_status::CardIsSuperGluedShut:
							draw::text(cursor, 120, "CardIsSuperGluedShut", bigFnt, 0xff);
							break;
						case sd::init_status::CardIsElmersGluedShut:
							draw::text(cursor, 120, "CardIsElmersGluedShut", bigFnt, 0xff);
							break;
						case sd::init_status::NotInitialized:
							draw::text(cursor, 120, "NotInitialized", bigFnt, 0xff);
							break;
						case sd::init_status::InternalPeripheralError:
							draw::text(cursor, 120, "InternalPeripheralError", bigFnt, 0xff);
							break;
						case sd::init_status::NotSupported:
							draw::text(cursor, 120, "NotSupported", bigFnt, 0xff);
							break;
						default:
							break;
					}
					util::delay(1000);
				}
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

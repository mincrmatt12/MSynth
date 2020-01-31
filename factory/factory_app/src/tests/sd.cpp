#include "sd.h"
#include <msynth/draw.h>
#include <msynth/fs.h>
#include <msynth/periphcfg.h>
#include <msynth/util.h>

uint8_t sector_dump[512];

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
			case ResettingCardAndExiting:
				draw::fill(0b11'01'01'11);
				draw::text(70, 120, "ResettingCardAndExiting", bigFnt, 0b11'11'11'11);
				sd::reset();
				return Ok;
			case ReadingDataFromCardForDump:
				draw::fill(0b11'01'01'11);
				draw::text(20, 120, "ReadingDataFromCardForDump", bigFnt, 0b11'11'11'11);
				sd_access_error = sd::read(8192, sector_dump, 1);
				break;
			case WaitingForActionSelection:
				draw::fill(0);
				{
					char buf[64] = {0}; snprintf(buf, 64, "size: %u KiB; est %d GB", (uint32_t)(sd::card.length / 1024), (int)(sd::card.length / (1000*1000*1000)));
					draw::text(draw::text(30, 60, "SD Init OK!", bigFnt, 0xff), 60, buf, uiFnt, 0b11'10'10'10);
				}
				draw::text(120, 120, "1 - erase/read/write", uiFnt, 0xff);
				draw::text(120, 152, "2 - read sector 8192", uiFnt, 0xff);
				draw::text(120, 184, "3 - exit", uiFnt, 0xff);
				break;
			case ShowingDumpOnScreen:
				draw::fill(0);
				// Header
				draw::text(2, 14, "Sector 8192 dump ([ENTER] to return to menu)", uiFnt, 0xff);
				{
					char buf[8] = {0};
					int x = 2;
					int y = 30;
					for (int pos = 0; pos < 512; ++pos) {
						snprintf(buf, 8, "%02x", sector_dump[pos]);
						x = draw::text(x, y, buf, uiFnt, 0xff) + 10;
						if (x > 460) {y += 16; x = 2;}
						if (y > 270) break;
					}
				}
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
			case ShowingAccessError:
				{
					draw::fill(0b11'11'01'01);
					uint16_t cursor = draw::text(50, 120, "ERR: ", bigFnt, 0b11'11'11'10);
					switch (sd_access_error) {
						case sd::access_status::DMATransferError:
							draw::text(cursor, 120, "DMATransferError", bigFnt, 0xff);
							break;
						case sd::access_status::CardLockedError:
							draw::text(cursor, 120, "CardNotInserted", bigFnt, 0xff);
							break;
						case sd::access_status::CardNotInserted:
							draw::text(cursor, 120, "CardNotInserted", bigFnt, 0xff);
							break;
						case sd::access_status::CardNotResponding:
							draw::text(cursor, 120, "CardNotResponding", bigFnt, 0xff);
							break;
						case sd::access_status::CardWillForeverMoreBeStuckInAnEndlessWaltzSendingData:
							draw::text(cursor, 120, "CardWillForeverMoreBeStuckInAnEndlessWaltzSendingData", uiFnt, 0xff);
							break;
						case sd::access_status::CRCError:
							draw::text(cursor, 120, "CRCError", bigFnt, 0xff);
							break;
						case sd::access_status::NotInitialized:
							draw::text(cursor, 120, "NotInitialized", bigFnt, 0xff);
							break;
						default:
							break;
					}
					util::delay(2000);
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
		case WaitingForActionSelection:
			if (periph::ui::buttons_pressed) {
				if (periph::ui::pressed(periph::ui::button::N3)) state = ResettingCardAndExiting;
				if (periph::ui::pressed(periph::ui::button::N2)) state = ReadingDataFromCardForDump;
			}
			return InProgress;
		case ReadingDataFromCardForDump:
			if (sd_access_error != sd::access_status::Ok) state = ShowingAccessError;
			else state = ShowingDumpOnScreen;
		case ShowingDumpOnScreen:
			if (periph::ui::pressed(periph::ui::button::ENTER)) state = WaitingForActionSelection;
			return InProgress;
		default:
			return Fail;
	}
}

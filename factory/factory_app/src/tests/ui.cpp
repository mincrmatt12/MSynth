#include "ui.h"

#include <msynth/draw.h>
#include <msynth/fs.h>
#include <msynth/periphcfg.h>

#include <string.h>
#include <stm32f4xx.h>

void UiTest::start() {
	uiFnt = fs::open("fonts/djv_16.fnt");
	state = Button;

	draw::fill(0b11'11'00'00); // solid blue
	draw::text(200, 100, "Last button: ", uiFnt, 255);

	last_button = 0;
}

int popcount(uint32_t x) {
	int c = 0;
    for (; x != 0; x &= x - 1)
        c++;
    return c;
}

TestState UiTest::loop() {
	switch (state) {
		case Button:
			{
				// If button was pressed?
				periph::ui::poll();

				if (periph::ui::buttons_pressed) {
					// 0 is for the skipped over ones due to internal ordering
					if (popcount(last_button) == 23) {
						state = Led;

						draw::fill(0b11'00'00'11); // solid blue
						draw::text(100, 100, "LED (button to select, enter to continue)", uiFnt, 255);

						last_led = periph::ui::led::F1;

						return InProgress;
					}

					last_button |= periph::ui::buttons_pressed;
					char name[12] = {0};

					if      (periph::ui::pressed(periph::ui::button::N0))    strncpy(name, "N0", 12);
					else if (periph::ui::pressed(periph::ui::button::N1))    strncpy(name, "N1", 12);
					else if (periph::ui::pressed(periph::ui::button::N2))    strncpy(name, "N2", 12);
					else if (periph::ui::pressed(periph::ui::button::N3))    strncpy(name, "N3", 12);
					else if (periph::ui::pressed(periph::ui::button::N4))    strncpy(name, "N4", 12);
					else if (periph::ui::pressed(periph::ui::button::N5))    strncpy(name, "N5", 12);
					else if (periph::ui::pressed(periph::ui::button::N6))    strncpy(name, "N6", 12);
					else if (periph::ui::pressed(periph::ui::button::N7))    strncpy(name, "N7", 12);
					else if (periph::ui::pressed(periph::ui::button::N8))    strncpy(name, "N8", 12);
					else if (periph::ui::pressed(periph::ui::button::N9))    strncpy(name, "N9", 12);
					else if (periph::ui::pressed(periph::ui::button::BKSP))  strncpy(name, "BKSP", 12);
					else if (periph::ui::pressed(periph::ui::button::ENTER)) strncpy(name, "ENTER", 12);
					else if (periph::ui::pressed(periph::ui::button::PATCH)) strncpy(name, "PATCH", 12);
					else if (periph::ui::pressed(periph::ui::button::REC))   strncpy(name, "REC", 12);
					else if (periph::ui::pressed(periph::ui::button::PLAY))  strncpy(name, "PLAY", 12);
					else if (periph::ui::pressed(periph::ui::button::F1))    strncpy(name, "F1", 12);
					else if (periph::ui::pressed(periph::ui::button::F2))    strncpy(name, "F2", 12);
					else if (periph::ui::pressed(periph::ui::button::F3))    strncpy(name, "F3", 12);
					else if (periph::ui::pressed(periph::ui::button::F4))    strncpy(name, "F4", 12);
					else if (periph::ui::pressed(periph::ui::button::UP))    strncpy(name, "UP", 12);
					else if (periph::ui::pressed(periph::ui::button::LEFT))  strncpy(name, "LEFT", 12);
					else if (periph::ui::pressed(periph::ui::button::RIGHT)) strncpy(name, "RIGHT", 12);
					else if (periph::ui::pressed(periph::ui::button::DOWN))  strncpy(name, "DOWN", 12);

					draw::rect(0, 101, 480, 128, 0);
					draw::text(100, 116, name, uiFnt, 0xff);

				}
				return InProgress;
			}
		case Led:
			{
				// Set current LED
				periph::ui::set(periph::ui::led::ALL, false);
				periph::ui::set(last_led, true);

				// Get buttons
				periph::ui::poll();

				if (periph::ui::buttons_pressed) {
					const char * name;

					if (periph::ui::pressed(periph::ui::button::PATCH)) {
						last_led = periph::ui::led::PATCH;
						name = "PATCH";
					}
					else if (periph::ui::pressed(periph::ui::button::REC)) {
						last_led = periph::ui::led::REC;
						name = "REC";
					}
					else if (periph::ui::pressed(periph::ui::button::PLAY)) {
						last_led = periph::ui::led::PLAY;
						name = "PLAY";
					}
					else if (periph::ui::pressed(periph::ui::button::F1)) {
						last_led = periph::ui::led::F1;
						name = "F1";
					}
					else if (periph::ui::pressed(periph::ui::button::F2)) {
						last_led = periph::ui::led::F2;
						name = "F2";
					}
					else if (periph::ui::pressed(periph::ui::button::F3)) {
						last_led = periph::ui::led::F3;
						name = "F1";
					}
					else if (periph::ui::pressed(periph::ui::button::F4)) {
						last_led = periph::ui::led::F4;
						name = "F1";
					}
					else if (periph::ui::pressed(periph::ui::button::ENTER)) {
						periph::ui::set(periph::ui::led::ALL, false);

						// Set text
						draw::fill(0b11'11'00'00); // solid blue
						draw::text(200, 100, "Knob (UP to exit)", uiFnt, 255);
						// exit
						state = Knob;
						return InProgress;
					}

					// Show screen
					draw::rect(0, 101, 480, 128, 0);
					draw::text(100, 116, name, uiFnt, 0xff);
				}

				return InProgress;
			}
			break;
		case Knob:
			{
				// Sample all knobs
				uint16_t fx1 = periph::ui::get(periph::ui::knob::FX1), 
						 fx2 = periph::ui::get(periph::ui::knob::FX2), 
						 vol = periph::ui::get(periph::ui::knob::VOLUME);
				
				// Construct string
				char buf[48] = {0};
				snprintf(buf, 48, "fx1: %d, fx2: %d, vol: %d", fx1, fx2, vol);

				while ((LTDC->CPSR & 0xFFFF) < 240) {;}
				draw::rect(0, 101, 480, 128, 0);
				draw::text(90, 116, buf, uiFnt, 0xff);

				periph::ui::poll();

				// IF check
				if (periph::ui::pressed(periph::ui::button::UP)) {
					// done
					return Ok;
				}

				return InProgress;
			}
			break;
	}

	return Ok;
}

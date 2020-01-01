#include "ui.h"

#include <msynth/draw.h>
#include <msynth/fs.h>
#include <msynth/periphcfg.h>

#include <string.h>

void UiTest::start() {
	uiFnt = fs::open("fonts/djv_16.fnt");
	state = Button;

	draw::fill(0b11'11'00'00); // solid blue
	draw::text(200, 100, "Last button: ", uiFnt, 255);

	last_button = 0;
}

TestState UiTest::loop() {
	switch (state) {
		case Button:
			{
				// If button was pressed?
				periph::ui::poll();

				if (periph::ui::buttons_pressed) {
					if (last_button == (1ull << 23) - 1) {
						state = Led;

						draw::fill(0b11'00'01'10); // solid blue
						draw::text(200, 100, "LED (press to continue)", uiFnt, 255);

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
		case Knob:
			break;
	}

	return Ok;
}

#include "lcd.h"

#include <msynth/draw.h>
#include <msynth/periphcfg.h>

#include <string.h>
#include <stm32f4xx.h>

void LcdTest::start() {
}

TestState LcdTest::loop() {
	periph::ui::poll();

	if (periph::ui::pressed(periph::ui::button::N0)) screenno = 0;
	if (periph::ui::pressed(periph::ui::button::N1)) screenno = 1;
	if (periph::ui::pressed(periph::ui::button::N2)) screenno = 2;
	if (periph::ui::pressed(periph::ui::button::N3)) screenno = 3;
	if (periph::ui::pressed(periph::ui::button::N4)) screenno = 4;
	if (periph::ui::pressed(periph::ui::button::N5)) screenno = 5;
	if (periph::ui::pressed(periph::ui::button::ENTER)) return Ok;

	int color = 0;

	switch (screenno) {
		case 0:
			draw::fill(0);
			break;
		case 1:
			draw::fill(0b11'11'00'00);
			break;
		case 2:
			draw::fill(0b11'00'11'00);
			break;
		case 3:
			draw::fill(0b11'00'00'11);
			break;
		case 4:
			for (int x = 0; x < 480; x += 16) {
				for (int y = 0; y < 272; y += 16) {
					draw::rect(x, y, x + 16, y + 16, 0b11'000000 | color);
					++color; color &= 0b00'111111;
				}
			}
			break;
		case 5:
			for (int x = 0; x < 480; ++x) {
				draw::rect(x, 0, x + 1, 272, 0b11'000000 | color);
				++color; color &= 0b00'111111;
			}
			break;
	}

	return InProgress;
}


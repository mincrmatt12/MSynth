#include <msynth/lcd.h>
#include <msynth/periphcfg.h>
#include <stdio.h>
#include <msynth/fs.h>
#include <msynth/util.h>
#include <msynth/draw.h>
#include <stm32f4xx.h>

// Tests
#include "tests/base.h"
#include "tests/ui.h"

int main() {
	periph::setup_dbguart();
	puts("Starting FACTORY");
	periph::setup_blpwm();
	periph::setup_ui();
	puts("Starting LCD");
	lcd::init();
	periph::bl::set(255);

	const void * insnFnt = fs::open("fonts/lato_32.fnt");

	enum {
		Menu,
		Ui
	} state;

	UiTest ui_test;

drawMenu:
	draw::fill(0);
	draw::text(16, 38, "Test menu:", insnFnt, 255);
	draw::text(16, 38 + 32, " 1 - UI", insnFnt, 255);

	while (1) {
		TestState ts = InProgress;

		switch (state) {
			case Menu:
				{
					periph::ui::poll();
					if (periph::ui::buttons_pressed) {
						if (periph::ui::pressed(periph::ui::button::N1)) {
							state = Ui;
							ui_test.start();
						}
					}
				}
				break;
			case Ui:
				ts = ui_test.loop();
				break;
		}

		if (ts == Ok) {
			draw::fill(0b11'00'00'11);
			draw::text(210, 100, "OK", insnFnt, 255);

			for (int i = 0; i < 100000000; ++i) asm volatile("nop");
			state = Menu;
			goto drawMenu;
		}
		else if (ts == Fail) {
			// todo
			goto drawMenu;
		}
	}


	return 0;
}

#include <msynth/lcd.h>
#include <msynth/periphcfg.h>
#include <msynth/fs.h>
#include <msynth/util.h>
#include <msynth/draw.h>
#include <msynth/sound.h>
#include <msynth/sd.h>
#include <stm32f4xx.h>
#include <stdio.h>

// Tests
#include "tests/base.h"
#include "tests/ui.h"
#include "tests/audio.h"
#include "tests/lcd.h"
#include "tests/sd.h"

int main() {
	periph::setup_dbguart();
	puts("Starting FACTORY");
	periph::setup_blpwm();
	periph::setup_ui();
	puts("Starting LCD");
	lcd::init();
	periph::bl::set(255);
	puts("Starting AUDIO");
	sound::init();
	puts("Starting SD");
	sd::init(false); // disable EXTi

	const void * insnFnt = fs::open("fonts/lato_32.fnt");

	enum {
		Menu,
		Ui,
		Audio,
		LCD,
		SD
	} state;

	UiTest ui_test;
	AudioTest audio_test;
	LcdTest lcd_test;
	SdTest sd_test;

drawMenu:
	draw::fill(0);
	draw::text(16, 38, "Test menu:", insnFnt, 255);
	draw::text(16, 38 + 32, " 1 - UI", insnFnt, 255);
	draw::text(16, 38 + 64, " 2 - Audio", insnFnt, 255);
	draw::text(16, 38 + 64 + 32, " 3 - LCD", insnFnt, 255);
	draw::text(16, 38 + 128, " 4 - SD", insnFnt, 255);

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
						else if (periph::ui::pressed(periph::ui::button::N2)) {
							state = Audio;
							audio_test.start();
						}
						else if (periph::ui::pressed(periph::ui::button::N3)) {
							state = LCD;
							lcd_test.start();
						}
						else if (periph::ui::pressed(periph::ui::button::N4)) {
							state = SD;
							sd_test.start();
						}
					}
				}
				break;
			case Ui:
				ts = ui_test.loop();
				break;
			case Audio:
				ts = audio_test.loop();
				break;
			case LCD:
				ts = lcd_test.loop();
				break;
			case SD:
				ts = sd_test.loop();
				break;
		}

		if (ts == Ok) {
			draw::fill(0b11'00'00'11);
			draw::text(210, 150, "OK", insnFnt, 0);

			util::delay(1500);
			state = Menu;
			goto drawMenu;
		}
		else if (ts == Fail) {
			draw::fill(0b11'11'00'00);
			draw::text(210, 150, "Fail", insnFnt, 0xff);

			util::delay(1500);
			state = Menu;
			goto drawMenu;
		}
	}


	return 0;
}

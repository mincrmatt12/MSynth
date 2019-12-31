#include <msynth/lcd.h>
#include <msynth/periphcfg.h>
#include <stdio.h>
#include <msynth/fs.h>
#include <msynth/util.h>
#include <msynth/draw.h>

int main() {
	periph::setup_dbguart();
	periph::setup_blpwm();
	periph::setup_ui();
	puts("Starting FACTORY");
	puts("Starting LCD");
	lcd::init();
	periph::bl::set(100);
	const void * font = fs::open("fonts/lato_32.fnt");

	// Write some stuff to the framebuffer
	puts("looping");

	// Write test text
	draw::text(5, 60, "To! Kern! AVo.", font, 255);
	for (int i = 0; i < 256; ++i) {
		periph::bl::set(i);
		for (int j = 0; j < 150000; ++j) {asm volatile("nop");}
	}

	util::LowPassFilter<17, 20> adc;

	while (1) {
		periph::ui::poll();
		int value = adc(periph::ui::get(periph::ui::knob::FX1));

		periph::bl::set(value >> 4);

		printf("vol = %d; press %08x; ", value, periph::ui::buttons_pressed);
		if      (periph::ui::get(periph::ui::button::N0))    puts("N0");
		else if (periph::ui::get(periph::ui::button::N1))    puts("N1");
		else if (periph::ui::get(periph::ui::button::N2))    puts("N2");
		else if (periph::ui::get(periph::ui::button::N3))    puts("N3");
		else if (periph::ui::get(periph::ui::button::N4))    puts("N4");
		else if (periph::ui::get(periph::ui::button::N5))    puts("N5");
		else if (periph::ui::get(periph::ui::button::N6))    puts("N6");
		else if (periph::ui::get(periph::ui::button::N7))    puts("N7");
		else if (periph::ui::get(periph::ui::button::N8))    puts("N8");
		else if (periph::ui::get(periph::ui::button::N9))    puts("N9");
		else if (periph::ui::get(periph::ui::button::BKSP))  puts("BKSP");
		else if (periph::ui::get(periph::ui::button::ENTER)) puts("ENTER");
		else if (periph::ui::get(periph::ui::button::PATCH)) puts("PATCH");
		else if (periph::ui::get(periph::ui::button::REC))   puts("REC");
		else if (periph::ui::get(periph::ui::button::PLAY))  puts("PLAY");
		else if (periph::ui::get(periph::ui::button::F1))    puts("F1");
		else if (periph::ui::get(periph::ui::button::F2))    puts("F2");
		else if (periph::ui::get(periph::ui::button::F3))    puts("F3");
		else if (periph::ui::get(periph::ui::button::F4))    puts("F4");
		else if (periph::ui::get(periph::ui::button::UP))    puts("UP");
		else if (periph::ui::get(periph::ui::button::LEFT))  puts("LEFT");
		else if (periph::ui::get(periph::ui::button::RIGHT)) puts("RIGHT");
		else if (periph::ui::get(periph::ui::button::DOWN))  puts("DOWN");
		else puts("");

		uint16_t x, y;
		if (lcd::poll(x, y)) {
			// scale x
			if (x < 1800)  continue;
			x -= 1800;
			x /= 59;
			if (x > 480) continue;

			// scale y
			if (y < 3200) continue;
			y -= 3200;
			y /= 100;

			if (y > 272) continue;
			else y = 272 - y;

			printf("touch: x %d, y %d\n", x, y);

			for (int x0 = x - 2; x0 < x + 2; ++x0) {
				for (int y0 = y - 2; y0 < y + 2; ++y0) {
					framebuffer_data[y0][x0] = 0xff;
				}
			}
		}
	}


	return 0;
}

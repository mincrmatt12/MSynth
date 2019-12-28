#include <lcd.h>
#include <periphcfg.h>
#include <stdio.h>

int main() {
	periph::setup_dbguart();
	periph::setup_blpwm();
	puts("Starting FACTORY");
	puts("Starting LCD");
	lcd::init();
	periph::bl::set(100);

	// Write some stuff to the framebuffer
	puts("looping");

	for (int x = 0; x < 480; ++x) {
		for (int y = 0; y < 272; ++y) {
			framebuffer_data[y][x] = (x+y) % 256;
		}
	}
	for (int i = 0; i < 256; ++i) {
		periph::bl::set(i);
		for (int j = 0; j < 150000; ++j) {asm volatile("nop");}
	}

	while (1) {;}

	return 0;
}

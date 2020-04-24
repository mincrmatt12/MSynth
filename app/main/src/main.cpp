#include "in.h"
#include "irq.h"

#include <msynth/periphcfg.h>
#include <msynth/draw.h>
#include <msynth/lcd.h>

#include <stdio.h>

int main() {
	// Setup debug UART
	periph::setup_dbguart();

	puts("Starting MSynth Main");

	// Initialize some stuff.
	ms::irq::init();
	ms::in::init();

	// Start the display
	lcd::init();

	while (1) {;}

	return 0;
}

#include "in.h"

#include <msynth/periphcfg.h>

MsUSB ms::in::usb_host;

void ms::in::init() {
	// Start periph UI
	periph::setup_ui();
	// Start MIDI
	periph::setup_midiuart();
	// Init USB
	usb_host.init();
}



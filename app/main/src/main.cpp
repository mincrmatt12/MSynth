#include "in.h"
#include "irq.h"
#include "audio.h"

#include <stdio.h>

#include <msynth/fs.h>
#include <msynth/periphcfg.h>
#include <msynth/draw.h>
#include <msynth/lcd.h>
#include "synth/module.h"

#include "ui/mgr.h"

int main() {
	// Setup debug UART
	periph::setup_dbguart();

	puts("");
	puts("Starting MSynth Main");
	puts("--------------------");
	puts("Version: 0.0");

	// Make sure the filesystem was flashed correctly
	if (!fs::is_present()) {
		puts("filesystem is missing; startup cannot continue.");
		return MSYNTH_APPCODE_MISSINGRSRC;
	}
	else {
		if (!fs::exists("fonts/djv_16.fnt")) {
			puts("no uifnt present.");
			return MSYNTH_APPCODE_MISSINGRSRC;
		}
	}

	// Load the UI font (for rendering loading status)
	const void * ui_font = fs::open("fonts/djv_16.fnt");
	auto status = [&ui_font](const char * v){
		draw::rect(80, 195, 340, 220, 0);
		auto x = 240 - draw::text_size(v, ui_font) / 2;
		draw::text(x, 212, v, ui_font, 0xea);
	};

	// Start the display
	lcd::init();
	periph::setup_blpwm();
	periph::bl::set(100);

	// Show a splash screen while we init
	{
		if (!fs::exists("ui/logo.img")) {
			puts("missing logo");
			draw::text(180, 130, "(no logo present)", ui_font, 0xff);
		}
		else draw::blit(190, 111, 100, 50, reinterpret_cast<const uint8_t *>(fs::open("ui/logo.img")));
	}

	status("Starting...");

	// Initialize some stuff.
	ms::irq::init();
	status("Initializing input...");
	ms::in::init();
	status("Initializing sound...");
	ms::audio::init();
	status("Setting up UI...");
	util::delay(100); // TODO: remove me when there's more loading tasks... lol
	ms::ui::ui_16_font = ui_font; // set ui font
	status("Loading settings...");

	// TODO: read previous UI state / serialize UI state and start the correct UI
	// TODO: check if we need to start the SD card
	
	status("Opening previous file...");
	
	// For now just open the play one.
	
	while (1) {
		util::delay(1);
		ms::in::poll();
		ms::ui::mgr::draw();
	}

	return 0;
}

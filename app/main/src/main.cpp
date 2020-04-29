#include "in.h"
#include "irq.h"

#include <stdio.h>

#include <msynth/fs.h>
#include <msynth/periphcfg.h>
#include <msynth/draw.h>
#include <msynth/lcd.h>

#include "ui/base.h"
#include "ui/el/button.h"
#include "evt/dispatch.h"
#include "evt/events.h"

// TEST UI

struct TestButtonUI : ms::ui::UI, ms::evt::EventHandler<ms::evt::TouchEvent> {
	// Callbacks
	void cb1() {puts("cb1");}
	void cb2() {puts("cb2");}
	void cb3() {puts("cb3");}
	
	// Buttons
	ms::ui::el::Button button1, button2, button3;

	constexpr static auto layout = ms::ui::l::make_layout(&TestButtonUI::button1, ms::ui::Box(100, 100, 90, 25), 
														  &TestButtonUI::button2, ms::ui::Box(100, 130, 90, 25),
														  &TestButtonUI::button3, ms::ui::Box(200, 100, 160, 30));

	TestButtonUI() :
		button1("Button 1", ms::ui::el::Button::Callback::create(*this, &TestButtonUI::cb1)), 
		button2("Button 2", ms::ui::el::Button::Callback::create(*this, &TestButtonUI::cb2)),
		button3("Big button 3", ms::ui::el::Button::Callback::create(*this, &TestButtonUI::cb3))
	{
		// Other init stuff etc.
	}

	void draw() override {
		// Tell the LayoutManager to draw to the entire screen
		layout.redraw(*this);
	}

	void handle(const ms::evt::TouchEvent& evt) override {
		layout.handle(*this, evt);
	}
};

int main() {
	// Setup debug UART
	periph::setup_dbguart();

	puts("");
	puts("Starting MSynth Main");
	puts("--------------------");
	puts("Version: 0.1");

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
	status("Setting up UI...");
	util::delay(100); // TODO: remove me when there's more loading tasks... lol
	ms::ui::ui_16_font = ui_font; // set ui font

	// Clear the display in preparation for the UI
	draw::fill(0x00);

	// TODO: read previous UI state / serialize UI state and start the correct UI
	// TODO: check if we need to start the SD card

	TestButtonUI ui;

	ms::evt::add(&ui);
	
	while (1) {
		util::delay(1);
		ms::in::poll();
		ui.draw();
	}

	return 0;
}

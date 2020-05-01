#include "in.h"
#include "irq.h"

#include <stdio.h>

#include <msynth/fs.h>
#include <msynth/periphcfg.h>
#include <msynth/draw.h>
#include <msynth/lcd.h>

#include "ui/base.h"
#include "ui/el/button.h"
#include "ui/el/text.h"
#include "evt/dispatch.h"
#include "evt/events.h"

// TEST UI

struct TestButtonUI : ms::ui::UI, ms::evt::EventHandler<ms::evt::TouchEvent> {
	// Callbacks
	void cb1() {puts("cb1");}
	void cb2() {puts("cb2");}
	void cb3() {puts("cb3");}
	
	// Buttons
	ms::ui::el::Button button1, button2, button3, button4;

	// Labels
	ms::ui::el::Text label_counter; 

	constexpr static auto layout = ms::ui::l::make_layout(&TestButtonUI::button1, ms::ui::Box(100, 100, 90, 25), 
														  &TestButtonUI::button2, ms::ui::Box(100, 130, 90, 25),
														  &TestButtonUI::button3, ms::ui::Box(400, 100, 160, 30),
														  &TestButtonUI::button4, ms::ui::Box(200, 200, 50, 51),
														  &TestButtonUI::label_counter, ms::ui::el::Text::LayoutParams(ms::ui::Box(10, 10, 200, 25), ms::ui::AlignBegin));
private:
	char textbuf[32] {};
	int i = 0;

public:
	TestButtonUI() :
		button1("Button 1", ms::ui::el::Button::Callback::create(*this, &TestButtonUI::cb1)), 
		button2("Button 2", ms::ui::el::Button::Callback::create(*this, &TestButtonUI::cb2)),
		button3("Big button 3", ms::ui::el::Button::Callback::create(*this, &TestButtonUI::cb3)),
		button4("ARDS", ms::ui::el::Button::Callback::create(*this, &TestButtonUI::cb3)),
		label_counter(textbuf, 0xfc)
	{
		// Other init stuff etc.
		strcpy(textbuf, "Nothin.");
	}

	void draw() override {
		// Tell the LayoutManager to draw to the entire screen
		layout.redraw(*this);
	}

	bool handle(const ms::evt::TouchEvent& evt) override {
		if (!layout.handle(*this, evt)) {
			++i;
			if (!(i % 60)) {
				snprintf(textbuf, 32, "Invalid %d times", i);
				label_counter.set_label(textbuf);
			}
		}
		return true;
	}

	void draw_bg(const ms::ui::Box& box) override {
		draw::StackLocalBoundary bound(box.x, box.x + box.w + 1, box.y, box.y + box.h + 1);

		draw::rect(0, 0, 480, 50, 0b11'00'00'11);
		draw::rect(0, 50, 480, 272, 0);
	}
};

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
	status("Setting up UI...");
	util::delay(100); // TODO: remove me when there's more loading tasks... lol
	ms::ui::ui_16_font = ui_font; // set ui font

	// TODO: read previous UI state / serialize UI state and start the correct UI
	// TODO: check if we need to start the SD card

	TestButtonUI ui;

	// Tell the UI to draw it's bg layer
	ui.draw_bg(ms::ui::Box(0, 0, 480, 272));

	ms::evt::add(&ui);
	
	while (1) {
		util::delay(1);
		ms::in::poll();
		// TEMP TIME 
		auto start = LTDC->CPSR;
		ui.draw();
		auto end = LTDC->CPSR;

		int lines = std::abs(int(READ_BIT(end, LTDC_CPSR_CYPOS) - READ_BIT(start, LTDC_CPSR_CYPOS)));
		if (lines == 0) continue;
		printf("lines - %d\n", lines);
	}

	return 0;
}

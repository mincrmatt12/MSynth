#pragma once
// ui test
//
// Asks the user for: buttons, LEDS and knobs

#include "base.h"
#include <msynth/periphcfg.h>

struct UiTest {
	void start();
	TestState loop(); 

private:
	enum {
		Button,
		Led,
		Knob
	} state;

	uint32_t last_button;
	periph::ui::led last_led;
	periph::ui::knob last_knob;

	const void * uiFnt;
};

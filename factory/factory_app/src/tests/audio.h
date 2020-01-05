#pragma once
// audio test
//
// Sends out a 440hz sawtooth wave until the user gets bored of it
// and presses ENTER

#include "base.h"
#include <msynth/periphcfg.h>

struct AudioTest {
	void start();
	TestState loop(); 

private:
	const void * uiFnt;
	uint16_t sample_buffer[100];
};

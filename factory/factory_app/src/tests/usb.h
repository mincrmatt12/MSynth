#pragma once

#include <msynth/usb.h>
#include "base.h"

struct UsbTest {
	void start();
	TestState loop(); 

private:
	void got_midi(uint8_t *buf, size_t dat);

	// USB TEST:
	//
	// Designed only for MIDI devices, and shows them on screen as connected
	// as well as the data from them
	
	enum {
		WaitingForInserted,
		DetectingPeripheral,
		Ready,
		Disconnected,
		UndefinedState
	} state = WaitingForInserted, last_state = UndefinedState;

	const void * uiFnt;
	const void * bigFnt;

	usb::init_status init_error;
};

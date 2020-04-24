#pragma once
// INput handling
//
// This conglomerates all the various sources of input events (MIDI, touchscreen, USB-keyboard, etc.) and dispatches it at a high level.
//
// Specific parsing is handled elsewhere (synth/ui), this just formats it all correctly and hides some of the implementation details.
// It also holds the USB driver

#include <msynth/usb.h>
#include <stm32f4xx.h>

typedef usb::Host<usb::StaticStateHolder, usb::MidiDevice, usb::HID> MsUSB;

namespace ms::in {
	extern MsUSB usb_host;

	// Initialize the input subsystem
	void init();

	// Poll and handle input events
	void poll();
}

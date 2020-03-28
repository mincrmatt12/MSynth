#include "usb.h"
#include "msynth/periphcfg.h"
#include "msynth/sound.h"
#include <msynth/draw.h>
#include <msynth/fs.h>
#include <cmath>

#include <stdio.h>
#include <stm32f4xx_ll_dma.h>

extern usb::Host<usb::StaticStateHolder, usb::MidiDevice> usb_host;

namespace {
	// CRAPSynth Player™ state
	bool note_on = false;
	uint16_t freq = 0;
	uint16_t amplitude = 0;
	int16_t sample_buffer[1024] = {0};
	bool change = false;
}

void UsbTest::start() {
	uiFnt  = fs::open("fonts/djv_16.fnt");
	bigFnt = fs::open("fonts/lato_32.fnt");

	state = WaitingForInserted;
	usb_host.enable(); // cause major mayhem
}


TestState UsbTest::loop() {
	// Do display loop
	if (last_state != state) {
		switch (state) {
			case WaitingForInserted:
				draw::fill(0b11'11'00'00);
				draw::text(30, 100, "WaitingForInserted", bigFnt, 0xff);
				break;
			case DetectingPeripheral:
				draw::fill(0b11'00'11'00);
				draw::text(30, 100, "DetectingPeripheral", bigFnt, 0xff);
				break;
			case Ready:
				{
					draw::fill(0b11'00'11'11);
					draw::text(30, 100, "MidiRunning", bigFnt, 0x00);
					auto info = usb_host.info();
					if (info.vendor_name[0] && info.product_name[0]) {
						draw::text(draw::text(draw::text(30, 140, info.vendor_name, uiFnt, 0x00), 140, " : ", uiFnt, 0x00), 140, info.product_name, uiFnt, 0x00);
					}
					draw::text(30, 160, "[BKSP] to exit, or unplug the keyboard", uiFnt, 0b11'00'11'00);
				}
				break;
			case Disconnected:
				draw::fill(0b11'11'00'11);
				draw::text(30, 100, "Disconnected", bigFnt, 0xff);
				break;
			case UndefinedState:
				break;
		}

		last_state = state;
	}

	switch (state) {
		case WaitingForInserted:
			{
				if (usb_host.inserted()) {
					// Begin init
					state = DetectingPeripheral;
				}
			}
			break;
		case DetectingPeripheral:
			{
				init_error = usb_host.init_periph();
				if (init_error == usb::init_status::Ok) {
					usb_host.dev<usb::MidiDevice>().set_callback(usb::MidiDevice::EventCallback::create(*this, &UsbTest::got_midi));
					state = Ready;
				}
				else {
					draw::fill(0b11'11'00'00);
					uint16_t cursor = draw::text(30, 200, "Fail: ", bigFnt, 0xff);
					switch (init_error) {
						case usb::init_status::NotInserted:
							draw::text(cursor, 200, "NotInserted", uiFnt, 0xfc);
							break;
						case usb::init_status::NotSupported:
							draw::text(cursor, 200, "NotSupported", uiFnt, 0xfc);
							break;
						case usb::init_status::TxnErrorDuringEnumeration:
							draw::text(cursor, 200, "TxnErrorDuringEnumeration", uiFnt, 0xfc);
							break;
						case usb::init_status::Timeout:
							draw::text(cursor, 200, "Timeout", uiFnt, 0xfc);
							break;
						case usb::init_status::DeviceInitError:
							draw::text(cursor, 200, "DeviceInitError", uiFnt, 0xfc);
							break;
						default:
							draw::text(cursor, 200, "?? invalid retcode", uiFnt, 0xfc);
							break;
					}

					util::delay(2000);
					
					// disable usb

					usb_host.disable();
					return Fail;
				}
			}
		case Ready:
			{
				if (!usb_host.inserted()) {
					// Disconnect detected
					usb_host.disable();
					state = Disconnected;
				}
				usb_host.dev<usb::MidiDevice>().update();
				if (change) {
					change = false;
					draw::rect(0, 200, 480, 240, 0);
					char buf[32];
					snprintf(buf, 32, "on %d; freq %d; amp %d", note_on, freq, amplitude);
					draw::text(60, 230, buf, uiFnt, 0xff);
				}

				periph::ui::poll();
				if (periph::ui::pressed(periph::ui::button::BKSP)) {
					usb_host.disable();
					return Ok;
				}
			}
			break;
		case Disconnected:
			{
				sound::stop_output();
				util::delay(2000);
				return Ok;
			}
		case UndefinedState:
			break;
	}

	return InProgress;
}

void UsbTest::got_midi(uint8_t *buf, size_t len) {
	printf("got midi: ");
	for (int i = 0; i < len; ++i) {
		printf("%02x ", buf[i]);
	}
	puts(";");
	
	if (len == 3) {
		change = true;
		if (buf[0] == 0x90 && buf[2]) {
			amplitude = (uint16_t)(buf[2]) * (9830 / 0x7f);
			freq = (uint16_t)std::round(std::pow(2.0f, (buf[1] - 69.0f) / 12.0f) * 440.0f);
			note_on = true;
		}
		else if (buf[0] == 0x80) {
			if (freq == (uint16_t)std::round(std::pow(2.0f, (buf[1] - 69.0f) / 12.0f) * 440.0f)) {
				note_on = false;
			}
			else return;
		}
		else return;
		if (note_on) {
			LL_DMA_DisableStream(DMA1, LL_DMA_STREAM_4);

			int length = 44100 / freq;
			float offset = amplitude / length;
			float b = -amplitude / 2;
			for (int i = 0; i < length; ++i) {
				sample_buffer[i*2] = b;
				sample_buffer[i*2 + 1] = b;
				b += offset / 2;
			}

			sound::continuous_sample(sample_buffer, length);
		}
		else {
			LL_DMA_DisableStream(DMA1, LL_DMA_STREAM_4);
			memset(sample_buffer, 0, sizeof(sample_buffer));
			sound::continuous_sample(sample_buffer, 256);
		}
	}
}

#include "stm32f4xx_ll_dma.h"
#include <cmath>
#include <stdio.h>
#include <msynth/periphcfg.h>
#include <msynth/lcd.h>
#include <msynth/draw.h>
#include <msynth/usb.h>
#include <msynth/sound.h>
#include <msynth/sd.h>
#include <msynth/fs.h>

#define BUFSIZE 8192

ISR(SDIO) {
	sd::sdio_interrupt();
}

const void * uiFnt;

int16_t buffers[2][BUFSIZE];
bool sd_is_transferring = false;
bool not_up_to_date_flag = false;

uint32_t pos = 0;
uint32_t size = 0;

int16_t voltagediv = 1;

void output(const char * txt) {
	static uint16_t y = 50;
	if (y > 270) {
		draw::rect(0, 36, 480, 272, 0xff);
		y = 50;
	}
	draw::text(1, y, txt, uiFnt, 0x00);
	y += 16;
	puts(txt);
}

template<typename ...Args>
inline void output(const char *fmt, Args& ...args) {
	char buf[128];
	snprintf(buf, 128, fmt, std::forward<Args>(args)...);
	output(buf);
}

void sd_read_callback(void *arg, sd::access_status status) {
	if (pos * 512 >= size) {
		output("pos = %d; size = %d", pos, size);
		output("Done!");
		sound::stop_output();
		return;
	}
	if (status != sd::access_status::Ok) {
		output("read failed");
		LL_DMA_DisableStream(DMA1, LL_DMA_STREAM_4);
		return;
	}
	if (not_up_to_date_flag && sd_is_transferring) {
		LL_DMA_DisableStream(DMA1, LL_DMA_STREAM_4);
		// Handle behind
		output("behind");

		if (/*the active buffer is b1*/ arg == buffers[1]) {
			memcpy(buffers[0], buffers[1], BUFSIZE);
		}

		sd_is_transferring = false;
		sd::read(pos, buffers[1], BUFSIZE/256, sd_read_callback, nullptr);
		pos += BUFSIZE/256;
	}
	else if (not_up_to_date_flag) {
		output("restart");
		sd_is_transferring = false;
		not_up_to_date_flag = false;

		sound::setup_double_buffer(buffers[0], buffers[1], BUFSIZE / 2);
	}
	else {
		for (int i = 0; i < BUFSIZE; ++i) {
			((int16_t *)(arg))[i] /= voltagediv;
		}
		if (!(pos % 0x800)) output("pos = %d", pos);
		sd_is_transferring = false;
	}
}

// STREAM SYSTEM
//
// Initially both buffers are setup, once a buffer is sent, we try to load the next one.
// If this completes before the next TCIF of the sound buffer, we keep going.
// Otherwise, the sound output is paused. If this occurs, the SD handler will start another transfer, which when complete will restart the process.

ISR(DMA1_Stream4) {
	if (LL_DMA_IsActiveFlag_TE4(DMA1)) {
		LL_DMA_ClearFlag_TE4(DMA1);
		output("DMA TE!");
		LL_DMA_DisableStream(DMA1, LL_DMA_STREAM_4);
		return;
	}
	else if (!LL_DMA_IsActiveFlag_TC4(DMA1)) return;
	LL_DMA_ClearFlag_TC4(DMA1);

	// TC is set, we check if sd_is_transferring
	if (!sd_is_transferring && sd::card.active_state != sd::Card::ActiveStateReading) {
		// Good, we can continue and start the SD to fill up this current buffer
		int16_t * buf = LL_DMA_GetCurrentTargetMem(DMA1, LL_DMA_STREAM_4) == LL_DMA_CURRENTTARGETMEM1 ? buffers[0] : buffers[1];
		sd_is_transferring = true;
		if (sd::read(pos, buf, BUFSIZE/256, sd_read_callback, buf) != sd::access_status::InProgress) {
			output("Read failed");
			LL_DMA_DisableStream(DMA1, LL_DMA_STREAM_4);
		}
		pos += BUFSIZE/256;
	}
	else {
		not_up_to_date_flag = true;
	}
}

int main() {
	periph::setup_dbguart();
	periph::setup_blpwm();
	periph::setup_ui();
	puts("\nStarting MSynth v0");
	periph::bl::set(160);
	lcd::init();
	draw::fill(0xff);
	puts("Starting filesystem");
	if (!fs::is_present()) {
		puts("filesystem check failed");
		return 1;
	}
	const void * bigFnt = fs::open("fonts/lato_32.fnt");
	uiFnt = fs::open("fonts/djv_16.fnt");
	draw::text(1, 33, "MSynth SD player", bigFnt, 0);

	output("Starting SD");
	sd::init();
	output("Waiting for SD card");
	output("It should have a 4byte size with audio data");
retry:
	while (!sd::inserted()) {
		;
	}
	output("Starting SD card");
	auto status = sd::init_card();
	if (status != sd::init_status::Ok) {
		switch (status) {
			case sd::init_status::CardUnusable:
				output("err CardUnusable");
				break;
			case sd::init_status::CardNotInserted:
				output("err CardNotInserted");
				break;
			case sd::init_status::MultipleCardsDetected:
				output("err MultipleCardsDetected");
				break;
			case sd::init_status::CardNotResponding:
				output("err CardNotResponding");
				break;
			case sd::init_status::CardIsSuperGluedShut:
				output("err CardIsSuperGluedShut");
				break;
			case sd::init_status::CardIsElmersGluedShut:
				output("err CardIsElmersGluedShut");
				break;
			case sd::init_status::NotInitialized:
				output("err NotInitialized");
				break;
			case sd::init_status::InternalPeripheralError:
				output("err InternalPeripheralError");
				break;
			case sd::init_status::NotSupported:
				output("err NotSupported");
			default:
				break;
		}
		while (1) {;}
		return 1;
	}
	output("Ok");

	output("Checking size of data");
	uint8_t data[512];
	if (sd::read(0, data, 1) != sd::access_status::Ok) {
		output("failed");
	};

	size = *(uint32_t *)(data);
	output("Data size = %d", size);
	util::delay(100);
	output("Enabling NVIC");
	NVIC_EnableIRQ(DMA1_Stream4_IRQn);
	NVIC_SetPriority(DMA1_Stream4_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));

	memset(buffers[0], 0, sizeof(buffers[0]));
	memset(buffers[1], 0, sizeof(buffers[1]));

restart:
	output("Starting");
	if (sd::read(pos, buffers[0], BUFSIZE/256) != sd::access_status::Ok) {
		output("R1 fail");
		util::delay(50);
		goto retry;
	}
	if (sd::read(pos+BUFSIZE/256, buffers[1], BUFSIZE/256) != sd::access_status::Ok) {
		output("R2 fail");
		util::delay(50);
		goto retry;
	}
	pos += (BUFSIZE/256);
	output("Read 16k samples");
	output("Starting audio player!");
	sound::init();
	util::delay(10);

	sound::setup_double_buffer(buffers[0], buffers[1], BUFSIZE / 2);
	// Main MSynth start
	while (1) {
		util::delay(1);
		auto current = (float)periph::ui::get(periph::ui::knob::VOLUME);
		auto target_max = (std::pow((4096 - current) / 4096.0f, 2.5f) * 32768.f);
		current = std::max(1.f, std::min(32767.0f, 32768.f / target_max));

		auto old = voltagediv;
		voltagediv = std::round(current);

		if (!LL_DMA_IsEnabledStream(DMA1, LL_DMA_STREAM_4)) goto restart;
	}

	return 0;
}

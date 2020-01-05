#include "audio.h"

#include <msynth/draw.h>
#include <msynth/fs.h>
#include <msynth/sound.h>
#include <msynth/periphcfg.h>

#include <string.h>
#include <stm32f4xx.h>

void AudioTest::start() {
	uiFnt = fs::open("fonts/djv_16.fnt");

	draw::fill(0b11'11'11'00); // solid orange
	draw::text(200, 100, "LineoutAudio", uiFnt, 0);

	// Set the buffer going
	for (int i = 0, j = 0; i < 65536; i += 1310, j += 2) {
		sample_buffer[j] = i;
		sample_buffer[j+1] = i;
	}

	// Set the audio out
	sound::continuous_sample(sample_buffer, 100);
	// Enable speaker
	sound::set_mute(false);
}

TestState AudioTest::loop() {
	periph::ui::poll();

	if (periph::ui::pressed(periph::ui::button::F1)) {
		sound::set_mute(true);
		periph::ui::set(periph::ui::led::F1, true);
	}
	else if (periph::ui::pressed(periph::ui::button::F2)) {
		sound::set_mute(false);
		periph::ui::set(periph::ui::led::F1, false);
	}
	else if (periph::ui::pressed(periph::ui::button::ENTER)) {
		sound::stop_output();
		return Ok;
	}

	return InProgress;
}

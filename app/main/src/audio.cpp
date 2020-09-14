#include "audio.h"
#include <cstdio>
#include <cstring>
#include <msynth/sound.h>
#include "msynth/util.h"
#include "stm32f429xx.h"
#include "stm32f4xx_ll_dma.h"
#include <cstddef>

namespace ms::audio {
	constexpr inline size_t master_buffer_sample_count = 300;

	int16_t master_sample_buffer[2][master_buffer_sample_count * 2]{}; // *2 for stereo panning.

	// TODO: proper interface for this
	//
	// + mixers + effects + etc. etc. etc. aaaaaaaaaaaaaaaaaaaaaaaaaa
	
	synth::playback::AudioGenerator *active_generator = nullptr;

	int32_t master_volume = INT16_MAX;

	void dma_interrupt() {
		if (LL_DMA_IsActiveFlag_TE4(DMA1)) {
			LL_DMA_ClearFlag_TE4(DMA1);
			LL_DMA_DisableStream(DMA1, LL_DMA_STREAM_4);
			sound::stop_output();
			return;
		}
		else if (!LL_DMA_IsActiveFlag_TC4(DMA1)) return;
		LL_DMA_ClearFlag_TC4(DMA1);

		// Generate the next bunch of samples
		int16_t * buf = LL_DMA_GetCurrentTargetMem(DMA1, LL_DMA_STREAM_4) == LL_DMA_CURRENTTARGETMEM1 ? master_sample_buffer[0] : master_sample_buffer[1];

		if (!active_generator) {
			memset(buf, 0, master_buffer_sample_count * sizeof(uint16_t) * 2);
			return;
		}

		const static auto generate = &synth::playback::AudioGenerator::generate;
		typedef int16_t (*hoisted_loop_type)(synth::playback::AudioGenerator *);
		hoisted_loop_type hoisted_loop = (hoisted_loop_type)(active_generator->*generate);

		for (size_t sample = 0; sample < master_buffer_sample_count*2; sample += 2) {
			int16_t raw = hoisted_loop(active_generator);
			raw = static_cast<int16_t>((static_cast<int32_t>(raw) * master_volume) / INT16_MAX);
			buf[sample] = raw;
			buf[sample+1] = raw;
		}
	}

	void add_source(synth::playback::AudioGenerator *ptr) {
		active_generator = ptr;
	}

	void stop() {
		sound::stop_output();
		active_generator = 0;
	}

	void start() {
		sound::setup_double_buffer(master_sample_buffer[0], master_sample_buffer[1], master_buffer_sample_count);
	}

	void init() {
		puts("Starting sound subsystem");
		sound::init();
		util::delay(10);
	}

	void set_volume(int16_t max_level) {
		master_volume = max_level;
	}
}

#pragma once
// sound.h -- routines for setting up the I2S peripheral
//
// This sets up both I2S for output; with the RX DMA and RXNE interrupt set
//
// There are some basic routines present for outputting data.
// These include:
//    - a scheme where audio data is sent out in a single block with DMA
//    - a scheme where audio data is sent out in a loop with DMA (generally intended for
//    	testing with very simple waveforms, e.g. create a triangle wave at some frequency and
//    	circular DMA it out)
//    - a dual buffering scheme, where the app can fill up one buffer (either ahead of time
//    	or in sync with the RXNE interrupt) and DMA will output the other.

#include <stdint.h>

namespace sound {
	// Setup the AUDIO system (and speaker mute GPIO)
	void init();

	// Enable/disable speaker mute
	//
	// NOTE: this doesn't really work correctly due to issues with the PCB.
	// It's possible some janky soldering jobs and external components will revive this feature, but right now it does not work
	void set_mute(bool mute);

	// Send out data in a single block.
	void single_shot(int16_t *audio_data, uint32_t nsamples);

	// Send out the audio data in a loop until cancelled.
	void continuous_sample(int16_t *audio_data, uint32_t nsamples);

	// Set up double buffered output mode
	void setup_double_buffer(int16_t *audio_data_1, int16_t *audio_data_2, uint32_t blocksize);

	// Is the current output done? Only valid in single_shot mode
	bool finished_sending();

	// Stop the current output task
	void stop_output();
}

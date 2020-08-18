#include "waves.h"
#include <cmath>

// TODO: decide if all of these methods should be declared as -O3?
//
// or maybe synth methods should get -ffast-math

bool ms::synth::mod::SqwWave::generate(const Cfg& config) {
	float recip_freq = (1.f/frequency);
	output = fmod(curr_time, recip_freq) / recip_freq;

	if ((config.inverted && (output < duty)) || (!config.inverted && (output > duty)))
		output = amplitude;
	else
		output = config.centered ? -amplitude : 0;

	output += dc_offset;
	return true;
}

bool ms::synth::mod::TriangleWave::generate(const Cfg& config) {
	float period = (1.f/frequency);
	
	// TODO: respect config
	output = 2.f*amplitude * fabs(fmod(curr_time, period) - (period / 2.f));
	output /= (period / 2.f);
	output += dc_offset - (amplitude);

	return true;
}

bool ms::synth::mod::SinWave::generate(const Cfg& config) {
	output = sinf((static_cast<float>(M_TWOPI) / frequency) * curr_time);

	if (config.rectified) {
		output = fabs(output);
	}
	if (config.inverted) {
		output = -output;
	}

	output *= amplitude;
	output += dc_offset;
	
	return true;
}

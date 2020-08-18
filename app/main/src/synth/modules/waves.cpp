#include "waves.h"
#include <cmath>
#include <cstdio>

// TODO: decide if all of these methods should be declared as -O3?
//
// or maybe synth methods should get -ffast-math

bool ms::synth::mod::SqwWave::generate(const Cfg& config) {
	output = fmod(curr_time, 1.f/frequency) / (1.f/frequency);
	output = (output < duty) ? -amplitude : amplitude;

	output += dc_offset;
	return true;
}

bool ms::synth::mod::TriangleWave::generate(const Cfg& config) {
#define OOF 100000
	int period = (1.f/frequency)*OOF;

	return true;
}

bool ms::synth::mod::SinWave::generate(const Cfg& config) {
	output = sinf((static_cast<float>(M_TWOPI) * frequency) * curr_time);

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

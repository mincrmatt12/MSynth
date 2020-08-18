#include "waves.h"
#include <cmath>
#include <cstdio>

// TODO: decide if all of these methods should be declared as -O3?
//
// or maybe synth methods should get -ffast-math

bool ms::synth::mod::SqwWave::generate(const Cfg& config) {
	if (curr_time == 0) {
		incstate = 0;
		output = dc_offset - amplitude;
		upstate = false;
	}
	incstate += (1.f/44100.f);
	if (upstate && incstate > duty*(1.f/frequency)) {
		upstate = false;
		output = dc_offset + amplitude;
	}
	else if (!upstate && incstate > (1.f/frequency)) {
		upstate = true;
		output = dc_offset - amplitude;
		incstate -= (1.f/frequency);
	}
	
	return true;
}

bool ms::synth::mod::TriangleWave::generate(const Cfg& config) {
	if (curr_time == 0) {
		incstate = 0;
	}

	incstate += (frequency/44100.f);
	if (incstate > 1.f) {
		incstate -= 1.f;
	}
	output = dc_offset + amplitude * (1 - fabsf(incstate - 0.5f)*4.f);

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

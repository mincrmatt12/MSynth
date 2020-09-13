#include "waves.h"
#include "../util.h"
#include <cmath>
#include <cstdio>

// TODO: decide if all of these methods should be declared as -O3?
//
// or maybe synth methods should get -ffast-math

bool ms::synth::mod::SqwWave::generate(const Cfg& config) {
	if (curr_time == 0) {
		incstate = 0;
	}
	incstate += (frequency/44100.f);
	if (incstate > 1.f) incstate -= 1.f;
	if (incstate < duty)
		output = dc_offset - (config.inverted ? -amplitude : amplitude);
	else
		output = dc_offset + (config.inverted ? -amplitude : amplitude);
	
	return true;
}

bool ms::synth::mod::TriangleWave::generate(const Cfg& config) {
	if (curr_time == 0.f) {
		incstate = 0.f;
	}

	incstate += (frequency/44100.f);
	if (incstate > 1.f) {
		incstate -= 1.f;
	}
	if (config.inverted) {
		output = dc_offset - amplitude * (1 - fabsf(incstate - 0.5f)*4.f);
	}
	else {
		output = dc_offset + amplitude * (1 - fabsf(incstate - 0.5f)*4.f);
	}

	return true;
}

bool ms::synth::mod::SawWave::generate(const Cfg& config) {
	if (curr_time == 0.f) {
		incstate = 0.f;
	}

	incstate += (frequency/44100.f);
	if (incstate > 1.f) {
		incstate -= 1.f;
	}
	output = dc_offset + (incstate - 0.5f) * amplitude;
	if (config.inverted) output = -output;

	return true;
}

bool ms::synth::mod::SinWave::generate(const Cfg& config) {
	if (curr_time == 0.f) incstate = 0.f;
	incstate += (frequency/44100.f);
	if (incstate > 1.f) {
		incstate -= 1.f;
	}
	output = wavesin(incstate);

	if (config.rectified) {
		output = fabsf(output);
		output = output * 2.f - 1.f;
	}
	if (config.inverted) {
		output = -output;
	}

	output *= amplitude;
	output += dc_offset;
	
	return true;
}

#include "env.h"
#include <cmath>

bool ms::synth::mod::ADSR::generate(const Cfg& cfg) {
	output = 0.f;

	if (off_time < 0.f && curr_time < cfg.attack_time) {
		output = (curr_time / cfg.attack_time) * cfg.attack_level;
	}
	else if (off_time < 0.f && curr_time < (cfg.attack_time + cfg.decay_time)) {
		output = cfg.attack_level - ((curr_time - cfg.attack_time) / cfg.decay_time) * (cfg.attack_level - cfg.sustain_level);
	}
	else if (off_time < 0.f) {
		output = cfg.sustain_level;
	}
	else {
		output = cfg.sustain_level - ((curr_time - off_time) / cfg.release_time) * cfg.sustain_level;
	}

	output *= scale;

	if (off_time < 0.f) return false;
	return (curr_time - off_time) > cfg.release_time;
}

bool ms::synth::mod::ExpADSR::generate(const Cfg& cfg) {
	output = 0.f;

	if (off_time < 0.f && curr_time < cfg.attack_time) {
		output = (1.f - powf(1.f - curr_time / cfg.attack_time, 2.f)) * cfg.attack_level;
	}
	else if (off_time < 0.f && curr_time < (cfg.attack_time + cfg.decay_time)) {
		output = cfg.attack_level - powf((curr_time - cfg.attack_time) / cfg.decay_time, 2.f) * (cfg.attack_level - cfg.sustain_level);
	}
	else if (off_time < 0.f) {
		output = cfg.sustain_level;
	}
	else {
		output = cfg.sustain_level - powf((curr_time - off_time) / cfg.release_time, 2.f) * cfg.sustain_level;
	}

	output *= scale;

	if (off_time < 0.f) return false;
	return (curr_time - off_time) > cfg.release_time;
}

#include "env.h"

bool ms::synth::mod::ADSR::generate(const Cfg& cfg) {
	output = 0;

	if (off_time < 0 && curr_time < cfg.attack_time) {
		output = (curr_time / cfg.attack_time) * cfg.attack_level;
	}
	else if (off_time < 0 && curr_time < (cfg.attack_time + cfg.decay_time)) {
		output = cfg.attack_level - ((curr_time - cfg.attack_time) / cfg.decay_time) * (cfg.attack_level - cfg.sustain_level);
	}
	else if (off_time < 0) {
		output = cfg.sustain_level;
	}
	else {
		output = cfg.sustain_level - ((curr_time - off_time) / cfg.release_time) * cfg.sustain_level;
	}

	output *= scale;

	if (off_time < 0) return false;
	return (curr_time - off_time) > cfg.release_time;
}

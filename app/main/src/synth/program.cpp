#include "program.h"

void ms::synth::Voice::reset_time() {
	on_time = 0.0f;
	program.set_off_time(-1.f, dyncfg_blob);
	original_pitch = -1.f;
}

void ms::synth::Voice::set_velocity(float vel) {
	program.set_velocity(vel, dyncfg_blob);
}

void ms::synth::Voice::set_pitch(float freq) {
	program.set_pitch(freq, dyncfg_blob);
	if (original_pitch < 0) original_pitch = freq;
}

void ms::synth::Voice::mark_off() {
	program.set_off_time(on_time, dyncfg_blob);
}

int16_t ms::synth::Voice::generate(bool &cut_note) {
	float result;

	program.set_time(on_time, dyncfg_blob);
	cut_note = program.generate(&result, dyncfg_blob);
	on_time += (1.f/44100.f); // TODO: MAKE THIS CONFIGURABLE
	
	return result * INT16_MAX;
}

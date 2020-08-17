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

bool ms::synth::Program::generate(float *result, void *blob) const {
	// Call the procedure
	return ((bool (*)(float *, void *))(this->compiled_procedure.data()))(result, blob);
}

namespace {
inline void set_x(float v, void *blob, const std::vector<uintptr_t>& dat, uint8_t i, uint8_t i2) {
	for (; i < i2; ++i) {
		*reinterpret_cast<float *>(reinterpret_cast<uintptr_t>(blob) + dat[i]) = v;
	}
}
}

void ms::synth::Program::set_pitch(float v, void *blob) const {
	set_x(v, blob, this->offset_pool, 0, this->pitch_end);
}

void ms::synth::Program::set_velocity(float v, void *blob) const {
	set_x(v, blob, this->offset_pool, this->pitch_end, this->velocity_end);
}

void ms::synth::Program::set_time(float v, void *blob) const {
	set_x(v, blob, this->offset_pool, this->velocity_end, this->time_end);
}

void ms::synth::Program::set_off_time(float v, void *blob) const {
	set_x(v, blob, this->offset_pool, this->time_end, this->offset_pool.size());
}

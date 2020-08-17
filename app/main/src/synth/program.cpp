#include "program.h"
#include "jit.h"
#include <algorithm>

void ms::synth::Voice::reset_time() {
	on_time = 0.0f;
	off_time = -1.f;
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

	bool is_in_input_list(const ms::synth::Patch::ModuleHolder *search_in, const ms::synth::Patch::ModuleHolder *search_for) {
		for (const auto& x : search_in->get_links()) {
			if (x.source == ms::synth::predef::ModuleRefGlobalIn || x.source == nullptr) continue;
			if (x.source == search_for) return true;
			if (is_in_input_list(x.source, search_for)) return true;
		}
		return false;
	}

	template<typename Func>
	void add_offset_pool_entries(std::vector<uintptr_t>& into, const std::vector<const ms::synth::Patch::ModuleHolder *>& from, Func&& predicate) {
		size_t offset = 0;
		for (const auto& x : from) {
			for (const auto& link : x->get_links()) {
				if (predicate(link)) {
					into.push_back(offset + x->mod->inputs[link.target_idx].offset);
				}
			}

			offset += x->mod->dyncfg_size;
			if (offset % 4)
				offset += 4 - (x->mod->dyncfg_size);
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

ms::synth::Program::Program(const ms::synth::Patch& patch) {
	// The first step in creating the program is to correctly order all of the modules in strictly depth-first order.
	std::vector<const Patch::ModuleHolder *> ordered_copy;
	std::vector<jit::PsuedoInstruction>      pinsns;
	ordered_copy.reserve(patch.all_modules().size());

	size_t dyncfg_total_size = 0;
	for (const auto& x : patch.all_modules()) {
		ordered_copy.push_back(x.get());
		dyncfg_total_size += x->mod->dyncfg_size;
		// Align to 4 bytes
		if (dyncfg_total_size % 4)
			dyncfg_total_size += 4 - (dyncfg_total_size % 4);
	}

	dyncfg_original_len = dyncfg_total_size;

	// Sort them:
	// 	Predicate is operator<, returns true if A is before B. A is strictly before B if A is anywhere in the input chain of B.
	std::sort(ordered_copy.begin(), ordered_copy.end(), [](const Patch::ModuleHolder *a, const Patch::ModuleHolder *b){
		return is_in_input_list(b, a);
	});

	// Alright, we now have a workable order of the modules.
	//
	// We can now allocate the dyncfg blob (well we could have earlier but we didn't)
	this->dyncfg_original.reset(new uint32_t[dyncfg_total_size / 4]); // using uint32_t here to ensure 32-bit alignment without
																			// having to use a separate deleter with new (std::align_val_t(4)) uint32_t[4]
	
	// Now, we initialize the dyncfg with the values from the Patch. Linking the outputs is performed during psuedoinstruction generation
	for (uint8_t *i = reinterpret_cast<uint8_t *>(this->dyncfg_original.get()); const auto& x : patch.all_modules()) {
		memcpy(i, x->dynamic_configuration.get(), x->mod->dyncfg_size);
		i += x->mod->dyncfg_size;
		if (x->mod->dyncfg_size % 4)
			i += 4 - (x->mod->dyncfg_size % 4);
	}

	// Keep track of the state of the MJIT
	uint8_t *dyncfg_for_x = (uint8_t *)this->dyncfg_original.get();
	size_t   mod_index = 0;
	jit::PsuedoInstruction insn;

	// Begin creating pseudo instructions.
	//
	// This operates as a loop emitting in the order:
	// 	- LoadConfig
	// 	- Run
	// 	- LoadResult (optional)
	// 	- Copy (optional/multiple)
	// 	- AdvanceDynConfig (if not end)
	for (const auto& x : ordered_copy) {
		// Load the config
		insn.opcode = jit::PsuedoInstruction::OpcodeLoadConfig;
		insn.config_location = x->configuration.get();
		pinsns.push_back(insn);
		// Run the module
		insn.opcode = jit::PsuedoInstruction::OpcodeRunModule;
		insn.proc   = x->mod->proc;
		pinsns.push_back(insn);
		// Clear output enables
		for (size_t i = 0; i < x->mod->output_count; ++i) {
			const auto& output = x->mod->outputs[i];
			if (output.offset_enabled != -1) 
				*(bool *)(dyncfg_for_x + output.offset_enabled) = false;
		}
		// Check if we need to load the result pointer
		if (x == patch.output_source) {
			const auto& output = x->mod->outputs[patch.output_idx];
			// Emit the instruction to load the pointer into the dyncfg at runtime
			insn.opcode = jit::PsuedoInstruction::OpcodeLoadResult;
			insn.advance_or_result_offset = output.offset; // dyncfg pointer is at this module's dyncfg, so we just use the raw offset
			pinsns.push_back(insn);
			// Check if we need to setup enabled/min/max
			if (output.offset_enabled != -1) 
				*(bool *)(dyncfg_for_x + output.offset_enabled) = true;
			if (output.offset_min != -1) 
				*(float *)(dyncfg_for_x + output.offset_min) = -1.f;
			if (output.offset_max != -1) 
				*(float *)(dyncfg_for_x + output.offset_max) = 1.f;
		}
		size_t dyncfg_offset_for_potential = x->mod->dyncfg_size;
		if (dyncfg_offset_for_potential % 4)
			dyncfg_offset_for_potential += 4 - (dyncfg_offset_for_potential % 4);
		// Setup output copies
		for (size_t i = mod_index + 1; i < ordered_copy.size(); ++i) {
			// Check if this module points to here
			for (const auto& link : ordered_copy[i]->get_links()) {
				if (link.source == x) {
					const auto& output = x->mod->outputs[link.source_idx];
					const auto& input  = ordered_copy[i]->mod->inputs[link.target_idx];

					// Setup dyncfg data
					if (output.offset_enabled != -1)
						*(bool *)(dyncfg_for_x + output.offset_enabled) = true;
					if (output.offset_min != -1) 
						*(float *)(dyncfg_for_x + output.offset_min) = input.min;
					if (output.offset_max != -1)
						*(float *)(dyncfg_for_x + output.offset_max) = input.max;

					// Emit a copy instruction
					insn.opcode = jit::PsuedoInstruction::OpcodeCopyFloat;
					insn.source_offset = output.offset;
					insn.target_offset = dyncfg_offset_for_potential + input.offset;
					pinsns.push_back(insn);
				}
			}

			// Update offset info
			dyncfg_offset_for_potential += ordered_copy[i]->mod->dyncfg_size;
			if (dyncfg_offset_for_potential % 4)
				dyncfg_offset_for_potential += 4 - (dyncfg_offset_for_potential % 4);
		}
		// If not at end, advance dyn config
		if (x != ordered_copy.back()) {
			uintptr_t offset = x->mod->dyncfg_size;
			if (offset % 4)
				offset += 4 - (offset % 4);
			
			insn.opcode = jit::PsuedoInstruction::OpcodeAdvanceDynConfig;
			insn.advance_or_result_offset = offset;
			pinsns.push_back(insn);

			// Also advance these
			mod_index += 1;
			dyncfg_for_x += offset;
		}
	}

	// Generate offset pool
	add_offset_pool_entries(offset_pool, ordered_copy, [](const Patch::ModuleLink& link){
		return link.source == predef::ModuleRefGlobalIn && link.source_idx == predef::GlobalInPitchIdx;
	});
	pitch_end = offset_pool.size();
	add_offset_pool_entries(offset_pool, ordered_copy, [](const Patch::ModuleLink& link){
		return link.source == predef::ModuleRefGlobalIn && link.source_idx == predef::GlobalInVelocityIdx;
	});
	velocity_end = offset_pool.size();
	add_offset_pool_entries(offset_pool, ordered_copy, [](const Patch::ModuleLink& link){
		return link.source == predef::ModuleRefGlobalIn && link.source_idx == predef::GlobalInOnTimeIdx;
	});
	time_end = offset_pool.size();
	add_offset_pool_entries(offset_pool, ordered_copy, [](const Patch::ModuleLink& link){
		return link.source == predef::ModuleRefGlobalIn && link.source_idx == predef::GlobalInOffTimeIdx;
	});

	// Run JIT
	jit::assemble(compiled_procedure, pinsns);
}

ms::synth::Voice * ms::synth::Program::new_voice() {
	// malloc a new dyncfg-blob
	void * blob = malloc(dyncfg_original_len);
	memcpy(blob, dyncfg_original.get(), dyncfg_original_len);
	return new ms::synth::Voice(*this, blob);
}

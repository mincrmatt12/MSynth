#pragma once
// 
// A _program_ combines various _modules_ into a single unit that can produce monophonic audio.
// A program itself has no state, rather it is effecitvely a patch that has been instantiated for processing.
// The per-voice state is then created on demand and fed into the program to create a sample (and update the state)
//
// Like most modern synths, programs are tied into an _effect bus_ which does stuff like multi-channel reverb. Per-channel reverb
// is a module. The output of programs plus all the other sound sources on the system can be connected to effects. In fact, the
// output of an effect is itself a component on the effect bus, as it isn't really a bus: it's a unidirectional pipeline out to an eventual
// sink.
//
// TODO: effects, patches, most stuff
//
//

#include <utility>
#include <stdlib.h>
#include <stdint.h>
#include <vector>
#include "patch.h"

namespace ms::synth {
	struct Program;

	// A single voice. These are created from the programs
	struct Voice {
		// Voices have the following "inputs":
		// 	 - note pitch (float frequency in hz)
		// 	 - note velocity (float 0-1)
		// 	 - note on time (float length in seconds)
		// 	 
		// the on time is computed based on the sample rate and is increased by this object.
		// the pitch/velocity can be updated via function calls

		void set_pitch(float pitch);
		void set_velocity(float velocity);
		void reset_time();
		void mark_off();

		int16_t generate(bool &cut_note);

		~Voice() {
			free(dyncfg_blob);
		}
		Voice(const Voice& voice) = delete;
		Voice(Voice&& voice) : program(voice.program) {
			std::swap(dyncfg_blob, voice.dyncfg_blob);
			std::swap(on_time, voice.on_time);
		}

		friend Program;

		const float& pitch() {return original_pitch;}
		const float& held_time() {return on_time;}

	private:
		const Program& program;
		float on_time, original_pitch;
		void *dyncfg_blob = nullptr;

		Voice(const Program& program, void *dyncfg_blob) : program(program),
			on_time(0.0f), original_pitch(-1.f), dyncfg_blob(dyncfg_blob) {
		}
	};
	
	// A configured program, created from a patch. These are configured in the patch, and are then finalized into these objects. Only one exists per patch.
	//
	// They contain a member function which allocates/deallocates Voices which contain references to the procedure and configuration in the Program and a private dynconfiguration
	// blob, and themselves are the primary interface to the rest of MSynth
	struct Program {
		Voice* new_voice();

		friend Voice;

		// Create a new program.
		//
		// If patch is modified after this point in _any way_ this instance is completely useless and dangerous to use.
		// 
		// This constructor does allocate quite a lot of temporary stuff in addition to the 
		Program(const Patch& patch);
	private:
		// TODO: check if there's a better datatype to use here? in terms of size/speed/etc.

		// Offsets that set_pitch and friends need
		std::vector<uintptr_t> offset_pool;
		// The compiled program
		//
		// yes it's stored in a vector, don't you store your bytecode in a vector?
		// no?
		std::vector<uint16_t> compiled_procedure;

		// only 8 bits for packing/size reasons
		uint8_t pitch_end, velocity_end, time_end;
		
		// the fully assembled dyncfg
		std::unique_ptr<uint32_t[]> dyncfg_original;

		// size of assembled dyncfg
		size_t dyncfg_original_len;
		
		bool generate(float *out, void *dyncfg_blob) const;
		void set_pitch(float pitch, void *dyncfg_blob) const;
		void set_velocity(float velocity, void *dyncfg_blob) const;
		void set_time(float on_time, void *dyncfg_blob) const;
		void set_off_time(float off_time, void *dyncfg_blob) const;
	};
}

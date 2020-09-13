#pragma once

#include "../module.h"

// Various core modules that implement envelopes.
//

namespace ms::synth::mod {
	struct ADSR {
		struct Cfg {
			float attack_time, decay_time, release_time;
			float attack_level, sustain_level;
		};

		float scale;

		float curr_time;
		float off_time;
		
		float output;

		bool generate(const Cfg& cfg);
	};

	struct ExpADSR : ADSR {
		bool generate(const Cfg& cfg);
	};

	constexpr auto ADSRInputs = make_inputs(
		make_input("scale", &ADSR::scale),
		make_input(predef::AutoOnTime, &ADSR::curr_time),
		make_input(predef::AutoReleaseTime, &ADSR::off_time)
	);

	constexpr auto ADSROutputs = make_outputs(
		make_output("", &ADSR::output)
	);

	constexpr auto ADSRModule = make_module<ADSR>(
		"adsr_envelope",
		ADSRInputs,
		ADSROutputs
	);

	constexpr auto ExpADSRInputs = make_inputs(
		make_input("scale", &ExpADSR::scale),
		make_input(predef::AutoOnTime, &ExpADSR::curr_time),
		make_input(predef::AutoReleaseTime, &ExpADSR::off_time)
	);

	constexpr auto ExpADSROutputs = make_outputs(
		make_output("", &ExpADSR::output)
	);

	constexpr auto ExpADSRModule = make_module<ExpADSR>(
		"exponential_adsr_envelope",
		ExpADSRInputs,
		ExpADSROutputs
	);
}

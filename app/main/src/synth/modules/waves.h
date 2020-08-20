#pragma once
// These all have a fairly common interface of
//  - frequency
//  - amplitude
//  - dc_offset

#include "../module.h"

// Various core modules that implement boring waveforms.
//
// These all have a fairly common interface of
//  - frequency
//  - amplitude
//  - dc_offset

namespace ms::synth::mod {
	struct SqwWave {
		struct Cfg {
			bool inverted;
		};
		
		float frequency, amplitude, duty, dc_offset;
		float curr_time;
		float output;

		bool generate(const Cfg& config);

	private:
		float incstate;
	};

	struct TriangleWave {
		struct Cfg {
			bool inverted;
		};
		
		float frequency, amplitude, dc_offset;
		float curr_time;
		float output;

		bool generate(const Cfg& config);
	private:
		float incstate;
	};

	struct SinWave {
		struct Cfg {
			bool rectified;
			bool inverted;
		};

		float frequency, amplitude, dc_offset;
		float curr_time;
		float output;

		bool generate(const Cfg& config);
	private:
		float incstate;
	};

	static constexpr auto SqwInputs = make_inputs(
			make_input("frequency", &SqwWave::frequency),
			make_input("amplitude", &SqwWave::amplitude, 0.f, 1.f),
			make_input("duty", &SqwWave::duty, 0.f, 1.f),
			make_input("dc_offset", &SqwWave::dc_offset),
			make_input(predef::AutoOnTime, &SqwWave::curr_time)
	);
	static constexpr auto SqwOutputs = make_outputs(
			make_output("", &SqwWave::output)
	);

	static constexpr auto SqwModule = make_module<SqwWave>(
			"square_wave",
			SqwInputs,
			SqwOutputs
	);

	static constexpr auto TriInputs = make_inputs(
			make_input("frequency", &TriangleWave::frequency),
			make_input("amplitude", &TriangleWave::amplitude, 0.f, 1.f),
			make_input("dc_offset", &TriangleWave::dc_offset),
			make_input(predef::AutoOnTime, &TriangleWave::curr_time)
	);
	static constexpr auto TriOutputs = make_outputs(
			make_output("", &TriangleWave::output)
	);

	static constexpr auto TriModule = make_module<TriangleWave>(
			"triangle_wave",
			TriInputs,
			TriOutputs
	);

	static constexpr auto SinInputs = make_inputs(
			make_input("frequency", &SinWave::frequency),
			make_input("amplitude", &SinWave::amplitude, 0.f, 1.f),
			make_input("dc_offset", &SinWave::dc_offset),
			make_input(predef::AutoOnTime, &SinWave::curr_time)
	);
	static constexpr auto SinOutputs = make_outputs(
			make_output("", &SinWave::output)
	);

	static constexpr auto SinModule = make_module<SinWave>(
			"sin_wave",
			SinInputs,
			SinOutputs
	);
}

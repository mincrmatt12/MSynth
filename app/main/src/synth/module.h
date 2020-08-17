#pragma once

// _Modules_ are the key building block in MSynth. They directly correspond to the blocks when editing a patch.
//
// Modules have configuration. This configuration is provided to them through an arbitrary blob of their choosing. This configuration plus
// a potentially runtime-changing dynconfiguration blob is used to generate the output signal(s). 
// These output signals get routed to other dynconfiguration blobs via the input sytem.
//
// This includes the current playing pitch, for example, but could also be used to automate the pulse width with an LFO, for example.
//
// Modules return a single value, which is whether or not they think the note should end. 

#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdint.h>
#include <stddef.h>
#include <utility>

namespace ms::synth {
	struct ModuleInput {
		// The name of this input, as seen in the UI
		const char * name = nullptr;
		// Offset into the dynconfiguration blob that this input occupies
		uintptr_t offset = -1; // effectively castable to a (CfgBlock::*float), but really just offsetof(T, val)
		
		// Optional, if not supported set both to NaN
		float min{};
		float max{};
	};

	struct ModuleOutput {
		// The name of this output, as seen in the UI.
		const char * name = nullptr;
		
		// If these are not -1, they get the values of the corresponding input, and offset_enabled is set to 1 (it's a byte pointer)
		// These are offsets into the _configuration_ blob.
		uintptr_t offset_min = -1, offset_max = -1, offset_enabled = -1, offset = -1;
	};

	typedef bool (*ModuleProc)(void *block, const void *staticblock);

	struct ModuleBase {
		// The name as seen in the UI
		const char * name;
		// The procedure that actually evaluates 
		ModuleProc proc;
		// The size of the configuration blob
		size_t cfg_size;
		// The size of the dynconfiguration blob
		size_t dyncfg_size;
		// Number of inputs and outputs.
		size_t input_count, output_count;
		// Pointer to descriptors for inputs and outputs
		const ModuleInput * const inputs; 
		const ModuleOutput * const outputs;
	};

	namespace detail {
		template<typename Holds, size_t Length>
		struct InOutHolder {
			Holds data[Length];

			template<typename ...Proxies>
			constexpr InOutHolder(Proxies &&...p) : 
				data{std::forward<Proxies>(p)...}
			{}
		};

		template<typename Holds, size_t Length>
		struct EmptyHolder {
			const Holds *data{};
		};

		template<typename T> struct declval_helper { static T value; };

		// This ugliness will break under clang due to it being slightly more intelligent about
		// following the standard.
		template<typename T, typename Z>
		constexpr int offset_from_memberptr(Z T::*MPtr) {
			using TV = declval_helper<T>;
#ifdef __clang__
			return 0;
#else
			char for_sizeof[
				(char *)&(TV::value.*MPtr) -
				(char *)&TV::value
			];
			return sizeof(for_sizeof);
#endif
		}

		// Helper interface to make_module for simple things
		template<typename Impl>
		struct ModuleHelper {
			static bool proc(void *dyncfg, const void *cfg) {
				return reinterpret_cast<Impl *>(dyncfg)->generate(*(const typename Impl::Cfg *)cfg);
			}
		};
	}

	// Create a list of outputs
	constexpr auto make_outputs() {
		return detail::EmptyHolder<ModuleOutput, 0>();
	}

	// Create a list of inputs
	constexpr auto make_inputs() {
		return detail::EmptyHolder<ModuleInput, 0>();
	}

	// Create a list of inputs
	template<typename ...Ts>
	constexpr auto make_inputs(Ts&& ...ts) {
		return detail::InOutHolder<ModuleInput, sizeof...(Ts)>(ts...);
	}

	// Create a list of outputs
	template<typename ...Ts>
	constexpr auto make_outputs(Ts&& ...ts) {
		return detail::InOutHolder<ModuleOutput, sizeof...(Ts)>(ts...);
	}

	// Create an input with a name and pointer
	template<typename Cfg>
	constexpr auto make_input(const char * name, float Cfg::* ptr, float min, float max) {
		ModuleInput obj;
		obj.name = name;
		obj.offset = detail::offset_from_memberptr(ptr);
		obj.min = min;
		obj.max = max;
		return obj;
	}

	// Create an input with a name and pointer with undefined min/max
	template<typename Cfg>
	constexpr auto make_input(const char * name, float Cfg::* ptr) {
		return make_input<Cfg>(name, ptr, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN());
	}

	// Create an output with a name and output pointer
	template<typename Cfg>
	constexpr auto make_output(const char * name, float *Cfg::* ptr) {
		ModuleOutput obj;
		obj.name = name;
		obj.offset = detail::offset_from_memberptr(ptr);
		obj.offset_enabled = -1;
		obj.offset_min = -1;
		obj.offset_max = -1;
		return obj;
	}

	// Create an output with a name, output pointer and metadata references
	template<typename Cfg>
	constexpr auto make_output(const char * name, float *Cfg::* ptr, float Cfg::* ptr_min, float Cfg::* ptr_max,
			bool Cfg::* ptr_enabled) {
		ModuleOutput obj;
		obj.name = name;
		obj.offset = detail::offset_from_memberptr(ptr);
		obj.offset_enabled = detail::offset_from_memberptr(ptr_min);
		obj.offset_min = detail::offset_from_memberptr(ptr_max);
		obj.offset_max = detail::offset_from_memberptr(ptr_enabled);
		return obj;
	}

	// Create a module.
	//
	// The only template argument you should specify is a ModuleType.
	// A ModuleType is a type which contains a member typedef `Cfg`, which is the static configuration of your module. The dynamic configuration
	// is the ModuleType itself. The ModuleType should also contain a public member function 
	//
	// bool generate(const Cfg& config);
	//
	// which handles output generation.
	//
	// Configuration is handled with a UI callback, which is a member that takes a non-const reference to the cfg and should
	// open a useful configuration UI immediately on the UI stack.
	//
	// Pass to this function the name of the module and the input/output sets created with make_inputs/make_outputs.
	template<typename Module, template<typename, size_t> typename InHolder, template<typename, size_t> typename OutHolder, size_t InCount, size_t OutCount>
	constexpr ModuleBase make_module(
		const char *name,
		const InHolder<ModuleInput, InCount> &inputs,
		const OutHolder<ModuleOutput, OutCount> &outputs
	) {
		return ModuleBase{
			name,
			detail::ModuleHelper<Module>::proc,
			sizeof(typename Module::Cfg),
			sizeof(Module),
			InCount,
			OutCount,
			inputs.data,
			outputs.data
		};
	}

	// These constants can be used in place of a name to indicate an always connected input.
	namespace predef {
		inline const char * OnTime = reinterpret_cast<const char *>(0xffff0000);
		inline const char * Velocity = reinterpret_cast<const char *>(0xffff0001);
		inline const char * Frequency = reinterpret_cast<const char *>(0xffff0002);
		inline const char * ReleaseTime = reinterpret_cast<const char *>(0xffff0003);
	};

	/* Sample module: 
		namespace square {
			struct SqwWave {
				struct Cfg {
					int test;
				};
				
				float frequency, amplitude, duty;
				float * output;

				bool generate(const Cfg& config) {
					*this->output = 0.f;
					return true;
				}
			};

			static constexpr auto SqwInputs = make_inputs(
					make_input("frequency", &SqwWave::frequency),
					make_input("amplitude", &SqwWave::amplitude, 0.f, 1.f),
					make_input("duty", &SqwWave::duty, 0.f, 1.f)
			);
			static constexpr auto SqwOutputs = make_outputs(
					make_output("", &SqwWave::output)
			);

			static constexpr auto SqwModule = make_module<SqwWave>(
					"square",
					SqwInputs,
					SqwOutputs
			);
		}
	*/
}

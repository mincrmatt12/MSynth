#pragma once
// The core patch object
//
// This is the unserialized one which sits in memory and holds info.

#include "module.h"
#include <vector>
#include <memory>

namespace ms::synth {
	// Yes this class does a bunch of unique_ptr-ing, and will probably corrupt the heap at an annoying time; the solution?
	//
	// ~~download more ram, n00b~~
	// board rev 2 will put the main heap on a separate ram chip (along with the framebuffer, hopefully)
	class Patch {
	public:
		struct ModuleHolder;
	
		// TODO: this should probably be scaled for potential multi-channel synths
		const ModuleHolder *output_source;
		uint16_t            output_idx;
	private:
		std::vector<std::unique_ptr<ModuleHolder>> modules;

	public:
		// Represents a single input link
		struct ModuleLink {
			uint16_t target_idx, source_idx;
			const ModuleHolder *source;
		};

		// Represents a single instance of a module in a program/patch
		//
		// This is designed to be stored in a Patch and referred to (usually through a vector reference)
		// 
		// These also contain the data about how inputs are connected in a small heap member.
		struct ModuleHolder {
			const ModuleBase *mod;
			
			friend class Patch;

			ModuleHolder(const ModuleBase& mod) :
				mod(&mod) {

				configuration.reset(new uint32_t[(mod.cfg_size + 3) / 4]);
				dynamic_configuration.reset(new uint32_t[(mod.dyncfg_size + 3) / 4]);
			}

			ModuleHolder(const ModuleBase& mod, const void * config, const void * dynamic_config) :
				ModuleHolder(mod) {

				memcpy(configuration.get(), config, mod.cfg_size);
				memcpy(dynamic_configuration.get(), dynamic_config, mod.dyncfg_size);
			}

			// no constructor provided for initializing links since usually you need to init every single module before
			// setting up cross-refs.

			inline const auto&  get_links() const {return links;}

			std::unique_ptr<uint32_t[]> configuration;
			std::unique_ptr<uint32_t[]> dynamic_configuration;
		private:
			std::vector<ModuleLink>     links;
		};

		const ModuleHolder * add_module(std::unique_ptr<ModuleHolder>&& module) {
			modules.emplace_back(module);
			return modules.back().get();
		}

		inline const auto& all_modules() const {
			return modules;
		}

		bool link_modules(const ModuleHolder *src, const ModuleHolder *tgt, uint16_t output_idx, uint16_t input_idx);
		void remove_module(const ModuleHolder *&& mod);
	};

	namespace predef {
		inline const Patch::ModuleHolder *ModuleRefGlobalIn = reinterpret_cast<const Patch::ModuleHolder *>(0xffff0000);
		inline const Patch::ModuleHolder *ModuleRefGlobalOut = reinterpret_cast<const Patch::ModuleHolder *>(0xffff0001);

		inline const uint16_t GlobalInPitchIdx = 0;
		inline const uint16_t GlobalInVelocityIdx = 1;
		inline const uint16_t GlobalInOnTimeIdx = 2;
		inline const uint16_t GlobalInOffTimeIdx = 3;
	}
}

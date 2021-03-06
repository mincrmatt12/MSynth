#include "patch.h"

bool ms::synth::Patch::link_modules(const ModuleHolder *src, const ModuleHolder *tgt, uint16_t output_idx, uint16_t input_idx) {
	// First, check if we're targeting the global output
	if (tgt == predef::ModuleRefGlobalOut) {
		// TODO: multiple channels
		this->output_idx = output_idx;
		this->output_source = src;

		return true;
	}
	// Next, check try to find the target in us.
	for (auto& mod : modules) {
		if (mod.get() == tgt) {
			// Check if the link is already set and overwrite it
			
			for (auto& link : mod->links) {
				if (link.target_idx == input_idx) {
					// Overwrite and return

					link.source = src;
					link.source_idx = output_idx;

					return true;
				}
			}

			// Otherwise, just add a new link
			mod->links.push_back(ModuleLink{
				.target_idx = input_idx,
				.source_idx = output_idx,
				.source = src
			});

			return true;
		}
	}
	return false;
}

void ms::synth::Patch::remove_module(const ModuleHolder *&& rmv_mod) {
	// Go through and unlink anything that refers to this
	for (auto& mod : modules) {
		mod->links.erase(std::remove_if(mod->links.begin(), mod->links.end(), [&](const auto& x){
			return x.source == rmv_mod;
		}), mod->links.end());
	}
	// Remove module
	modules.erase(std::find_if(modules.begin(), modules.end(), [&](const auto& x){return x.get() == rmv_mod;}));
}
const ms::synth::Patch::ModuleHolder * ms::synth::Patch::add_module(std::unique_ptr<ModuleHolder>&& module) {
	modules.emplace_back(std::move(module));
	// Automatically link up any auto-names
	for (size_t i = 0; i < modules.back()->mod->input_count; ++i) {
		switch (modules.back()->mod->inputs[i].autoname) {
			case predef::AutoFrequency:
				link_modules(predef::ModuleRefGlobalIn, modules.back().get(), predef::GlobalInPitchIdx, i);
				break;
			case predef::AutoVelocity:
				link_modules(predef::ModuleRefGlobalIn, modules.back().get(), predef::GlobalInVelocityIdx, i);
				break;
			case predef::AutoOnTime:
				link_modules(predef::ModuleRefGlobalIn, modules.back().get(), predef::GlobalInOnTimeIdx, i);
				break;
			case predef::AutoReleaseTime:
				link_modules(predef::ModuleRefGlobalIn, modules.back().get(), predef::GlobalInOffTimeIdx, i);
				break;
			default:
				continue; // not an autoname
		}
	}
	return modules.back().get();
}

#pragma once
// Main UI base classes.
//
// These include: various helper utilities for interacting with the layout system; the base types for elements and screens (a utility and virtual interface, respectively)
//
// Actual UI elements are defined in the `el/` folder.

#include <stdint.h>
#include <type_traits>
#include <stddef.h>

#include "layout.h"
#include <msynth/util.h>

namespace ms::ui {
	// UI BASES

	// Base UI class -- all UIs must inherit from this (since it's used for maintaining a stack of UIs)
	
	struct UI { // note the default UI doesn't actually handle any events(!); the reasoning being that some UIs won't need keyboard, so we don't inherit at all.
		// Construct as appropriate (the default one setus up UI stack stuff)
		UI() {};
		// Destruct as appropriate (the default one cleans up event handling and restores the previous UI in the stack)
		//virtual ~UI() {};

		// Called whenever it's time to redraw; this is called _after_ the events are processed usually.
		virtual void draw() = 0;
	};

	// UI BASE HELPERS
	// 
	// These are various mixins that define the correct things for layout clauses
	
	// In general, focus numbers use 0 for "nothing focused"
	struct UIFocusMixin {
		uint16_t currently_focused = 0;

		// TODO: perhaps some "set focus"? it's probably going to go elsewhere though -- the plan
		// is for callbacks to manage it (or perhaps an arbitrary pointer, idk)
	};

	// UI ELEMENT HELPERS
	//
	// These are designed to help create the required typedefs for the layout system easily. The Trait system is largely left alone here,
	// but these classes do stuff like generate LayoutParams etc.
	//
	// There are also some default LayoutParams present. There's also some of the critically important 
	
	template<typename FlagWidth>
	struct ElementFlagsProviderImpl {
		const static inline size_t FlagDirty = 0;

		FlagWidth flags = 0;

		bool flag(size_t idx) {return flags & (1 << idx);}

	protected:
		void mark_dirty() {set_flag(FlagDirty, true);}
		void set_flag(size_t idx, bool val) {
			if (val) flags |= (1 << idx);
			else     flags &= ~(1 << idx);
		}
		void toggle_flag(size_t idx) {
			flags ^= (1 << idx);
		}
	};

	template<size_t FlagCount>
	using ElementFlagsProvider = ElementFlagsProviderImpl<std::conditional_t<(FlagCount > 8), std::conditional_t<(FlagCount > 16), uint32_t, uint16_t>, uint8_t>>;

	template<typename BBoxType>
	struct BasicLayoutParams {
		BBoxType bbox;

		using InsideTrait = l::traits::Inside<BasicLayoutParams<BBoxType>>;
		using NonInteractableTraitRequirement = l::traits::HasAdjustableBBox;

		constexpr BasicLayoutParams(const BBoxType& implicity) : bbox(implicity) {}
	};

	using BoxLayoutParams = BasicLayoutParams<Box>;

	namespace element {};
	namespace el = element;

	// GLOBAL RESOURCES (placed in ccmdata)
	extern CCMDATA const void * ui_16_font;
	extern CCMDATA const void * ui_24_font;
	extern CCMDATA const void * ui_32_font;
}

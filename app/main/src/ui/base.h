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
#include <msynth/draw.h>

namespace ms::ui {
	// UI BASES

	// Base UI class -- all UIs must inherit from this (since it's used for maintaining a stack of UIs)
	
	struct UI { // note the default UI doesn't actually handle any events(!); the reasoning being that some UIs won't need keyboard, so we don't inherit at all.
		// Destruct as appropriate
		virtual ~UI() {};

		// Called whenever it's time to redraw; this is called _after_ the events are processed usually.
		virtual void draw() = 0;

		// Force redraw (should set everything dirty -- see the helper "set dirty" method in the layoutmanager)
		virtual void force_dirty() {}

		// Fill in the background for a given area -- by default fills the given area with black.
		// Note this is also called as part of the initial setup for a UI externally
		virtual void draw_bg(const Box& box) {
			draw::rect(box.x, box.y, box.x + box.w, box.y + box.h, 0);
		}
	};

	// UI BASE HELPERS
	// 
	// These are various mixins that define the correct things for layout clauses
	
	// In general, focus numbers use 0 for "nothing focused"
	struct UIFocusMixin {
		uint16_t currently_focused = 0;
	};

	// UI ELEMENT HELPERS
	//
	// These are designed to help create the required typedefs for the layout system easily. The Trait system is largely left alone here,
	// but these classes do stuff like generate LayoutParams etc.
	//
	// There are also some default LayoutParams present. There's also some of the required public members, with some helper functions implemented.
	
	// Provide an appropriately sized flags member, as well as some helper methods.
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

	// Helper typedef to pick the right size for ElementFlagsProviderImpl
	//
	// Adds various helper methods and the flags attribute. Unless managed manually, all elements must inherit from this.
	template<size_t FlagCount>
	using ElementFlagsProvider = ElementFlagsProviderImpl<std::conditional_t<(FlagCount > 8), std::conditional_t<(FlagCount > 16), uint32_t, uint16_t>, uint8_t>>;

	// A very basic layout parameter skeleton which only provides the necessary 'bbox' attribute.
	//
	// If you don't use the provided InsideTrait, you should include the NonInteractableTraitRequirement to inform the layout engine
	// that we have a bounding box attribute.
	template<typename BBoxType>
	struct BasicLayoutParams {
		BBoxType bbox;

		using InsideTrait = l::traits::Inside<BasicLayoutParams<BBoxType>>;
		using NonInteractableTraitRequirement = l::traits::HasAdjustableBBox;

		constexpr BasicLayoutParams(const BBoxType& implicity) : bbox(implicity) {}
		constexpr BasicLayoutParams(const BBoxType& implicity, BasicLayoutParams&& ) : bbox(implicity) {} // allow re-initializing
	};

	// Alias for the above basic layout parameter for the common case of a bounding box.
	using BoxLayoutParams = BasicLayoutParams<Box>;

	// A mixin type that memorizes the name of the focus order attribute for you :)
	struct FocusLayoutParamsMixin {
		uint16_t focus_index;

		constexpr explicit FocusLayoutParamsMixin(uint16_t in) : focus_index(in) {}
	};

	// USEFUL ENUMS
	
	enum Align {
		AlignBegin,
		AlignCenter,
		AlignEnd
	};

	// Helper function to align something in a region
	inline constexpr int16_t align_in(int16_t dimension, int16_t space, const Align& method) {
		switch (method) {
			default:
			case AlignBegin:
				return 0;
			case AlignCenter:
				return (space / 2) - (dimension / 2);
			case AlignEnd:
				return space - dimension;
		}
	}

	namespace element {};
	namespace el = element;

	// GLOBAL RESOURCES 
	//
	// These are placed in CCM because reasons.
	extern CCMDATA const void * ui_16_font;
	extern CCMDATA const void * ui_24_font;
	extern CCMDATA const void * ui_32_font;
}

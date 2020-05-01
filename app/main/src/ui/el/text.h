#pragma once

#include "../base.h"
#include <msynth/util.h>
#include <msynth/draw.h>

namespace ms::ui::element {
	// A fairly basic text rendering component.
	//
	// Allows you to set the text color and content at runtime, and alignment options are provided through the parameters.
	struct Text : ElementFlagsProvider<1> {
		Text(const char * label, uint8_t color) : color(color), label(label) {
			set_flag(FlagDirty, true);
		}

		struct LayoutParams : BoxLayoutParams {
			Align horiz_align = AlignCenter, vert_align = AlignCenter;

			// TODO: font selection

			constexpr LayoutParams(const Box& box, Align horiz=AlignCenter, Align vert=AlignCenter) : BoxLayoutParams(box), horiz_align(horiz), vert_align(vert) {}
			constexpr LayoutParams(const Box& box, LayoutParams&& lp) : LayoutParams(box, lp.horiz_align, lp.vert_align) {}
		};

		using LayoutTraits = l::LayoutTraits<BoxLayoutParams::NonInteractableTraitRequirement, l::traits::IsTransparent>;

		void draw(const LayoutParams& lp); 
		void set_label(const char * text);
		void set_color(uint8_t color);

	private:
		uint8_t color;
		const char *label;
	};
}

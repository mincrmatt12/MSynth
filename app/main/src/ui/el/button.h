#pragma once

#include "../base.h"
#include <msynth/util.h>
#include <msynth/draw.h>

namespace ms::ui::element {
	// The "boring" button
	struct Button : ElementFlagsProvider<3> {
		const static inline size_t FlagPressed = 1;

		using Callback = util::FuncHolder<void>;

		Button(const char * label, Callback cb) :
			label(label),
			cb(cb) {

			set_flag(FlagDirty, true);
		}

		using LayoutParams = BoxLayoutParams;
		using LayoutTraits = l::LayoutTraits<BoxLayoutParams::InsideTrait, l::traits::MinPressure<40> /* todo some enabled check (IfFlag<FlagEnabled>) */, 
			  l::traits::ForceIf<l::traits::MinPressure<evt::TouchEvent::PressureRemovedTouch>>>;

		bool handle(const evt::TouchEvent& evt, const LayoutParams& lp) {
			if (evt.pressure == evt::TouchEvent::PressureRemovedTouch && flag(FlagPressed)) {
				toggle_flag(FlagPressed);
				mark_dirty();
				cb();
			}
			if (evt.pressure != evt::TouchEvent::PressureRemovedTouch && !flag(FlagPressed)) {
				toggle_flag(FlagPressed);
				mark_dirty();
				return true;
			}
			return false;
		}

		void draw(const LayoutParams& lp) {
			draw::outline(lp.bbox.x, lp.bbox.y, lp.bbox.x + lp.bbox.w, lp.bbox.y + lp.bbox.h, flag(FlagPressed) ? 0b11'01'11'01 : 0b11'10'10'10);
			draw::rect(lp.bbox.x + 1, lp.bbox.y + 1, lp.bbox.x + lp.bbox.w, lp.bbox.y + lp.bbox.h, 0b11'01'01'01);

			// center text
			int16_t cx = lp.bbox.x + lp.bbox.w / 2 - (draw::text_size(label, ui_16_font) / 2);
			int16_t cy = lp.bbox.y + lp.bbox.h / 2 + 5; // ascender height from top (13) - center of font (16/2 = 8)
			draw::text(cx, cy, label, ui_16_font, 0xff);
		}

	private:
		const char *label;
		Callback cb;
	};
}

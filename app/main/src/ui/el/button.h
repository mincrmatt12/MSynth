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
		using LayoutTraits = l::LayoutTraits<BoxLayoutParams::InsideTrait, l::traits::MouseTypes<evt::TouchEvent::StatePressed> /* todo some enabled check (IfFlag<FlagEnabled>) */, 
			  l::traits::ForceIf<l::traits::MouseTypes<evt::TouchEvent::StateReleased>>>;

		bool handle(const evt::TouchEvent& evt, const LayoutParams& lp); 
		void draw(const LayoutParams& lp); 

	private:
		const char *label;
		Callback cb;
	};
}

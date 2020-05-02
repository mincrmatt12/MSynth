#pragma once

#include "../base.h"
#include <msynth/util.h>
#include <msynth/draw.h>

namespace ms::ui::element {
	// The "boring" button
	struct Button : ElementFlagsProvider<4> {
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

	protected:
		const char *label;
		Callback cb;
	};

	// The focusable button.
	struct FocusableButton : Button {
		const static inline size_t FlagFocused = 3;

		using Button::Button;

		struct LayoutParams : BoxLayoutParams, FocusLayoutParamsMixin {
			constexpr LayoutParams(const Box& box, uint16_t idx) : BoxLayoutParams(box), FocusLayoutParamsMixin(idx) {}
			constexpr LayoutParams(const Box& box, LayoutParams&& lp) : BoxLayoutParams(box), FocusLayoutParamsMixin(lp) {}
		};

		using LayoutTraits = l::LayoutTraits<LayoutParams::InsideTrait, l::traits::MouseTypes<evt::TouchEvent::StatePressed>, l::traits::FocusEnable<l::traits::KeyTrait>, 
			  l::traits::PassiveFocus, l::traits::ForceIf<l::traits::MouseTypes<evt::TouchEvent::StateReleased>>>;

		bool handle(const evt::TouchEvent& evt, const LayoutParams& lp) {
			return Button::handle(evt, static_cast<BoxLayoutParams>(lp));
		}
		bool handle(const evt::KeyEvent& evt, const LayoutParams&);
		bool handle(const evt::FocusEvent& evt, const LayoutParams&);
		void draw(const LayoutParams& lp); 
	};
}

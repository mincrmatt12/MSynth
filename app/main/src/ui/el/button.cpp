#include "button.h"

bool ms::ui::el::Button::handle(const evt::TouchEvent& evt, const LayoutParams& lp) {
	if (evt.state == evt::TouchEvent::StateReleased && flag(FlagPressed)) {
		toggle_flag(FlagPressed);
		mark_dirty();
		cb();
	}
	else if (evt.state == evt::TouchEvent::StatePressed && !flag(FlagPressed)) {
		toggle_flag(FlagPressed);
		mark_dirty();
		return true;
	}
	return false;
}

void ms::ui::el::Button::draw(const LayoutParams& lp) {
	draw::outline(lp.bbox.x, lp.bbox.y, lp.bbox.x + lp.bbox.w, lp.bbox.y + lp.bbox.h, flag(FlagPressed) ? 0b11'01'11'01 : 0b11'10'10'10);
	draw::rect(lp.bbox.x + 1, lp.bbox.y + 1, lp.bbox.x + lp.bbox.w, lp.bbox.y + lp.bbox.h, flag(FlagPressed) ? 0b11'00'01'00 : 0b11'01'01'01);

	// center text
	int16_t cx = lp.bbox.x + lp.bbox.w / 2 - (draw::text_size(label, ui_16_font) / 2);
	int16_t cy = lp.bbox.y + lp.bbox.h / 2 + 5; // ascender height from top (13) - center of font (16/2 = 8)
	draw::text(cx, cy, label, ui_16_font, 0xff);
}

void ms::ui::el::FocusableButton::draw(const LayoutParams& lp) {
	Button::draw(lp);

	if (flag(FlagFocused)) {
		draw::dashed_outline(lp.bbox.x, lp.bbox.y, lp.bbox.x + lp.bbox.w, lp.bbox.y + lp.bbox.h, flag(FlagPressed) ? 0b11'00'10'00 : 0b11'11'11'11);
	}
}

bool ms::ui::el::FocusableButton::handle(const evt::KeyEvent& evt, const LayoutParams&) {
	if (evt.down == !flag(FlagPressed) && evt.key == periph::ui::button::ENTER) {
		toggle_flag(FlagPressed);
		mark_dirty();
		if (evt.down) cb();
		return true;
	}

	return false;
}

bool ms::ui::el::FocusableButton::handle(const evt::FocusEvent& evt, const LayoutParams&) {
	set_flag(FlagFocused, evt.is_focused);
	mark_dirty();
	return true;
}

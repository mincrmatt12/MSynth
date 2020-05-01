#include "text.h"

void ms::ui::element::Text::draw(const LayoutParams& lp) {
	// TODO: get the font and baseline
	
	const void * font = ui_16_font;

	// Honor alignment rules
	uint16_t width = draw::text_size(label, font);
	
	int16_t x = align_in(width, lp.bbox.w, lp.horiz_align);
	int16_t y = align_in(16, lp.bbox.h, lp.vert_align) + 13; // TODO: use baseline

	draw::text(lp.bbox.x + x, lp.bbox.y + y, label, font, color);
}

void ms::ui::element::Text::set_label(const char * text) {
	this->label = text;
	mark_dirty();
}

void ms::ui::element::Text::set_color(uint8_t color) {
	this->color = color;
	mark_dirty();
}

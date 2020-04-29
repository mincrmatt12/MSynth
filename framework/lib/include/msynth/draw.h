#pragma once
// draw.h -- various routines for drawing to the framebuffer

#include <stdint.h>
namespace draw {
	// GENERAL DRAWING ROUTINES

	// Fill the LCD with a color
	void fill(uint8_t color);

	// Draw a filled rectangle
	void rect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint8_t color);
	
	// Draw a line to the LCD
	void line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint8_t color);

	// Draw text to the LCD
	uint16_t /* end pos */ text(uint16_t x, uint16_t y, const char* text, const void * font, uint8_t color);

	uint16_t text_size(const char *text, const void * font);

	// Copy a block of rectangular pixels to a location on the display.
	void blit(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t * data);

	// Draw the border of a rectangle (1-pixel thick)
	void outline(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint8_t color);
}

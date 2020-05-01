#pragma once
// draw.h -- various routines for drawing to the framebuffer

#include <stdint.h>
namespace draw {
	// GENERAL DRAWING ROUTINES

	// Fill the LCD with a color
	void fill(uint8_t color);

	// Draw a filled rectangle
	void rect(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color);
	
	// Draw a line to the LCD
	void line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color);

	// Draw text to the LCD
	uint16_t /* end pos */ text(int16_t x, int16_t y, const char* text, const void * font, uint8_t color);

	uint16_t text_size(const char *text, const void * font);

	// Copy a block of rectangular pixels to a location on the display.
	void blit(int16_t x, int16_t y, uint16_t width, uint16_t height, const uint8_t * data);

	// Draw the border of a rectangle (1-pixel thick)
	void outline(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color);

	// BOUNDARY VARS (inited by default)
	extern int16_t min_x, max_x, min_y, max_y;

	// Helper RAII struct to set and restore the min,max_x,y vars above.
	struct StackLocalBoundary {
		StackLocalBoundary(int16_t new_min_x, int16_t new_max_x, int16_t new_min_y, int16_t new_max_y) {
			old_min_x = min_x; old_max_x = max_x; old_min_y = min_y; old_max_y = max_y;
			min_x = new_min_x; max_x = new_max_x; min_y = new_min_y; max_y = new_max_y;
		}

		StackLocalBoundary(const StackLocalBoundary& other) = delete;

		~StackLocalBoundary() {
			min_x = old_min_x; max_x = old_max_x; min_y = old_min_y; max_y = old_max_y;
		}

		// Old state
		int16_t old_min_x, old_max_x, old_min_y, old_max_y;
	};
}

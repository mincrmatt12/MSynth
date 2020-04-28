#pragma once
// Utilities for bounding boxes, circles, etc.

#include <stdint.h>
#include <stddef.h>

namespace ms::ui {
	// Inside function -- specialized for various types
		
	struct Box {
		int16_t x, y, w, h;
	};

	struct Circle {
		int16_t x, y, r;
	};

	bool inside(int16_t x, int16_t y, const Box& bound);
	bool inside(int16_t x, int16_t y, const Circle& bound);

	// These forms allow for checking bounding box stuff.
	bool inside(const Box& overlap, const Box& bound);
	bool inside(const Box& overlap, const Circle& bound);
}

#pragma once
// Utilities for bounding boxes, circles, etc.

#include <stdint.h>
#include <stddef.h>

namespace ms::ui {
	// Various geometry primitives
	struct Box {
		constexpr Box(int16_t x, int16_t y, int16_t w, int16_t h) : x(x), y(y), w(w), h(h) {}
		constexpr Box(int16_t x, int16_t y) : x(x), y(y) {}

		int16_t x, y, w=0, h=0;
	};

	struct Circle {
		constexpr Circle(int16_t x, int16_t y, int16_t radius) : x(x), y(y), r(radius) {}

		int16_t x, y;
		union {
			int16_t r, w, h; // These are used for compatibility with Box.
		};
	};

	// Inside function -- specialized for various types
	bool inside(int16_t x, int16_t y, const Box& bound);
	bool inside(int16_t x, int16_t y, const Circle& bound);

	// These forms allow for checking bounding box stuff.
	bool inside(const Box& overlap, const Box& bound);
	bool inside(const Box& overlap, const Circle& bound);
}

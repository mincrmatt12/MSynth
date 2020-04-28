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

	struct MultiBox {
		size_t amount;
		Box boxes[];
	};


	bool inside(int16_t x, int16_t y, const Box& bound);
	bool inside(int16_t x, int16_t y, const Circle& bound);
	bool inside(int16_t x, int16_t y, const MultiBox& bound);
}

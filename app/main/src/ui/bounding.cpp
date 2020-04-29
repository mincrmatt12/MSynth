#include "bounding.h"

namespace ms::ui {
	bool inside(int16_t x, int16_t y, const Box& box) {
		return x >= box.x && x < (box.x + box.w) &&
			   y >= box.y && y < (box.y + box.h);
	}

	bool inside(int16_t x, int16_t y, const Circle& c) {
		x -= c.x;
		y -= c.y;

		return (x*x + y*y) <= (c.r*c.r);
	}

	bool inside(const Box& b, const Circle& c) {
		Box repl{static_cast<int16_t>(b.x - c.r), static_cast<int16_t>(b.y - c.r), static_cast<int16_t>(b.w + c.r), static_cast<int16_t>(b.h + c.r)};
		return inside(c.x, c.y, repl);
	}

	bool inside(const Box& a, const Box& b) {
		if (a.x + a.w < b.x) return false;
		if (a.x > b.x + b.w) return false;

		if (a.y + a.h < b.y) return false;
		return !(a.y > b.y + b.h);
	}
}

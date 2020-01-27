#pragma once
// util.h - various common primitives throughout the API
#include <stdint.h>

namespace util {
	template<int N, int D>
	struct LowPassFilter {
		uint16_t operator()(int x, bool reset = false)
		{
			if (reset) {
				s = x;
				return x;
			}

			// Use integer arithmetic for weighted average
			s = (N * s + (D - N) * x + D / 2) / D;
			return s;
		}

	private:
		int s = 0;
	};

	void delay(uint32_t ms);
}

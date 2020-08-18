#include "util.h"
#include <gcem.hpp>

namespace ms::synth {
	namespace tables {
		struct SinTable {
			float data[4096];

			constexpr SinTable() {
				float pos = 0;
				for (int i = 0; i < 4096; ++i) {
					data[i] = gcem::sin(pos);
					pos += (M_PI/2.f)/4095.f;
				}
			}
		};

		constexpr auto sin_table = SinTable{};
	}

	float wavesin(float in) {
		if (in < 0.f) in += 1.f;
		if (in > 1.f) in -= 1.f;
		
		if (in > 0.5f) return -wavesin(in-0.5f);
		if (in > 0.25f) return wavesin(0.5f-in);
		int offset = in * (4095.f/0.25f);
		return tables::sin_table.data[offset];
	}
}

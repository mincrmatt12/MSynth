#include <algorithm>
#include <draw.h>
#include <lcd.h>
#include <string.h>
#include <stm32f4xx_ll_dma2d.h>

namespace draw {
	// GLOBAL STATE VARIABLES
	int16_t min_x = 0, max_x = 480, 
			min_y = 0, max_y = 272;

	// FONT ROUTINES & TYPEDEFS
	struct Kern {
		char a, b;
		int8_t offset;
	};

	struct Metrics {
		uint8_t width, height, stride_or_wordlen;
		int8_t advance, bearingX, bearingY;

		const uint8_t * glyph() const {return ((const uint8_t *)(this) + sizeof(Metrics));}
	};
	
	struct Font {
		inline const static uint8_t Compressed = 1;
		inline const static uint8_t Italic = 2;
		inline const static uint8_t Bold = 4;
		inline const static uint8_t HasKern = 8;

		char magic[4];
		uint8_t flags;
		uint16_t data_offset;
		uint16_t kern_length;
		uint16_t kern_offset;

		Font(const Font& other) = delete;

		template<uint8_t Has>
		inline bool is() const {
			return flags & Has;
		}

		const Kern* kern() const {return (const Kern *)(((const uint8_t *)(this)) + kern_offset);}
		const uint16_t metrics_offset(char c) const {return ((const uint16_t *)(((const uint8_t *)this) + data_offset))[c];}
		const Metrics* metrics(char c) const {return (const Metrics *)(((const uint8_t *)(this)) + metrics_offset(c));}

		bool search_kern(char a, char b, Kern& out) const {
			uint32_t start = 0, end = kern_length;
			uint16_t needle = (((uint16_t)a << 8) + (uint16_t)b);
			while (start != end) {
				uint32_t head = (start + end) / 2;
				uint16_t current = ((uint16_t)(kern()[head].a) << 8) + (uint16_t)(kern()[head].b);
				if (current == needle) {
					out = kern()[head];
					return true;
				}
				else {
					if (start - end == 1 || end - start == 1) {
						return false;
					}
					if (current > needle) {
						end = head;
					}
					else {
						start = head;
					}
				}
			}
			return true;
		};
	};

	void raw_write_glyph(int16_t x, int16_t y, const Metrics& metrics, uint8_t color) {
		for (int16_t cursor_y = std::max(min_y, y); cursor_y < y + metrics.height; ++y) {
			if (cursor_y >= max_y) break;
			for (int16_t cursor_x = std::max(min_x, x); cursor_x < x + metrics.width; ++x) {
				if (cursor_x >= max_x) break;
				// Lookup value in data
				uint8_t byte = (cursor_x - x) / 8;
				uint8_t bit =  (cursor_x - x) % 8;

				if (metrics.glyph()[(cursor_y - y) * metrics.stride_or_wordlen + byte] & (1 << bit))
					framebuffer_data[cursor_y][cursor_x] = color;
			}
		}
	}

	void decompress_write_glyph(int16_t x, int16_t y, const Metrics& metrics, uint8_t color) {
		// Helper function to get bit
		
		int bit = 8;
		const uint8_t *data = metrics.glyph();
		auto GetBit = [&]() -> uint32_t {
			if (bit == 0) {
				bit = 8;
				++data;
			}
			--bit;
			return (*data & (1 << bit)) >> bit;
		};

		int16_t cursor_x = x, cursor_y = y;
		int total = 0;

		bool rowbuf[metrics.width * 2]; // rowbuf1 gets filled, copied to rowbuf0

		auto WriteBit = [&](bool bit){
			++total;
			rowbuf[cursor_x - x] = bit;
			if (bit) {
				if (cursor_y >= max_y || cursor_y < min_y || cursor_x >= max_x || cursor_x < min_x) goto next;
				framebuffer_data[cursor_y][cursor_x] = color;
			}
next:
			++cursor_x;
			if (cursor_x == x + metrics.width) {
				cursor_x = x;
				++cursor_y;

				memcpy(rowbuf + metrics.width, rowbuf, metrics.width);
			}
		};

		while (total < (metrics.width * metrics.height)) {
			// Read two-bit command
			auto cmd = GetBit() << 1 | GetBit();
			if (total == 0 && cmd == 0) {
				// UNCOMPRESSED_RAW
				while (total < (metrics.width * metrics.height)) {
					WriteBit(GetBit());
				}
				return;
			}
			else if (cmd == 0) {
				// Repetition command
				auto cmd_2 = GetBit();
				if (cmd_2) {
					// REPEAT_ROW
					for (int i = 0; i < metrics.width; ++i)
						WriteBit(rowbuf[i + metrics.width]);
				}
				else {
					// REPEAT_WORD
					int word_index = ((cursor_x - x) / metrics.stride_or_wordlen) * metrics.stride_or_wordlen;
					for (int i = word_index; i < (word_index + metrics.stride_or_wordlen); ++i) {
						WriteBit(rowbuf[i + metrics.width]);
					}
				}
			}
			else if (cmd == 0b10) {
				// COPY_ROW
				for (int i = 0; i < metrics.width; ++i) WriteBit(GetBit());
			}
			else if (cmd == 0b11) {
				// ZERO_WORD
				for (int i = 0; i < metrics.stride_or_wordlen; ++i) WriteBit(0);
			}
			else if (cmd == 0b01) {
				// COPY_WORD
				for (int i = 0; i < metrics.stride_or_wordlen; ++i) WriteBit(GetBit());
			}
		}
	}

	uint16_t /* end pos */ text(int16_t x, int16_t y, const char* text, const void * font_, uint8_t color) {
		uint16_t pen = x;
		const Font& font = *(const Font *)(font_);

		uint8_t c, c_prev = 0;
		while ((c = *(text++)) != 0) {
			if (c_prev != 0 && font.is<Font::HasKern>()) {
				Kern dat;
				if (font.search_kern(c_prev, c, dat)) {
					pen += dat.offset;
				}
			}
			c_prev = c;
			if (font.metrics_offset(c) == 0) continue; // unknown char
			const auto& metrics = *font.metrics(c);
			if (c != ' ') {
				if (font.is<Font::Compressed>()) {
					decompress_write_glyph(pen + metrics.bearingX, y - metrics.bearingY, metrics, color);
				}
				else {
					raw_write_glyph(pen + metrics.bearingX, y - metrics.bearingY, metrics, color);
				}
			}

			pen += metrics.advance;
		}

		return pen;
	}

	uint16_t text_size(const char * text, const void * font_) {
		uint16_t pen = 0;
		const Font& font = *(const Font *)(font_);

		uint8_t c, c_prev = 0;
		while ((c = *(text++)) != 0) {
			if (c_prev != 0 && font.is<Font::HasKern>()) {
				Kern dat;
				if (font.search_kern(c_prev, c, dat)) {
					pen += dat.offset;
				}
			}
			c_prev = c;
			if (font.metrics_offset(c) == 0) continue; // unknown char
			const auto& metrics = *font.metrics(c);
			pen += metrics.advance;
		}

		return pen;
	}

	// DRAWING ROUTINES
	void fill(uint8_t color) {
		memset(framebuffer_data, color, sizeof(framebuffer_data));
	}

	void rect(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color) {
		if (y0 >= max_y) return;
		x0 = std::min(std::max(min_x, x0), max_x);
		x1 = std::min(std::max(min_x, x1), max_x);
		for (int16_t y = std::max(y0, min_y); y < std::min<int16_t>(max_y-1, y1); ++y) {
			memset(&framebuffer_data[y][x0], color, x1 - x0);
		}
	}

	void blit(int16_t x, int16_t y, uint16_t w, uint16_t h, const uint8_t * data) {
		size_t stride = (x + w) >= max_x ? (max_x - x) : w;
		if (x < min_x) {
			data += min_x - x;
			stride -= min_x - x;
			x = min_x;
		}
		for (int16_t i = 0; i < h; ++i) {
			if (y >= min_y && y < max_y) {
				memcpy(&framebuffer_data[y][x], data, stride);
			}
			++y;
			data += w;
		}
	}

	void outline(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color) {
		for (int y = y0; y < y1; ++y) {
			if (y < min_y || max_y <= y) continue;
			if (x0 >= min_x && max_x > x0) framebuffer_data[y][x0] = color;
			if (x1 >= min_x && max_x > x1) framebuffer_data[y][x1] = color;
		}

		for (int x = x0; x < x1; ++x) {
			if (x < min_x || max_x <= x) continue;
			if (y0 >= min_y && max_y > y0) framebuffer_data[y0][x] = color;
			if (y1 >= min_y && max_y > y1) framebuffer_data[y1][x] = color;
		}
	}
}

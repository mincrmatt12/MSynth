#include <draw.h>
#include <lcd.h>
#include <string.h>

namespace draw {
	// FONT ROUTINES & TYPEDEFS
	struct Kern {
		char a, b;
		int16_t offset;
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

	void raw_write_glyph(uint16_t x, uint16_t y, const Metrics& metrics, uint8_t color) {
		for (uint16_t cursor_y = y; cursor_y < y + metrics.height; ++y) {
			for (uint16_t cursor_x = x; cursor_x < x + metrics.width; ++x) {
				// Lookup value in data
				uint8_t byte = (cursor_x - x) / 8;
				uint8_t bit =  (cursor_x - x) % 8;

				if (metrics.glyph()[(cursor_y - y) * metrics.stride_or_wordlen + byte] & (1 << bit))
					framebuffer_data[cursor_y][cursor_x] = color;
			}
		}
	}

	void decompress_write_glyph(uint16_t x, uint16_t y, const Metrics& metrics, uint8_t color) {
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

		uint16_t cursor_x = x, cursor_y = y;
		int total = 0;

		bool rowbuf[metrics.width * 2]; // rowbuf1 gets filled, copied to rowbuf0

		auto WriteBit = [&](bool bit){
			++total;
			rowbuf[cursor_x - x] = bit;
			if (bit) {framebuffer_data[cursor_y][cursor_x] = color;}
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

	uint16_t /* end pos */ text(uint16_t x, uint16_t y, const char* text, const void * font_, uint8_t color) {
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
};

#include "mgr.h"

namespace ms::ui::mgr {
	UI *ui_stack[8] = {};
	uint8_t to_close_bitfield = 0;

	// Open a UI
	void open(UI *ui) {
		// Append to the end of the stack
		for (int pos = 0; pos < 8; ++pos) {
			if (ui_stack[pos]) continue;
			ui_stack[pos] = ui;
			return;
		}
	}

	// Close a UI (doesn't do eh because oh does it)
	void close(UI *ui) {
		// Locate it
		for (int pos = 0; pos < 8; ++pos) {
			if (ui_stack[pos] == ui) {
				// Clear forwards
				to_close_bitfield |= ~(uint8_t)(((uint8_t)1 << pos) - 1);
				// TODO: should there be an "on_close"?
				return;
			}
		}
	}

	void draw() {
		// Check if we need to clear things
		if (to_close_bitfield) {
			for (int pos = 0x80, i = 7; pos; pos >>= 1, --i) {
				if (to_close_bitfield & pos) {
					delete ui_stack[i];
					ui_stack[i] = nullptr;
				}
			}
		}

		for (int i = 0; i < 8; ++i) {
			// If this is the topmost entry
			if (i == 7 || ui_stack[i+1] == nullptr) {
				if (to_close_bitfield) {
					// Redraw the BG if we just closed stuff
					ui_stack[i]->draw_bg(Box(0, 0, 480, 272));
					ui_stack[i]->force_dirty();

					// Actually finish closing
					to_close_bitfield = 0;
				}
				ui_stack[i]->draw();
			}
		}
	}
}

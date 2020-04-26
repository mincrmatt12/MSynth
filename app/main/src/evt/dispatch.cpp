#include "dispatch.h"

// Event handlers

namespace ms::evt {
	const static inline int handler_count = 16;

	OpaqueHandler *handlers[handler_count] = {};

	OpaqueHandler::OpaqueHandler() {
		for (int i = 0; i < handler_count; ++i) {
			if (!handlers[i]) {
				handlers[i] = this;
				return;
			}
		}

		// should never happen -- error handler here
	}

	OpaqueHandler::~OpaqueHandler() {
		for (int i = 0; i < handler_count; ++i) {
			if (handlers[i] == this) {
				handlers[i] = nullptr;
				return;
			}
		}
	}

	void dispatch(const void *opaque, int id) {
		for (int i = 0; i < handler_count; ++i) {
			if (handlers[i]) handlers[i]->dispatch(opaque, id);
		}
	}
};

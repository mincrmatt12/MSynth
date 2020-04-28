#include "dispatch.h"

// Event handlers

namespace ms::evt {
	const static inline int handler_count = 16;

	OpaqueHandler *handlers[handler_count] = {};

	void add(OpaqueHandler *handler) {
		for (int i = 0; i < handler_count; ++i) {
			if (!handlers[i]) {
				handlers[i] = handler;
				return;
			}
		}

		// should never happen -- error handler here
	}

	void remove(OpaqueHandler *handler) {
		for (int i = 0; i < handler_count; ++i) {
			if (handlers[i] == handler) {
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

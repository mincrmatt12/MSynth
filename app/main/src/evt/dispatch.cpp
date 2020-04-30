#include "dispatch.h"

// Event handlers

namespace ms::evt {
	const static inline int handler_count_global = 8;
	const static inline int handler_count_ui = 8;

	OpaqueHandler *handlers_global[handler_count_global] = {};
	OpaqueHandler *handlers_ui[handler_count_ui] = {};

	template<OpaqueHandler ** ptr, size_t count>
	inline void add_impl(OpaqueHandler *handler) {
		for (int i = count - 1; i >= 0; --i) { // add from end for the purpose of ui stacks
			if (!ptr[i]) {
				ptr[i] = handler;
				return;
			}
		}

		// should never happen -- error handler here
	}

	void add(OpaqueHandler *handler) {
		add_impl<handlers_global, handler_count_global>(handler);
	}

	void add_ui(OpaqueHandler *handler) {
		add_impl<handlers_ui, handler_count_ui>(handler);
	}

	void remove(OpaqueHandler *handler) {
		for (int i = 0; i < handler_count_global; ++i) {
			if (handlers_global[i] == handler) {
				handlers_global[i] = nullptr;
				return;
			}
		}
		for (int i = 0; i < handler_count_ui; ++i) {
			if (handlers_ui[i] == handler) {
				handlers_ui[i] = nullptr;
				return;
			}
		}
	}

	void dispatch(const void *opaque, int id) {
		for (int i = 0; i < handler_count_global; ++i) {
			if (handlers_global[i]) handlers_global[i]->dispatch(opaque, id);
		}
		for (int i = 0; i < handler_count_ui; ++i) {
			if (handlers_ui[i]) if (handlers_ui[i]->dispatch(opaque, id)) return;
		}
	}
};

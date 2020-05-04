#pragma once
// UI manager
//
// Keeps track of the open UI stack in its list of 8 pointers. A UI can elect to open a child UI.

#include "base.h"
#include "../evt/dispatch.h"

namespace ms::ui::mgr {
	// Open a UI. It will go to the top of the UI stack, and whether or not it dims the background/etc is
	// up to it. The pointer must be on the heap, and will not have events registered.
	void open(UI* to_open);

	// Open a UI, and try to register it as an event handler. This is only done if the given type can be converted to an OpaqueHandler, hence the templating.
	//
	// The new UI to the top of the UI stack, and whether or not it dims the background/etc is
	// up to it.
	//
	// The pointer must be on the heap
	template<typename T>
	std::enable_if_t<!std::is_same_v<std::decay_t<T>, UI> && std::is_base_of_v<UI, T>> open(T* to_open) {
		if constexpr (std::is_base_of_v<evt::OpaqueHandler, T>) {
			evt::add_ui(to_open);
		}
		open((UI*)to_open);
	}

	// Close a specific UI. If it is not the topmost UI, it and all UIs above it are closed as well, in stackwise order.
	//
	// It is safe to use this from within a UI, as it only marks the UI for closing in a bitfield.
	void close(UI* to_close);

	// Call the correct draw function for the UI stack
	void draw();
}



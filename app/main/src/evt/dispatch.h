#pragma once

// The core part of the event system, containing all the base definitions for events.
// To save some RAM, all UI screens are placed onto the heap. They form a stack (for dialogs), and only
// the topmost one sticks around.
//
// Other stuff (like buffers) go onto RAM, and song data is stored wherever (TODO)
//
// Because the event listeners are constructed and deconstructed at semantically relevant times, however, we can use some
// fairly simple logic to handle event processing, based on constructors.
//
// Events are source-agnostic representations of interactions. For example:
// 	- TouchEvent: touchscreen/mouse (abusing LTDC layer 2 mouse cursor is handled by the input code)
// 		- pressure, x, y
// 	- KeyEvent: button press/hid keyboard
// 		- state (down, up, press), key-id (translated if applicable; key combinations are handled separately)
// 	- etc. (various state events so the UI/system can be informed of stuff)
//
// UI's must handle some subset of them by implementing EventHandler<SomeEvents>
//
// This exposes some internal stuff which allows the resulting type to be used by the event handling system.

#include <cstdio>
#include <stdint.h>
#include <type_traits>

namespace ms::evt {
	// All events must have a const static public member named "id" which is a unique integer.
	//
	// These form a bitmask of up to 32 entries.
	
	// EVENT DISPATCH

	template<typename Event>
	constexpr static inline bool is_event_v = std::is_integral_v<decltype(Event::id)> && Event::id < 32;

	// Dispatch an event.
	template<typename Event>
	inline std::enable_if_t<is_event_v<Event>> dispatch(const Event& evt) {
		dispatch(&evt, Event::id);
	}

	// Internal dispatcher
	void dispatch(const void *opaque, int id);

	// Helper declaration to convert parameter pack to a bitmask of events
	template<typename ...Events>
	constexpr static inline std::enable_if_t<(is_event_v<Events> && ...), uint32_t> events_to_bitmask = ((1 << Events::id) | ...);

	// EVENT HANDLING

	// Internal helper to encapsulate the correct inheritance
	template<typename Event>
	struct callback_holder {
		virtual bool handle(const Event& evt) = 0;
	};

	// Advanced event handling, allows for custom opaque processing. EventHandler does this for you.
	// All event handlers must be convertable to this.
	struct OpaqueHandler {
		~OpaqueHandler();
		friend void ::ms::evt::dispatch(const void *, int);

	protected:
		virtual bool dispatch(const void *opaque, int id) = 0;
	};

	// Base event handler type
	template<typename ...ListeningFor>
	struct EventHandler : public OpaqueHandler, protected callback_holder<ListeningFor>... {
		static const inline uint32_t bitmask = events_to_bitmask<ListeningFor...>;
	
	protected:
		using callback_holder<ListeningFor>::handle...; // required to get around hiding rules

	private:
		bool dispatch(const void *opaque, int id) final {
			if (!((1 << (uint32_t)id) & bitmask)) return false;
			return ((id == ListeningFor::id && this->handle(*reinterpret_cast<const ListeningFor *>(opaque))) || ...);
		}
	};

	// REGISTRATION

	// Register a handler
	void add(OpaqueHandler *oh);
	void add_ui(OpaqueHandler *oh);

	// Unregister a handler
	//
	// Silently fails if the given handler isn't registered (i.e. it's safe to call multiple times)
	void remove(OpaqueHandler *oh);
}

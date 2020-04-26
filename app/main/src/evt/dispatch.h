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

#include <stdint.h>
#include <type_traits>
#include <msynth/periphcfg.h>

namespace ms::evt {
	// All events must have a const static public member named "id" which is a unique integer.
	//
	// These form a bitmask of up to 32 entries.
	
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
	constexpr static inline std::enable_if_t<std::conjunction_v<is_event_v<Events>...>, uint32_t> events_to_bitmask = (Events::id | ...);

	// Internal helper to encapsulate the correct inheritance
	template<typename Event>
	struct callback_holder {
		virtual void handle(const Event& evt) = 0;
	};

	// Advanced event handling, allows for custom opaque processing. EventHandler does this for you.
	// All event handlers must be convertable to this.
	struct OpaqueHandler {
		friend void ::ms::evt::dispatch(const void *, int);

		OpaqueHandler();
		~OpaqueHandler();
		OpaqueHandler(const OpaqueHandler& other) = delete;
		OpaqueHandler(OpaqueHandler&& other) {};

	protected:
		virtual void dispatch(const void *opaque, int id) = 0;
	};

	// Base event handler type
	template<typename ...ListeningFor>
	struct EventHandler : OpaqueHandler, protected callback_holder<ListeningFor>... {
		static const inline uint32_t bitmask = events_to_bitmask<ListeningFor...>;

	private:
		void dispatch(const void *opaque, int id) override {
			if (!((1 << id) & bitmask)) return;
			((id == ListeningFor::id && (handle(*reinterpret_cast<const ListeningFor *>(opaque)), true)) || ...);
		}
	};

	// EVENT DEFINITIONS
	
	struct TouchEvent {
		const static inline int id = 1;

		int16_t x, y, pressure;
	};

	struct KeyEvent {
		const static inline int id = 2;
		
		bool down; // false for up
		periph::ui::button key;	// todo: expandability
	};

	struct VolumeEvent {
		const static inline int id = 3;

		int8_t volume; // this is already ran through a nice low-pass filter, and is already set elsewhere, but it can be useful to show it on screen.
	};

	struct MidiEvent {
		// These are _parsed_ events -- and are importantly source agnostic (specifically, they don't care between USB/hw uart)

		const static inline int id = 4;

		enum {
			TypeNoteOn,
			TypeNoteOff,
			TypePitchBend,
			TypeAftertouch,
			TypeControl,
			TypeProgramChange,
			TypeSysex
		} type;
		
		struct NoteEvt {
			uint8_t note;
			uint8_t velocity;
		};

		struct ControllerEvt {
			uint8_t control;
			uint8_t value;
		};

		struct AftertouchEvt {
			const static inline uint8_t All = 0xff;

			uint8_t note;
			uint8_t amount;
		};
		
		struct ProgramChangeEvt {
			uint8_t pc;
		};

		struct PitchBendEvt {
			int16_t amount;
		};

		struct SysExEvt {
			std::size_t size;
			char * buf;
		};

		union {
			NoteEvt note;
			ControllerEvt controller;
			AftertouchEvt aftertouch;
			PitchBendEvt pitchbend;
			ProgramChangeEvt programchange;
			SysExEvt sysex;
		};
	};

	struct DeviceStateEvent {
	};
}

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <msynth/periphcfg.h>

namespace ms::evt {
	// EVENT DEFINITIONS
	
	struct TouchEvent {
		const static inline int id = 1;

		int16_t x, y, pressure;
		enum {
			StatePressed = 1,
			StateDragged = 2,
			StateReleased = 4
		} state;
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
			size_t size;
			const char * buf;
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
		// Triggers when devices are connected/removed

		const static inline int id = 5;

		enum {
			SourceUSB,
			SourceMIDI,
			SourceDisconnected
		} source;

		enum {
			InputMIDI,
			InputMouse,
			InputKey
		} target;

		const char * name;
		const char * vendor;
	};

	struct FocusEvent {
		// Virtual event; only used internally within the UI system, which doesn't have polymorphic handling.

		bool is_focused;
	};
}

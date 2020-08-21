#pragma once
// This is a really basic thingy that handles channel allocation for a single patch
//
// It's used in the playback mode (and while recording over an existing track in tandem with the ChannelAllocated one)
//
// It follows RAII principles and takes in a patch, converts it to a Program, holds onto the program and dynamically creates/finalizes
// the various voices. Unlike a proper channel based setup, there's no per-channel panning or effects, rather the entire object
// handles stereo stuff and effect buffers.
//
// TODO: well i mean effects, layering, literally everything :)

#include "program.h"
#include "playback.h"
#include "../evt/dispatch.h"
#include "../evt/events.h"
#include "util.h"

#include <cmsis_gcc.h>

namespace ms::synth::playback {

	template<size_t Channels> // this could be heap'd, but at that point i'm going to be fragmenting everything
	struct LivePlayback : evt::EventHandler<evt::MidiEvent>, AudioGenerator {
		LivePlayback(const Patch &patch) :
			patch(patch)
		{}

		bool handle(const evt::MidiEvent& evt) override {
			switch (evt.type) {
				case evt::MidiEvent::TypeNoteOn:
					if (evt.note.velocity) {
						printf("start note %d\n", evt.note.note);
						start_note(midi_to_freq(evt.note.note), static_cast<float>(evt.note.velocity) / 0x7f);
						break;
					}
					// some keyboards never send note off
				case evt::MidiEvent::TypeNoteOff:
					printf("end note %d\n", evt.note.note);
					end_note(midi_to_freq(evt.note.note));
					break;
				case evt::MidiEvent::TypePitchBend:
					// bend 
					{
						float semitones = 2.f;
						if (evt.pitchbend.amount < 128)
							semitones = static_cast<float>(evt.pitchbend.amount - 64) / 32.f;
						pitch_bend_offset = offset_scale_from_semitones(semitones);
						for (int i = 0; i < Channels; ++i) {
							if (voices[i])
								voices[i]->set_pitch(voices[i]->pitch() * pitch_bend_offset);
						}
					}
				default:
					break;
			}

			return true;
		}
		int16_t generate() override {
			int16_t total = 0;
			for (size_t i = 0; i < Channels; ++i) {
				if (cut_voices[i] && voices[i]->released_time() != -1.f) continue;
				if (voices[i]) {
					total = __QADD16(total, voices[i]->generate(cut_voices[i]) / (int16_t)Channels);
				}
				if (cut_voices[i] && (voices[i]->released_time() == -1.f)) cut_voices[i] = false;
			}
			return total;
		}

		void update() {
			for (int i = 0; i < Channels; ++i) {
				if (cut_voices[i] && voices[i]) {
					delete voices[i];
					voices[i] = nullptr;
				}
			}
		}
		
	private:
		void start_note(float pitch, float velocity) {
			// Find an open slot
			size_t i;
			for (i = 0; i < Channels; ++i) {
				if (cut_voices[i] || !voices[i]) {
					if (!voices[i]) voices[i] = patch.new_voice();
					// Re-init
init:
					voices[i]->reset_time();
					voices[i]->set_pitch(pitch);
					voices[i]->set_pitch(pitch*pitch_bend_offset);
					voices[i]->set_velocity(velocity);
					cut_voices[i] = false;
					return;
				}
			}
			i = 0;
			// Otherwise, replace the earliest one
			for (size_t j = 0; j < Channels; ++j) {
				if (voices[j]->held_time() > voices[i]->held_time()) i = j;
			}
			goto init;
		}
		void end_note(float pitch) {
			for (size_t i = 0; i < Channels; ++i) {
				if (voices[i]->pitch() == pitch) {
					voices[i]->mark_off();
				}
			}
		}

		Program patch;
		Voice*  voices[Channels]{};
		bool    cut_voices[Channels]{};
		float pitch_bend_offset=1.f;
	};
}

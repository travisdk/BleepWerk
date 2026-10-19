// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// Turns the host's MIDI into notes for the engines: note-on and note-off, the sustain pedal,
// all-notes-off and pitch bend, on every channel. Mono mode's last-note priority lives in the
// VoicePool, which gets the key presses and releases in order from here. JUCE-free and
// allocation-free, so it runs on the audio thread and in the tests.
//
// The engines object provides noteOn (int note, float velocity), noteOff (int note),
// allNotesOff() and pitchBend (float amount), with amount -1..1 of the bend range.
namespace bleepforge
{
class NoteInput
{
public:
    // Forgets every key, the pedal and the bend, without telling the engines (they are reset
    // too when this is called from prepareToPlay).
    void reset()
    {
        keyDown.fill (false);
        sustained.fill (false);
        pedalDown = false;
        bend = 0.0f;
    }

    float currentBend() const { return bend; }

    // One MIDI message: status byte, then up to two data bytes. Anything else is ignored.
    template <typename Engines>
    void handle (const uint8_t* data, int size, Engines& engines)
    {
        if (size < 1)
            return;
        const int type = data[0] & 0xf0;
        const int d1 = size > 1 ? data[1] & 0x7f : 0;
        const int d2 = size > 2 ? data[2] & 0x7f : 0;

        if (type == 0x90 && d2 > 0)
        {
            keyDown[(size_t) d1] = true;
            sustained[(size_t) d1] = false; // the key holds it again
            engines.noteOn (d1, (float) d2 / 127.0f);
        }
        else if (type == 0x80 || type == 0x90) // a note-on with velocity 0 is a note-off
        {
            keyDown[(size_t) d1] = false;
            if (pedalDown)
                sustained[(size_t) d1] = true;
            else
                engines.noteOff (d1);
        }
        else if (type == 0xe0)
        {
            // 8192 is the centre; both ends reach the full range, though the top is one step short
            const int value = d1 | (d2 << 7);
            bend = (float) (value - 8192) / (value >= 8192 ? 8191.0f : 8192.0f);
            engines.pitchBend (bend);
        }
        else if (type == 0xb0)
        {
            handleController (d1, d2, engines);
        }
    }

private:
    template <typename Engines>
    void handleController (int controller, int value, Engines& engines)
    {
        switch (controller)
        {
            case 64: // sustain pedal
                if (value >= 64)
                    pedalDown = true;
                else
                    releasePedal (engines);
                break;

            case 121: // reset all controllers: pedal up and the bend back to the centre
                releasePedal (engines);
                bend = 0.0f;
                engines.pitchBend (0.0f);
                break;

            case 120: // all sound off
            case 123: // all notes off, also what hosts send as a panic: the pedal lets go too
                keyDown.fill (false);
                sustained.fill (false);
                pedalDown = false;
                engines.allNotesOff();
                break;

            default:
                break;
        }
    }

    // Ends the notes the pedal held whose keys are up by now.
    template <typename Engines>
    void releasePedal (Engines& engines)
    {
        pedalDown = false;
        for (size_t note = 0; note < sustained.size(); ++note)
        {
            if (sustained[note] && ! keyDown[note])
                engines.noteOff ((int) note);
            sustained[note] = false;
        }
    }

    std::array<bool, 128> keyDown {}, sustained {};
    bool pedalDown = false;
    float bend = 0.0f; // -1..1
};
} // namespace bleepforge

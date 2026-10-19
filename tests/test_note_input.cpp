// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

// MIDI input: note-on and note-off (also as velocity 0), the sustain pedal, all-notes-off,
// reset-controllers and pitch bend; and, played through a real Bleep engine in Mono mode,
// last-note priority: letting go of the newest key returns to the one still held.
#include "NoteInput.h"
#include "BleepSynth.h"
#include "TestUtils.h"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace bleepforge;

constexpr double fs = 48000.0;

static int ms (double milliseconds) { return (int) (milliseconds * 0.001 * fs); }

// Writes down what NoteInput asks the engines to do.
struct Recorder
{
    enum class Kind { On, Off, AllOff, Bend };
    struct Event
    {
        Kind kind;
        int note;
        float value;
        bool operator== (const Event& other) const
        {
            return kind == other.kind && note == other.note && std::abs (value - other.value) < 1.0e-4f;
        }
    };
    std::vector<Event> events;

    void noteOn (int note, float velocity) { events.push_back ({ Kind::On, note, velocity }); }
    void noteOff (int note) { events.push_back ({ Kind::Off, note, 0.0f }); }
    void allNotesOff() { events.push_back ({ Kind::AllOff, -1, 0.0f }); }
    void pitchBend (float amount) { events.push_back ({ Kind::Bend, -1, amount }); }
};

using Kind = Recorder::Kind;

static void send (NoteInput& input, Recorder& r, uint8_t status, uint8_t d1, uint8_t d2)
{
    const uint8_t data[] { status, d1, d2 };
    input.handle (data, 3, r);
}

static void notesOnAndOff()
{
    NoteInput input;
    Recorder r;
    send (input, r, 0x90, 60, 127);
    send (input, r, 0x93, 64, 64); // any channel
    send (input, r, 0x80, 60, 0);
    send (input, r, 0x93, 64, 0); // a note-on at velocity 0 is a note-off
    const std::vector<Recorder::Event> expected { { Kind::On, 60, 1.0f }, { Kind::On, 64, 64.0f / 127.0f },
                                                  { Kind::Off, 60, 0.0f }, { Kind::Off, 64, 0.0f } };
    CHECK (r.events == expected);

    // short or unknown messages do nothing
    r.events.clear();
    const uint8_t clock[] { 0xf8 };
    input.handle (clock, 1, r);
    input.handle (clock, 0, r);
    send (input, r, 0xb0, 1, 100); // mod wheel
    send (input, r, 0xc0, 5, 0);   // program change
    CHECK (r.events.empty());
}

static void sustainHoldsReleasedKeys()
{
    NoteInput input;
    Recorder r;
    send (input, r, 0x90, 60, 100);
    send (input, r, 0x90, 64, 100);
    send (input, r, 0xb0, 64, 127); // pedal down
    send (input, r, 0x80, 60, 0);   // held by the pedal
    send (input, r, 0x80, 64, 0);
    send (input, r, 0x90, 64, 100); // played again: its key holds it now
    r.events.clear();

    send (input, r, 0xb0, 64, 0); // pedal up: only 60 ends, 64's key is still down
    CHECK (r.events == (std::vector<Recorder::Event> { { Kind::Off, 60, 0.0f } }));

    r.events.clear();
    send (input, r, 0x80, 64, 0); // no pedal now: ends at once
    CHECK (r.events == (std::vector<Recorder::Event> { { Kind::Off, 64, 0.0f } }));

    // a pedal released twice ends nothing twice
    r.events.clear();
    send (input, r, 0xb0, 64, 0);
    CHECK (r.events.empty());
}

static void allNotesOffAndReset()
{
    NoteInput input;
    Recorder r;
    send (input, r, 0x90, 60, 100);
    send (input, r, 0xb0, 64, 127);
    send (input, r, 0x80, 60, 0);
    for (uint8_t controller : { 123, 120 })
    {
        r.events.clear();
        send (input, r, 0xb0, controller, 0);
        CHECK (r.events == (std::vector<Recorder::Event> { { Kind::AllOff, -1, 0.0f } }));

        // the pedal let go as well: nothing is left to end
        r.events.clear();
        send (input, r, 0xb0, 64, 0);
        CHECK (r.events.empty());
    }

    // reset-controllers lifts the pedal (ending what it held) and centres the bend
    send (input, r, 0xe0, 0x7f, 0x7f);
    send (input, r, 0x90, 62, 100);
    send (input, r, 0xb0, 64, 127);
    send (input, r, 0x80, 62, 0);
    r.events.clear();
    send (input, r, 0xb0, 121, 0);
    CHECK (r.events == (std::vector<Recorder::Event> { { Kind::Off, 62, 0.0f }, { Kind::Bend, -1, 0.0f } }));
    CHECK (input.currentBend() == 0.0f);
}

static void pitchBendSpansMinusOneToOne()
{
    NoteInput input;
    Recorder r;
    auto bendOf = [&] (int value)
    {
        send (input, r, 0xe0, (uint8_t) (value & 0x7f), (uint8_t) (value >> 7));
        return input.currentBend();
    };
    CHECK (bendOf (8192) == 0.0f);
    CHECK (bendOf (16383) == 1.0f);
    CHECK (bendOf (0) == -1.0f);
    CHECK (std::abs (bendOf (12288) - 0.5f) < 1.0e-3f);
    CHECK (r.events.back() == (Recorder::Event { Kind::Bend, -1, input.currentBend() }));

    input.reset();
    CHECK (input.currentBend() == 0.0f);
}

// --- through a real engine ---------------------------------------------------------------

struct BleepEngines
{
    BleepSynth& synth;
    float bendRange = 2.0f;
    void noteOn (int note, float velocity) { synth.noteOn (note, velocity); }
    void noteOff (int note) { synth.noteOff (note); }
    void allNotesOff() { synth.allNotesOff(); }
    void pitchBend (float amount) { synth.setPitchBend (amount * bendRange); }
};

static std::vector<float> render (BleepSynth& synth, int numSamples)
{
    std::vector<float> out ((size_t) numSamples);
    for (int pos = 0; pos < numSamples; pos += 256)
        synth.render (out.data() + pos, std::min (256, numSamples - pos));
    return out;
}

static double frequency (const std::vector<float>& v, int from, int to)
{
    double first = -1.0, last = -1.0;
    int crossings = 0;
    for (int i = std::max (1, from); i < to; ++i)
        if (v[(size_t) i - 1] < 0.0f && v[(size_t) i] >= 0.0f)
        {
            const double at = i - 1 + v[(size_t) i - 1] / (v[(size_t) i - 1] - v[(size_t) i]);
            if (first < 0.0)
                first = at;
            last = at;
            ++crossings;
        }
    return crossings < 2 ? 0.0 : (crossings - 1) * fs / (last - first);
}

static double noteHz (int note) { return 440.0 * std::exp2 ((note - 69) / 12.0); }

static BleepSynth monoSynth (float glideMs)
{
    BleepSynth synth;
    synth.prepare (fs);
    synth.setMode (BleepSynth::Mode::Mono);
    BleepParams p;
    p.wave = BleepWave::Sine;
    p.pitchEnvSemis = 0.0f;
    p.decayMs = 5000.0f;
    synth.setParams (p);
    synth.setGlideMs (glideMs);
    return synth;
}

static bool near (double hz, int note) { return std::abs (hz / noteHz (note) - 1.0) < 0.003; }

static void monoReturnsToTheHeldKey()
{
    for (float glideMs : { 0.0f, 60.0f })
    {
        auto synth = monoSynth (glideMs);
        NoteInput input;
        BleepEngines engines { synth };
        auto key = [&] (bool on, int note)
        {
            const uint8_t data[] { (uint8_t) (on ? 0x90 : 0x80), (uint8_t) note, 100 };
            input.handle (data, 3, engines);
        };

        key (true, 60);
        key (true, 64);
        key (true, 67);
        auto out = render (synth, ms (400));
        CHECK (near (frequency (out, ms (300), ms (400)), 67));

        key (false, 64); // an older key: the newest still plays
        out = render (synth, ms (400));
        CHECK (near (frequency (out, ms (300), ms (400)), 67));

        key (false, 67); // the newest: back to the one still held, not the one let go
        out = render (synth, ms (400));
        CHECK (synth.activeVoices() == 1);
        CHECK (near (frequency (out, ms (300), ms (400)), 60));
        if (glideMs > 0.0f) // slides down rather than jumping
        {
            const double start = frequency (out, 0, ms (15));
            CHECK (start > noteHz (60) * 1.03 && start < noteHz (67));
        }
        else
            CHECK (near (frequency (out, ms (2), ms (40)), 60));

        key (false, 60); // nothing held: the note ends
        render (synth, ms (300));
        CHECK (synth.activeVoices() == 0);
    }
}

// Under the pedal a let-go key keeps sounding, so the newest one stays; pedal up ends it.
static void monoWithThePedal()
{
    auto synth = monoSynth (0.0f);
    NoteInput input;
    BleepEngines engines { synth };
    auto send3 = [&] (uint8_t a, uint8_t b, uint8_t c)
    {
        const uint8_t data[] { a, b, c };
        input.handle (data, 3, engines);
    };
    send3 (0x90, 60, 100);
    send3 (0x90, 67, 100);
    send3 (0xb0, 64, 127);
    send3 (0x80, 67, 0);
    auto out = render (synth, ms (300));
    CHECK (near (frequency (out, ms (200), ms (300)), 67));

    send3 (0xb0, 64, 0); // 67 ends, back to the held 60
    out = render (synth, ms (300));
    CHECK (near (frequency (out, ms (200), ms (300)), 60));

    send3 (0x80, 60, 0);
    render (synth, ms (300));
    CHECK (synth.activeVoices() == 0);
}

static void bendMovesTheNote()
{
    BleepSynth synth;
    synth.prepare (fs);
    BleepParams p;
    p.wave = BleepWave::Sine;
    p.pitchEnvSemis = 0.0f;
    p.decayMs = 5000.0f;
    synth.setParams (p);
    NoteInput input;
    BleepEngines engines { synth, 12.0f };
    auto send3 = [&] (uint8_t a, uint8_t b, uint8_t c)
    {
        const uint8_t data[] { a, b, c };
        input.handle (data, 3, engines);
    };

    send3 (0x90, 69, 100);
    send3 (0xe0, 0x00, 0x60); // 12288: half way up, 6 semitones
    auto out = render (synth, ms (200));
    CHECK (near (frequency (out, ms (100), ms (200)), 75));

    send3 (0xb0, 121, 0); // back to the centre
    out = render (synth, ms (200));
    CHECK (near (frequency (out, ms (100), ms (200)), 69));
}

int main()
{
    notesOnAndOff();
    sustainHoldsReleasedKeys();
    allNotesOffAndReset();
    pitchBendSpansMinusOneToOne();
    monoReturnsToTheHeldKey();
    monoWithThePedal();
    bendMovesTheNote();
    return finish ("note_input");
}

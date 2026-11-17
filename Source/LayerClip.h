// Copyright (C) 2026 Henrik Juhl. Part of BleepWerk, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "Sequencer.h"

#include <numeric>

// Everything the sequencer plays when several layers run at once, written down as one MIDI clip
// of a chosen number of bars (4/4, counted from bar 1 as the sequencer counts from the start of
// the timeline). The notes follow the same rules as the live engine, so length, rate, swing,
// gate, accent, drift and vary all come with. JUCE-free so it can be tested against the engine.
//
// The layers share one voice engine, so the clip holds what the voice actually plays and not
// the layers' notes one by one: a pitch is never on twice at once. Two layers that start the same
// pitch at the same time become one note, with the velocity of the later layer (A..H) and
// ending when the first of them ends. A layer that starts a pitch while it still sounds cuts the
// earlier note off at that moment. A host's MIDI editor then shows the same thing every time, and
// playing the clip back through BleepWerk sounds as the layers do.
namespace bleepwerk
{

constexpr int kClipTicksPerQuarter = 960;
constexpr int kBeatsPerBar = 4;
constexpr int64_t kBarTicks = (int64_t) kBeatsPerBar * kClipTicksPerQuarter;

// the lengths the export offers, in bars
inline constexpr int kExportBars[] = { 4, 8, 16, 32, 64 };
constexpr int kNumExportLengths = (int) std::size (kExportBars);
constexpr int kMaxExportBars = 64;

struct TickNote
{
    int64_t start, end; // ticks from the start of the clip
    int note;
    int velocity;       // 1..127
};

struct LayeredClip
{
    std::vector<TickNote> notes; // in order of start, and one pitch never overlaps itself
    int64_t lengthTicks = 0;
};

inline int64_t beatsToTicks (double beats) { return (int64_t) std::llround (beats * kClipTicksPerQuarter); }

// How many ticks one pass of a pattern's whole cycle takes before it plays exactly the same again:
// the passes of Vary, then as many of those as it takes for Drift to come round to where it
// started, and an even number of steps so swing lines up too.
inline int64_t layerRepeatTicks (const Groove& groove)
{
    const int length = std::clamp (groove.length, 1, kMaxSteps);
    const int period = lengthPeriod (groove);
    int64_t cycleSteps = 0;
    for (int p = 0; p < period; ++p)
        cycleSteps += lengthOfPass (p, groove);

    const int64_t moved = (((int64_t) period * groove.drift) % length + length) % length;
    const int64_t cycles = moved == 0 ? 1 : length / std::gcd ((int64_t) length, moved);
    int64_t steps = cycleSteps * cycles;
    if (steps % 2 != 0)
        steps *= 2;
    return steps * beatsToTicks (groove.stepBeats());
}

// After how many whole bars the playing layers (the ones with a note to play) are back where they
// started and everything repeats exactly. 0 when no layer has anything to play, and -1 when it
// takes more bars than the export offers.
inline int barsUntilRepeat (const PatternBank& bank)
{
    int64_t period = kBarTicks;
    bool any = false;
    for (int layer = 0; layer < kNumPatternSlots; ++layer)
    {
        if (! bank.isPlaying (layer) || bank.slot (layer).hits() == 0)
            continue;
        any = true;
        const int64_t repeat = layerRepeatTicks (bank.slot (layer).groove());
        period = period / std::gcd (period, repeat) * repeat;
        if (period > kMaxExportBars * kBarTicks)
            return -1;
    }
    return any ? (int) (period / kBarTicks) : 0;
}

// The layers that play, for the number of bars, as a clip. Notes that the sequencer would still
// be holding at the end are cut there.
inline LayeredClip layersToClip (const PatternBank& bank, int rootNote, int bars)
{
    LayeredClip clip;
    clip.lengthTicks = (int64_t) std::max (bars, 0) * kBarTicks;

    struct Event
    {
        int64_t tick;
        bool on;
        int note, velocity;
    };
    std::vector<Event> events;

    // the layers one after the other, each in time order; a stable sort by time then keeps layer A
    // before layer H at the same tick, the order the processor hands them to the voices
    for (int layer = 0; layer < kNumPatternSlots; ++layer)
    {
        if (! bank.isPlaying (layer))
            continue;
        const auto& pattern = bank.slot (layer);
        const auto groove = pattern.groove();
        for (int64_t j = 0;; ++j)
        {
            const double start = stepStartBeats (j, groove);
            const int64_t on = beatsToTicks (start);
            if (on >= clip.lengthTicks)
                break;
            const auto step = pattern.get (patternIndexAt (j, groove));
            if (! step.gate)
                continue;

            // a note ends at its gate or when the next step starts, whichever comes first
            const double next = stepStartBeats (j + 1, groove);
            const int64_t off = std::max (on + 1, std::min (beatsToTicks (start + (next - start) * std::clamp ((double) groove.gate, 0.05, 1.0)),
                                                            beatsToTicks (next)));
            const int note = std::clamp (rootNote + step.note, 0, 127);
            const int velocity = midiVelocity (kSeqVelocities[std::clamp (step.level, 0, kNumStepLevels - 1)]);
            events.push_back ({ on, true, note, velocity });
            events.push_back ({ off, false, note, 0 });
        }
    }
    std::stable_sort (events.begin(), events.end(), [] (const Event& a, const Event& b) { return a.tick < b.tick; });

    // what the voice does with them: one open note per pitch
    std::array<int, 128> open;
    open.fill (-1);
    for (const auto& e : events)
    {
        int& index = open[(size_t) e.note];
        if (e.on)
        {
            if (index >= 0 && clip.notes[(size_t) index].start == e.tick)
            {
                clip.notes[(size_t) index].velocity = e.velocity; // same pitch, same time: one note, the later layer's velocity
                continue;
            }
            if (index >= 0)
                clip.notes[(size_t) index].end = e.tick; // the new one cuts the earlier one off
            clip.notes.push_back ({ e.tick, clip.lengthTicks, e.note, e.velocity });
            index = (int) clip.notes.size() - 1;
        }
        else if (index >= 0)
        {
            auto& n = clip.notes[(size_t) index];
            n.end = std::max (e.tick, n.start + 1); // a note-off that arrives with its own note-on still leaves a tick of sound
            index = -1;
        }
    }
    for (auto& n : clip.notes)
        n.end = std::min (n.end, std::max (clip.lengthTicks, n.start + 1));
    return clip;
}

} // namespace bleepwerk

// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <locale>
#include <sstream>
#include <string>

// Probability grid sequencer: rows are every MIDI note (0-127) in the chosen scale and key,
// columns are steps. Each cell has on/off, probability, velocity and ratchet. Cells are stored
// per octave and scale degree, so changing the scale or key keeps a pattern's shape and
// register: the third degree stays the third degree, in the same octave.
// Played against the host timeline; the timing approach (steps counted from beat 0, events
// on the first sample at or after their beat, jump detection) comes from SubForge.
// JUCE-free so it can be tested on its own.
namespace bleepforge
{
inline constexpr int kMaxSteps = 32;
inline constexpr int kDefaultSteps = 16;
inline constexpr int kNumOctaves = 11;              // MIDI notes 0-127 span 11 octaves from C-1
inline constexpr int kNumSlots = kNumOctaves * 12;  // cell storage: an octave times up to 12 degrees
inline constexpr int kMaxRows = 128;                // chromatic: every MIDI note

struct Scale
{
    const char* name;
    int size;
    std::array<int, 12> degrees; // semitones above the root
};

inline constexpr Scale kScales[] = {
    { "Chromatic", 12, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } },
    { "Minor", 7, { 0, 2, 3, 5, 7, 8, 10 } },
    { "Major", 7, { 0, 2, 4, 5, 7, 9, 11 } },
    { "Dorian", 7, { 0, 2, 3, 5, 7, 9, 10 } },
    { "Phrygian", 7, { 0, 1, 3, 5, 7, 8, 10 } },
    { "Harmonic Minor", 7, { 0, 2, 3, 5, 7, 8, 11 } },
    { "Minor Pentatonic", 5, { 0, 3, 5, 7, 10 } },
    { "Major Pentatonic", 5, { 0, 2, 4, 7, 9 } },
};
inline constexpr int kNumScales = (int) std::size (kScales);
inline constexpr int kDefaultScale = 1; // Minor

inline constexpr const char* kRootNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

inline const Scale& scaleAt (int index) { return kScales[std::clamp (index, 0, kNumScales - 1)]; }

// Row 0 is the lowest root, root (0-11 for C-B) itself; rows above it climb the scale.
inline int rowNote (int row, int scale, int root)
{
    const auto& s = scaleAt (scale);
    return root + 12 * (row / s.size) + s.degrees[(size_t) (row % s.size)];
}

// Rows from the lowest root up to the highest note of the scale that MIDI has.
inline int numRows (int scale, int root)
{
    int rows = 0;
    while (rowNote (rows, scale, root) <= 127)
        ++rows;
    return rows;
}

// Where a row's cells are stored: its octave and its degree in the scale.
inline int rowSlot (int row, int scale)
{
    const int size = scaleAt (scale).size;
    return (row / size) * 12 + row % size;
}

// The lowest row at or above a note (the top row for notes above them all).
inline int rowAtOrAbove (int note, int scale, int root)
{
    const int rows = numRows (scale, root);
    for (int row = 0; row < rows; ++row)
        if (rowNote (row, scale, root) >= note)
            return row;
    return rows - 1;
}

struct Cell
{
    bool on = false;
    float probability = 1.0f; // 0..1, in 1 % steps
    float velocity = 0.8f;    // 1/127..1
    int ratchet = 1;          // 1..4 hits within the step

    // One 32-bit word, so the GUI and the audio thread share cells without locks.
    uint32_t pack() const
    {
        const auto prob = (uint32_t) std::lround (std::clamp (probability, 0.0f, 1.0f) * 100.0f);
        const auto vel = (uint32_t) std::clamp ((int) std::lround (velocity * 127.0f), 1, 127);
        const auto rat = (uint32_t) (std::clamp (ratchet, 1, 4) - 1);
        return (on ? 1u : 0u) | (prob << 1) | (vel << 8) | (rat << 15);
    }

    static Cell unpack (uint32_t word)
    {
        Cell c;
        c.on = (word & 1u) != 0;
        c.probability = (float) std::min (100u, (word >> 1) & 0x7fu) / 100.0f;
        c.velocity = (float) std::max (1u, (word >> 8) & 0x7fu) / 127.0f;
        c.ratchet = (int) ((word >> 15) & 3u) + 1;
        return c;
    }

    bool operator== (const Cell& other) const { return pack() == other.pack(); }
    bool operator!= (const Cell& other) const { return ! (*this == other); }
};

// The pattern: edited by the GUI while the audio thread plays it. Every cell is one atomic
// word, so a read never sees half an edit. Cells are addressed by slot (see rowSlot).
class Grid
{
public:
    Grid() { clear(); }

    Cell get (int slot, int step) const { return Cell::unpack (cells[index (slot, step)].load (std::memory_order_relaxed)); }
    void set (int slot, int step, const Cell& cell) { cells[index (slot, step)].store (cell.pack(), std::memory_order_relaxed); }

    int getLength() const { return length.load (std::memory_order_relaxed); }
    void setLength (int steps) { length.store (std::clamp (steps, 1, kMaxSteps), std::memory_order_relaxed); }

    void clear()
    {
        const auto blank = Cell().pack();
        for (auto& c : cells)
            c.store (blank, std::memory_order_relaxed);
        setLength (kDefaultSteps);
    }

    void copyFrom (const Grid& other)
    {
        for (size_t i = 0; i < cells.size(); ++i)
            cells[i].store (other.cells[i].load (std::memory_order_relaxed), std::memory_order_relaxed);
        setLength (other.getLength());
    }

    // "grid 2 length 16 cell <slot> <step> <on> <prob%> <vel> <ratchet> ...", only cells that
    // differ from a blank one. Stored with the project; numbers keep the classic locale.
    // Version 1 counted rows from a root note and is not read.
    std::string toString() const
    {
        std::ostringstream out;
        out.imbue (std::locale::classic());
        out << "grid 2 length " << getLength();
        const Cell blank;
        for (int slot = 0; slot < kNumSlots; ++slot)
            for (int step = 0; step < kMaxSteps; ++step)
                if (const auto c = get (slot, step); c != blank)
                    out << " cell " << slot << ' ' << step << ' ' << (c.on ? 1 : 0) << ' '
                        << std::lround (c.probability * 100.0f) << ' ' << std::lround (c.velocity * 127.0f) << ' ' << c.ratchet;
        return out.str();
    }

    // Leaves the grid as it was when the text isn't a grid.
    bool fromString (const std::string& text)
    {
        std::istringstream in (text);
        in.imbue (std::locale::classic());
        std::string word;
        int version = 0, steps = 0;
        if (! (in >> word) || word != "grid" || ! (in >> version) || version != 2
            || ! (in >> word) || word != "length" || ! (in >> steps))
            return false;

        Grid parsed;
        parsed.setLength (steps);
        while (in >> word)
        {
            int slot, step, on, prob, vel, ratchet;
            if (word != "cell" || ! (in >> slot >> step >> on >> prob >> vel >> ratchet))
                return false;
            if (slot < 0 || slot >= kNumSlots || step < 0 || step >= kMaxSteps)
                continue;
            Cell c;
            c.on = on != 0;
            c.probability = (float) std::clamp (prob, 0, 100) / 100.0f;
            c.velocity = (float) std::clamp (vel, 1, 127) / 127.0f;
            c.ratchet = std::clamp (ratchet, 1, 4);
            parsed.set (slot, step, c);
        }
        copyFrom (parsed);
        return true;
    }

private:
    static size_t index (int slot, int step)
    {
        return (size_t) (std::clamp (slot, 0, kNumSlots - 1) * kMaxSteps + std::clamp (step, 0, kMaxSteps - 1));
    }

    std::array<std::atomic<uint32_t>, (size_t) (kNumSlots * kMaxSteps)> cells;
    std::atomic<int> length { kDefaultSteps };
};

// A pattern for a fresh instance, so the sequencer plays something before it is edited:
// in the default scale, rows counted from C4.
inline void loadStarterPattern (Grid& grid)
{
    grid.clear();
    auto put = [&grid] (int row, int step, float probability, float velocity)
    {
        Cell c;
        c.on = true;
        c.probability = probability;
        c.velocity = velocity;
        const int fromC4 = 5 * scaleAt (kDefaultScale).size + row;
        grid.set (rowSlot (fromC4, kDefaultScale), step, c);
    };
    put (0, 0, 1.0f, 0.95f);
    put (0, 6, 1.0f, 0.8f);
    put (0, 10, 0.85f, 0.85f);
    put (7, 3, 0.6f, 0.7f);
    put (7, 11, 0.4f, 0.6f);
    put (4, 8, 1.0f, 0.75f);
    put (4, 14, 0.5f, 0.6f);
    put (2, 13, 0.3f, 0.65f);
    put (6, 15, 0.25f, 0.5f);
}

struct SequencerSettings
{
    bool enabled = true;
    int scale = kDefaultScale;
    int root = 0;             // 0-11 for C-B
    double stepBeats = 0.25;  // 1/16
};

// Turns the grid into note on/off events for one audio block. Steps are counted from beat 0
// of the host timeline, so loops and jumps land on the right step and the timing doesn't
// depend on the block size. Each played cell gets a note-off after kGate of its step.
class SequencerEngine
{
public:
    static constexpr double kGate = 0.5;

    void seed (uint32_t s) { rng = s != 0 ? s : 0x2545f491u; }

    int getCurrentStep() const { return currentStep.load(); }

    // The last cell of a slot that played, for the GUI: (count << 6) | step, where count goes
    // up with every hit so the same step playing again shows as a new hit. 0: none yet.
    uint32_t getLastHit (int slot) const { return lastHits[(size_t) std::clamp (slot, 0, kNumSlots - 1)].load (std::memory_order_relaxed); }

    // emit (int sampleOffset, bool noteOn, int note, float velocity)
    template <typename Emit>
    void process (bool playing, double ppqStart, double bpm, double sampleRate, int numSamples,
                  const SequencerSettings& settings, const Grid& grid, Emit&& emit)
    {
        const double ppqPerSample = bpm / 60.0 / sampleRate;
        const double ppqEnd = ppqStart + numSamples * ppqPerSample;
        if (! settings.enabled || ! playing || bpm <= 0.0 || numSamples <= 0)
        {
            releaseAll (0, emit);
            lastPpqEnd = -1.0;
            currentStep.store (-1);
            return;
        }

        // the host jumped or looped: stop what is sounding and pick up from here
        if (lastPpqEnd >= 0.0 && std::abs (ppqStart - lastPpqEnd) > ppqPerSample * 4.0)
            releaseAll (0, emit);
        lastPpqEnd = ppqEnd;

        // An event happens on the first sample at or after its beat position. Deciding
        // block membership by that sample keeps timing independent of the block size.
        auto sampleFor = [&] (double ppq) { return (int64_t) std::ceil ((ppq - ppqStart) / ppqPerSample - 1.0e-6); };

        const double beats = settings.stepBeats;
        const int length = std::clamp (grid.getLength(), 1, kMaxSteps);
        const auto first = (int64_t) std::floor (ppqStart / beats) - 1;
        const auto last = (int64_t) std::ceil (ppqEnd / beats) + 1;

        for (int64_t j = first; j <= last; ++j)
        {
            const double start = (double) j * beats; // silent before beat 0 (a host's pre-roll)
            const auto sample = sampleFor (start);
            if (start < 0.0 || sample < 0 || sample >= numSamples)
                continue;

            flushNoteOffs (sample, numSamples, sampleFor, emit);
            triggerStep ((int) (j % length), start, beats, (int) sample, settings, grid, emit);
        }
        flushNoteOffs (numSamples - 1, numSamples, sampleFor, emit);
    }

    // Note-offs for everything the sequencer has sounding (stop, bypass, jumps).
    template <typename Emit>
    void releaseAll (int sample, Emit& emit)
    {
        for (int i = 0; i < numSounding; ++i)
            emit (sample, false, sounding[(size_t) i].note, 0.0f);
        numSounding = 0;
    }

private:
    struct Sounding
    {
        int note;
        double offPpq;
    };

    template <typename SampleFor, typename Emit>
    void flushNoteOffs (int64_t upTo, int numSamples, SampleFor& sampleFor, Emit& emit)
    {
        for (int i = 0; i < numSounding;)
        {
            const auto offSample = sampleFor (sounding[(size_t) i].offPpq);
            if (offSample <= upTo)
            {
                emit ((int) std::clamp<int64_t> (offSample, 0, numSamples - 1), false, sounding[(size_t) i].note, 0.0f);
                sounding[(size_t) i] = sounding[(size_t) --numSounding];
            }
            else
                ++i;
        }
    }

    template <typename Emit>
    void triggerStep (int step, double start, double beats, int sample, const SequencerSettings& settings,
                      const Grid& grid, Emit& emit)
    {
        currentStep.store (step);
        const int rows = numRows (settings.scale, settings.root);
        for (int row = 0; row < rows; ++row)
        {
            const int slot = rowSlot (row, settings.scale);
            const auto cell = grid.get (slot, step);
            if (! cell.on || (cell.probability < 1.0f && nextRandom() >= cell.probability))
                continue;

            const int note = rowNote (row, settings.scale, settings.root);
            for (int i = 0; i < numSounding; ++i) // still sounding: end it before the new hit
                if (sounding[(size_t) i].note == note)
                {
                    emit (sample, false, note, 0.0f);
                    sounding[(size_t) i] = sounding[(size_t) --numSounding];
                    break;
                }

            emit (sample, true, note, cell.velocity);
            auto& hit = lastHits[(size_t) slot];
            hit.store ((((hit.load (std::memory_order_relaxed) >> 6) + 1) << 6) | (uint32_t) step, std::memory_order_relaxed);
            if (numSounding < (int) sounding.size())
                sounding[(size_t) numSounding++] = { note, start + beats * kGate };
            else
                emit (sample, false, note, 0.0f);
        }
    }

    // xorshift32 in [0, 1): allocation-free for the audio thread, and seedable for tests.
    float nextRandom()
    {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        return (float) (rng >> 8) * (1.0f / 16777216.0f);
    }

    std::array<Sounding, kMaxRows> sounding {}; // each note at most once
    int numSounding = 0;
    double lastPpqEnd = -1.0;
    uint32_t rng = 0x2545f491u;
    std::atomic<int> currentStep { -1 };
    std::array<std::atomic<uint32_t>, kNumSlots> lastHits {};
};
} // namespace bleepforge

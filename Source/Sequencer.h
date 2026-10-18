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
    const char* group;           // the heading it sits under in the Scale menu
};

// A project stores the index, so new scales go at the end; the menu groups them instead. The
// scales after the first eight come from SeqVerse (same author and licence).
inline constexpr Scale kScales[] = {
    { "Chromatic", 12, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }, "Basic" },
    { "Minor", 7, { 0, 2, 3, 5, 7, 8, 10 }, "Basic" },
    { "Major", 7, { 0, 2, 4, 5, 7, 9, 11 }, "Basic" },
    { "Dorian", 7, { 0, 2, 3, 5, 7, 9, 10 }, "Modes" },
    { "Phrygian", 7, { 0, 1, 3, 5, 7, 8, 10 }, "Modes" },
    { "Harmonic Minor", 7, { 0, 2, 3, 5, 7, 8, 11 }, "Basic" },
    { "Minor Pentatonic", 5, { 0, 3, 5, 7, 10 }, "Five notes" },
    { "Major Pentatonic", 5, { 0, 2, 4, 7, 9 }, "Five notes" },

    { "Melodic Minor", 7, { 0, 2, 3, 5, 7, 9, 11 }, "Basic" },
    { "Lydian", 7, { 0, 2, 4, 6, 7, 9, 11 }, "Modes" },
    { "Mixolydian", 7, { 0, 2, 4, 5, 7, 9, 10 }, "Modes" },
    { "Locrian", 7, { 0, 1, 3, 5, 6, 8, 10 }, "Modes" },
    { "Blues Minor", 6, { 0, 3, 5, 6, 7, 10 }, "Six notes" },
    { "Blues Major", 6, { 0, 2, 3, 4, 7, 9 }, "Six notes" },
    { "Whole Tone", 6, { 0, 2, 4, 6, 8, 10 }, "Six notes" },
    { "Diminished W-H", 8, { 0, 2, 3, 5, 6, 8, 9, 11 }, "Eight notes" },
    { "Diminished H-W", 8, { 0, 1, 3, 4, 6, 7, 9, 10 }, "Eight notes" },
    { "Augmented", 6, { 0, 3, 4, 7, 8, 11 }, "Six notes" },
    { "Harmonic Major", 7, { 0, 2, 4, 5, 7, 8, 11 }, "Exotic" },
    { "Phrygian Dominant", 7, { 0, 1, 4, 5, 7, 8, 10 }, "Exotic" },
    { "Double Harmonic", 7, { 0, 1, 4, 5, 7, 8, 11 }, "Exotic" },
    { "Hungarian Minor", 7, { 0, 2, 3, 6, 7, 8, 11 }, "Exotic" },
    { "Hungarian Major", 7, { 0, 3, 4, 6, 7, 9, 10 }, "Exotic" },
    { "Neapolitan Minor", 7, { 0, 1, 3, 5, 7, 8, 11 }, "Exotic" },
    { "Neapolitan Major", 7, { 0, 1, 3, 5, 7, 9, 11 }, "Exotic" },
    { "Lydian Dominant", 7, { 0, 2, 4, 6, 7, 9, 10 }, "Jazz" },
    { "Lydian Augmented", 7, { 0, 2, 4, 6, 8, 9, 11 }, "Jazz" },
    { "Locrian Natural 2", 7, { 0, 2, 3, 5, 6, 8, 10 }, "Jazz" },
    { "Altered", 7, { 0, 1, 3, 4, 6, 8, 10 }, "Jazz" },
    { "Mixolydian b6", 7, { 0, 2, 4, 5, 7, 8, 10 }, "Jazz" },
    { "Dorian b2", 7, { 0, 1, 3, 5, 7, 9, 10 }, "Jazz" },
    { "Ukrainian Dorian", 7, { 0, 2, 3, 6, 7, 9, 10 }, "Exotic" },
    { "Persian", 7, { 0, 1, 4, 5, 6, 8, 11 }, "Exotic" },
    { "Enigmatic", 7, { 0, 1, 4, 6, 8, 10, 11 }, "Exotic" },
    { "Prometheus", 6, { 0, 2, 4, 6, 9, 10 }, "Six notes" },
    { "Tritone", 6, { 0, 1, 4, 6, 7, 10 }, "Six notes" },
    { "Bebop Dominant", 8, { 0, 2, 4, 5, 7, 9, 10, 11 }, "Eight notes" },
    { "Bebop Major", 8, { 0, 2, 4, 5, 7, 8, 9, 11 }, "Eight notes" },
    { "Hirajoshi", 5, { 0, 2, 3, 7, 8 }, "Five notes" },
    { "Kumoi", 5, { 0, 2, 3, 7, 9 }, "Five notes" },
    { "In Sen", 5, { 0, 1, 5, 7, 10 }, "Five notes" },
    { "Iwato", 5, { 0, 1, 5, 6, 10 }, "Five notes" },
    { "Egyptian", 5, { 0, 2, 5, 7, 10 }, "Five notes" },
};
inline constexpr const char* kScaleGroups[] = { "Basic", "Modes", "Five notes", "Six notes", "Eight notes", "Exotic", "Jazz" };
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

// Step rates, in quarter-note beats per step.
inline constexpr double kSeqRateBeats[] = { 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0 };
inline constexpr const char* kSeqRateNames[] = { "1/8", "1/16", "1/32", "1/8T", "1/16T" };
inline constexpr int kNumSeqRates = (int) std::size (kSeqRateBeats);
inline constexpr int kDefaultSeqRate = 1; // 1/16

struct SequencerSettings
{
    bool enabled = true;
    int scale = kDefaultScale;
    int root = 0;               // 0-11 for C-B
    double stepBeats = 0.25;    // 1/16
    double swing = 0.5;         // 0.5 straight .. 0.75 heavy shuffle; delays every second step
    float velocityChaos = 0.0f; // 0..1: how far each hit's velocity may stray
    int cycleSteps = 0;         // 0: off; else the pattern restarts and its chances repeat every this many steps
    int seed = 1;               // which phrase a cycle locks in
};

// Cycle lengths in bars of four beats; 0 is off.
inline constexpr int kCycleBars[] = { 0, 1, 2, 4, 8, 16 };
inline constexpr const char* kCycleNames[] = { "Off", "1 bar", "2 bars", "4 bars", "8 bars", "16 bars" };
inline constexpr int kNumCycles = (int) std::size (kCycleBars);
inline constexpr int kMaxSeed = 999;

// Steps in a cycle at a step rate; every rate divides a bar into whole steps.
inline int cycleSteps (int cycleIndex, double stepBeats)
{
    const int bars = kCycleBars[(size_t) std::clamp (cycleIndex, 0, kNumCycles - 1)];
    return bars > 0 ? (int) std::lround (4.0 * bars / stepBeats) : 0;
}

// A random number generator (splitmix64) cheap enough to make one per cell and step, so a
// cell's chances depend only on the seed and where it is in the cycle: the same in every
// cycle and after every restart. (From SeqVerse, same author and licence.)
struct StepRandom
{
    uint64_t state;

    static constexpr uint64_t golden = 0x9e3779b97f4a7c15ull;

    static constexpr StepRandom forCell (int seed, int64_t cycleStep, int slot)
    {
        return { (uint64_t) seed * golden ^ ((uint64_t) cycleStep * kNumSlots + (uint64_t) slot) * 0xd1342543de82ef95ull };
    }

    constexpr uint64_t next()
    {
        state += golden;
        uint64_t z = state;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
        return z ^ (z >> 31);
    }

    float uniform() { return (float) (next() >> 40) * (1.0f / 16777216.0f); } // 0..1, below 1
};

// Start of step k in beats from the timeline's zero; swing delays every second step. (The
// swing scheme comes from SubForge.)
inline double stepStartBeats (int64_t k, double beats, double swing)
{
    const auto pairStart = (double) (k - (k & 1)) * beats;
    return (k & 1) ? pairStart + 2.0 * beats * swing : pairStart;
}

// Turns the grid into note on/off events for one audio block. Steps are counted from beat 0
// of the host timeline, so loops and jumps land on the right step and the timing doesn't
// depend on the block size. A played cell is scheduled when its step starts: one hit, or its
// ratchet's hits spread evenly over the step, each ending after kGate of its share. Hits are
// queued, so the later ones of a ratchet can fall in later blocks.
// With a cycle, the pattern starts over at every cycle (cycles start on multiples of their
// length on the timeline) and each cell's chance and velocity variation come from the seed
// and its place in the cycle, so every cycle plays the same phrase. Without one, they are
// drawn afresh every time.
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
        const double swing = std::clamp (settings.swing, 0.5, 0.75);
        const int length = std::clamp (grid.getLength(), 1, kMaxSteps);
        const auto first = (int64_t) std::floor (ppqStart / beats) - 2;
        const auto last = (int64_t) std::ceil (ppqEnd / beats) + 1;

        for (int64_t j = first; j <= last; ++j)
        {
            const double start = stepStartBeats (j, beats, swing); // silent before beat 0 (a host's pre-roll)
            const auto sample = sampleFor (start);
            if (start < 0.0 || sample < 0 || sample >= numSamples)
                continue;

            flush (sample - 1, numSamples, sampleFor, emit); // everything before this step
            const int64_t inCycle = settings.cycleSteps > 0 ? j % settings.cycleSteps : -1;
            scheduleStep ((int) ((inCycle >= 0 ? inCycle : j) % length), inCycle, start, stepStartBeats (j + 1, beats, swing) - start,
                          settings, grid);
        }
        flush (numSamples - 1, numSamples, sampleFor, emit);
    }

    // Note-offs for everything the sequencer has sounding, and forgets the hits still to
    // come (stop, bypass, jumps).
    template <typename Emit>
    void releaseAll (int sample, Emit& emit)
    {
        for (int i = 0; i < numPending; ++i)
            if (pending[(size_t) i].started)
                emit (sample, false, pending[(size_t) i].note, 0.0f);
        numPending = 0;
    }

private:
    struct Hit
    {
        int note, slot, step;
        float velocity;
        double onPpq, offPpq;
        bool started;
    };

    void remove (int index) { pending[(size_t) index] = pending[(size_t) --numPending]; }

    // Emits the queued note-ons and note-offs up to sample upTo in time order; at the same
    // sample, note-offs first.
    template <typename SampleFor, typename Emit>
    void flush (int64_t upTo, int numSamples, SampleFor& sampleFor, Emit& emit)
    {
        for (;;)
        {
            int best = -1;
            int64_t bestSample = 0;
            for (int i = 0; i < numPending; ++i)
            {
                const auto& h = pending[(size_t) i];
                const auto s = sampleFor (h.started ? h.offPpq : h.onPpq);
                if (s <= upTo && (best < 0 || s < bestSample || (s == bestSample && h.started && ! pending[(size_t) best].started)))
                {
                    best = i;
                    bestSample = s;
                }
            }
            if (best < 0)
                return;

            const int sample = (int) std::clamp<int64_t> (bestSample, 0, numSamples - 1);
            if (pending[(size_t) best].started)
            {
                emit (sample, false, pending[(size_t) best].note, 0.0f);
                remove (best);
                continue;
            }

            // the same note still sounding (a long ratchet hit, two cells on one note): end it first
            const int note = pending[(size_t) best].note;
            for (int i = 0; i < numPending; ++i)
                if (i != best && pending[(size_t) i].started && pending[(size_t) i].note == note)
                {
                    emit (sample, false, note, 0.0f);
                    remove (i);
                    if (best == numPending)
                        best = i; // it was moved into the removed one's place
                    break;
                }

            auto& h = pending[(size_t) best];
            emit (sample, true, note, h.velocity);
            h.started = true;
            auto& hit = lastHits[(size_t) h.slot];
            hit.store ((((hit.load (std::memory_order_relaxed) >> 6) + 1) << 6) | (uint32_t) h.step, std::memory_order_relaxed);
        }
    }

    // inCycle: the step's place in the cycle, or -1 without one
    void scheduleStep (int step, int64_t inCycle, double start, double duration, const SequencerSettings& settings, const Grid& grid)
    {
        currentStep.store (step);
        const int rows = numRows (settings.scale, settings.root);
        for (int row = 0; row < rows; ++row)
        {
            const int slot = rowSlot (row, settings.scale);
            const auto cell = grid.get (slot, step);
            if (! cell.on)
                continue;

            // both numbers are drawn for every cell that is on, so in a cycle editing one
            // cell never changes what another plays
            auto cellRandom = StepRandom::forCell (settings.seed, inCycle, slot);
            auto draw = [&] { return inCycle >= 0 ? cellRandom.uniform() : nextRandom(); };
            const float chance = draw(), variation = draw();
            if (cell.probability < 1.0f && chance >= cell.probability)
                continue;

            float velocity = cell.velocity;
            if (settings.velocityChaos > 0.0f)
                velocity = std::clamp (velocity * (1.0f + settings.velocityChaos * 0.6f * (2.0f * variation - 1.0f)), 1.0f / 127.0f, 1.0f);

            const int note = rowNote (row, settings.scale, settings.root);
            const double share = duration / cell.ratchet;
            for (int i = 0; i < cell.ratchet && numPending < (int) pending.size(); ++i)
            {
                const double on = start + i * share;
                pending[(size_t) numPending++] = { note, slot, step, velocity, on, on + share * kGate, false };
            }
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

    std::array<Hit, kMaxRows * 4> pending {}; // every note with a full ratchet
    int numPending = 0;
    double lastPpqEnd = -1.0;
    uint32_t rng = 0x2545f491u;
    std::atomic<int> currentStep { -1 };
    std::array<std::atomic<uint32_t>, kNumSlots> lastHits {};
};
} // namespace bleepforge

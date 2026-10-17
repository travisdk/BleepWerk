// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

// Sequencer: step timing independent of the host block size, scale mapping, probability
// distribution, velocity, no stuck notes after stopping or jumping, silent pre-roll, and the
// grid surviving a save and load.
#include "Sequencer.h"
#include "TestUtils.h"

#include <vector>

using namespace bleepforge;

constexpr double sampleRate = 48000.0, bpm = 120.0;
constexpr long samplesPerStep = 6000; // 1/16 at 120 BPM

struct Event
{
    long sample;
    bool on;
    int note;
    float velocity;

    bool operator== (const Event& o) const { return sample == o.sample && on == o.on && note == o.note; }
};

// Plays the grid for numSteps steps and then stops the transport. loopAtStep > 0 jumps back
// to beat 0 there (a host loop).
static std::vector<Event> run (const Grid& grid, int blockSize, long numSteps, SequencerSettings settings = {},
                               double startBeat = 0.0, long loopAtStep = -1, uint32_t seed = 1)
{
    SequencerEngine engine;
    engine.seed (seed);
    std::vector<Event> events;
    // the position comes from a sample count, as a host's does, so it doesn't drift
    double originBeat = startBeat;
    long originSample = 0, t = 0;
    const double ppqPerSample = bpm / 60.0 / sampleRate;
    auto ppqAt = [&] (long sample) { return originBeat + (double) (sample - originSample) * ppqPerSample; };
    const long end = numSteps * samplesPerStep;
    while (t < end)
    {
        if (loopAtStep > 0 && t >= loopAtStep * samplesPerStep)
        {
            originBeat = 0.0;
            originSample = t;
            loopAtStep = -1;
        }
        // blocks end exactly at the loop point and at the end, as a host's would
        const long stop = loopAtStep > 0 ? std::min (end, loopAtStep * samplesPerStep) : end;
        const int length = (int) std::min ((long) blockSize, stop - t);
        engine.process (true, ppqAt (t), bpm, sampleRate, length, settings, grid,
                        [&] (int s, bool on, int note, float vel) { events.push_back ({ t + s, on, note, vel }); });
        t += length;
    }
    engine.process (false, ppqAt (t), bpm, sampleRate, blockSize, settings, grid,
                    [&] (int s, bool on, int note, float vel) { events.push_back ({ t + s, on, note, vel }); });
    return events;
}

static Cell hit (float probability = 1.0f, float velocity = 0.8f)
{
    Cell c;
    c.on = true;
    c.probability = probability;
    c.velocity = velocity;
    return c;
}

// Every note-on is matched by a note-off, and nothing sounds at the end.
static bool balanced (const std::vector<Event>& events)
{
    std::array<int, 128> open {};
    for (const auto& e : events)
    {
        open[(size_t) e.note] += e.on ? 1 : -1;
        if (open[(size_t) e.note] < 0 || open[(size_t) e.note] > 1)
            return false;
    }
    for (int n : open)
        if (n != 0)
            return false;
    return true;
}

static void scaleMapping()
{
    const int minor[] = { 60, 62, 63, 65, 67, 68, 70, 72 };
    CHECK (numRows (1, 1) == 8);
    for (int row = 0; row < 8; ++row)
        CHECK (rowNote (row, 1, 60) == minor[row]);

    CHECK (numRows (0, 1) == 13);
    CHECK (rowNote (12, 0, 60) == 72);
    CHECK (numRows (6, 1) == 6);

    // over several octaves the rows keep climbing the scale
    CHECK (numRows (1, 3) == 22);
    CHECK (numRows (0, kMaxRange) == kMaxRows);
    CHECK (rowNote (21, 1, 36) == 72);  // C2 minor, range 3: the top row is C5
    CHECK (rowNote (9, 1, 36) == 51);   // the third degree in the second octave: D#3
    CHECK (rowNote (5, 6, 57) == 69); // A minor pentatonic: the top row is A an octave up
    CHECK (rowNote (2, 4, 64) == 67); // E Phrygian: F# isn't in it, G is
}

static void stepsLandOnTheirSample()
{
    Grid grid;
    for (int step = 0; step < kDefaultSteps; step += 4)
        grid.set (0, step, hit());
    const auto events = run (grid, 512, 32);

    std::vector<long> ons;
    for (const auto& e : events)
        if (e.on)
            ons.push_back (e.sample);
    CHECK (ons.size() == 8);
    for (size_t i = 0; i < ons.size(); ++i)
        CHECK (ons[i] == (long) i * 4 * samplesPerStep);

    // each note ends half a step later
    for (const auto& e : events)
        if (! e.on && e.sample < 32 * samplesPerStep)
            CHECK (e.sample % (4 * samplesPerStep) == samplesPerStep / 2);
    CHECK (balanced (events));
}

static void blockSizeDoesNotMatter()
{
    Grid grid;
    loadStarterPattern (grid);
    for (int step = 0; step < kDefaultSteps; ++step)
        for (int row = 0; row < numRows (kDefaultScale, 1); ++row)
            if (auto c = grid.get (row, step); c.on)
            {
                c.probability = 1.0f; // so every run plays the same notes
                grid.set (row, step, c);
            }

    const auto reference = run (grid, 64, 48);
    for (int blockSize : { 1, 37, 128, 333, 512, 4096 })
        CHECK (run (grid, blockSize, 48) == reference);
    CHECK (balanced (reference));
}

static void severalRowsSoundTogether()
{
    Grid grid;
    grid.set (0, 0, hit());
    grid.set (2, 0, hit());
    grid.set (4, 0, hit());
    const auto events = run (grid, 256, 4);
    int onsAtZero = 0;
    for (const auto& e : events)
        onsAtZero += e.on && e.sample == 0 ? 1 : 0;
    CHECK (onsAtZero == 3);
    CHECK (balanced (events));
}

static void rowsAboveTheScaleAreSilent()
{
    Grid grid;
    grid.set (10, 0, hit()); // only exists in the chromatic scale, or over more octaves
    SequencerSettings minor;
    CHECK (run (grid, 256, 16, minor).empty());

    SequencerSettings chromatic;
    chromatic.scale = 0;
    auto events = run (grid, 256, 16, chromatic);
    CHECK (events.size() == 2 && events[0].note == 70);

    SequencerSettings minorTwoOctaves;
    minorTwoOctaves.range = 2;
    events = run (grid, 256, 16, minorTwoOctaves);
    CHECK (events.size() == 2 && events[0].note == 77); // F5: fourth degree, second octave
}

static void probabilityDistribution()
{
    for (float probability : { 0.0f, 0.1f, 0.3f, 0.5f, 0.9f, 1.0f })
    {
        Grid grid;
        for (int step = 0; step < kDefaultSteps; ++step)
            grid.set (0, step, hit (probability));
        constexpr long steps = 4000;
        const auto events = run (grid, 512, steps);
        long ons = 0;
        for (const auto& e : events)
            ons += e.on ? 1 : 0;
        const double ratio = (double) ons / (double) steps;
        CHECK (std::abs (ratio - probability) < 0.03);
        CHECK (balanced (events));
    }

    // Different passes over the same pattern play different notes.
    Grid grid;
    for (int step = 0; step < kDefaultSteps; ++step)
        grid.set (0, step, hit (0.5f));
    const auto events = run (grid, 512, 32);
    std::array<bool, 16> firstPass {}, secondPass {};
    for (const auto& e : events)
        if (e.on)
            (e.sample < 16 * samplesPerStep ? firstPass : secondPass)[(size_t) ((e.sample / samplesPerStep) % 16)] = true;
    CHECK (firstPass != secondPass);
}

static void velocityIsPassedOn()
{
    Grid grid;
    grid.set (0, 0, hit (1.0f, 0.25f));
    grid.set (0, 1, hit (1.0f, 1.0f));
    const auto events = run (grid, 256, 2);
    CHECK (events.size() >= 2);
    CHECK (std::abs (events[0].velocity - 32.0f / 127.0f) < 1.0e-6f);
    for (const auto& e : events)
        if (e.on && e.sample == samplesPerStep)
            CHECK (e.velocity == 1.0f);
}

static void stopAndJumpsLeaveNoNoteHanging()
{
    Grid grid;
    for (int step = 0; step < kDefaultSteps; ++step)
        grid.set (step % 8, step, hit());

    // stop in the middle of a note: 0.25 step into step 5
    SequencerEngine engine;
    std::vector<Event> events;
    auto emit = [&] (int s, bool on, int note, float vel) { events.push_back ({ s, on, note, vel }); };
    engine.process (true, 0.0, bpm, sampleRate, (int) (5.25 * samplesPerStep), {}, grid, emit);
    engine.process (false, 0.0, bpm, sampleRate, 512, {}, grid, emit);
    CHECK (balanced (events));

    // a loop back to the start halfway through a note
    CHECK (balanced (run (grid, 512, 24, {}, 0.0, 10)));

    // switching the sequencer off ends notes too
    events.clear();
    SequencerSettings off;
    off.enabled = false;
    engine.process (true, 0.0, bpm, sampleRate, (int) (2.25 * samplesPerStep), {}, grid, emit);
    engine.process (true, 0.5625, bpm, sampleRate, 512, off, grid, emit);
    CHECK (balanced (events));
}

static void loopRestartsAtStepZero()
{
    Grid grid;
    grid.set (0, 0, hit());
    grid.set (1, 6, hit());
    const auto events = run (grid, 512, 20, {}, 0.0, 8); // jump back to beat 0 at step 8
    std::vector<long> rootOns;
    for (const auto& e : events)
        if (e.on && e.note == 60)
            rootOns.push_back (e.sample);
    CHECK (rootOns.size() == 2);
    CHECK (rootOns.size() == 2 && rootOns[1] == 8 * samplesPerStep);
}

static void preRollIsSilent()
{
    Grid grid;
    for (int step = 0; step < kDefaultSteps; ++step)
        grid.set (0, step, hit());
    const auto events = run (grid, 512, 12, {}, -2.0); // two beats of count-in
    CHECK (! events.empty());
    for (const auto& e : events)
        if (e.on)
            CHECK (e.sample >= 8 * samplesPerStep);
    CHECK (events.front().sample == 8 * samplesPerStep);
}

static void lengthWraps()
{
    Grid grid;
    grid.setLength (5);
    grid.set (0, 0, hit());
    grid.set (0, 7, hit()); // beyond the length: never plays
    const auto events = run (grid, 512, 20);
    std::vector<long> ons;
    for (const auto& e : events)
        if (e.on)
            ons.push_back (e.sample / samplesPerStep);
    CHECK ((ons == std::vector<long> { 0, 5, 10, 15 }));
}

static void cellsPack()
{
    for (bool on : { false, true })
        for (int prob : { 0, 1, 37, 100 })
            for (int vel : { 1, 64, 127 })
                for (int ratchet : { 1, 2, 3, 4 })
                {
                    Cell c;
                    c.on = on;
                    c.probability = (float) prob / 100.0f;
                    c.velocity = (float) vel / 127.0f;
                    c.ratchet = ratchet;
                    const auto back = Cell::unpack (c.pack());
                    CHECK (back.on == on && back.ratchet == ratchet);
                    CHECK (std::lround (back.probability * 100.0f) == prob);
                    CHECK (std::lround (back.velocity * 127.0f) == vel);
                }
}

static void gridSavesAndLoads()
{
    Grid grid;
    loadStarterPattern (grid);
    grid.setLength (23);
    Cell offButShaped;
    offButShaped.probability = 0.42f;
    offButShaped.ratchet = 3;
    grid.set (12, 31, offButShaped);

    Grid loaded;
    CHECK (loaded.fromString (grid.toString()));
    CHECK (loaded.getLength() == 23);
    bool same = true;
    for (int row = 0; row < kMaxRows; ++row)
        for (int step = 0; step < kMaxSteps; ++step)
            same = same && loaded.get (row, step) == grid.get (row, step);
    CHECK (same);

    // text that isn't a grid leaves it alone
    CHECK (! loaded.fromString (""));
    CHECK (! loaded.fromString ("grid 2 length 16"));
    CHECK (! loaded.fromString ("grid 1 length 16 cell 0 0 1"));
    CHECK (loaded.getLength() == 23 && loaded.get (12, 31) == offButShaped);
}

int main()
{
    scaleMapping();
    stepsLandOnTheirSample();
    blockSizeDoesNotMatter();
    severalRowsSoundTogether();
    rowsAboveTheScaleAreSilent();
    probabilityDistribution();
    velocityIsPassedOn();
    stopAndJumpsLeaveNoNoteHanging();
    loopRestartsAtStepZero();
    preRollIsSilent();
    lengthWraps();
    cellsPack();
    gridSavesAndLoads();
    return finish ("sequencer");
}

// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

// Sequencer engine: timing independent of the host block size, swing and gate, no stuck notes
// after loops, jumps or stopping, pattern slots that switch when the pattern starts over, the
// three velocities, text round trips, and MIDI clips that play like the sequencer.
#include "Sequencer.h"
#include "TestUtils.h"

#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

using namespace bleepforge;

constexpr double sampleRate = 48000.0, bpm = 120.0;

struct Event
{
    long sample;
    bool on;
    int note;
    float velocity;
};

// Plays seconds of a bank with the pattern in slot A; optionally jumps back to beat 0 at loopAtBeat.
static std::vector<Event> run (int blockSize, const Pattern& pattern, double seconds, double loopAtBeat = -1.0, double startBeat = 0.0,
                               bool stopAtEnd = true)
{
    SequencerEngine engine;
    PatternBank bank;
    bank.slot (0).copyFrom (pattern);
    SequencerSettings settings;
    settings.enabled = true;
    settings.rootNote = 0; // so a note is its step's offset
    std::vector<Event> events;
    double ppq = startBeat;
    long t = 0;
    auto emit = [&] (int s, bool on, int note, float velocity) { events.push_back ({ t + s, on, note, velocity }); };
    while (t < seconds * sampleRate)
    {
        if (loopAtBeat > 0.0 && ppq >= loopAtBeat)
        {
            ppq = 0.0;
            loopAtBeat = -1.0;
        }
        engine.process (true, ppq, bpm, sampleRate, blockSize, settings, bank, emit);
        ppq += blockSize * bpm / 60.0 / sampleRate;
        t += blockSize;
    }
    if (stopAtEnd)
        engine.process (false, 0.0, bpm, sampleRate, 64, settings, bank, emit);
    return events;
}

// Every step plays, from firstNote upwards (a Pattern holds atomics, so it can't be returned).
struct EveryStep
{
    EveryStep (int firstNote, const Groove& groove)
    {
        for (int i = 0; i < kMaxSteps; ++i)
            pattern.set (i, { true, firstNote + i, kNormalLevel });
        pattern.setGroove (groove);
    }
    Pattern pattern;
};

static bool nothingStuck (const std::vector<Event>& events)
{
    int sounding = 0;
    for (const auto& e : events)
    {
        sounding += e.on ? 1 : -1;
        if (sounding < 0 || sounding > 1) // a step ends the one before it, so never two at once
            return false;
    }
    return sounding == 0;
}

// the note-on times before the end (a block may run on past it)
static std::vector<long> noteOns (const std::vector<Event>& events, long before)
{
    std::vector<long> times;
    for (const auto& e : events)
        if (e.on && e.sample < before)
            times.push_back (e.sample);
    return times;
}

// The timing does not depend on how the host cuts the audio into blocks.
static void timingIsBlockSizeIndependent()
{
    Groove groove;
    groove.swing = 0.6f;
    const EveryStep every (0, groove);
    const auto& pattern = every.pattern;
    const long end = (long) (4.0 * sampleRate);
    const auto reference = noteOns (run (64, pattern, 4.0), end);
    CHECK (reference.size() == 32); // 4 s at 120 BPM and 1/16: 8 steps a second... 2 beats a second, 4 steps a beat
    for (int block : { 1, 37, 128, 480, 1000, 4096 })
        CHECK (noteOns (run (block, pattern, 4.0), end) == reference);
}

// A 1/16 step is 6000 samples at 120 BPM; the gate sets how long a note is held; swing delays every second step.
static void gateAndSwing()
{
    Groove groove;
    groove.gate = 0.5f;
    auto events = run (256, EveryStep (0, groove).pattern, 1.0);
    CHECK (events[0].sample == 0 && events[0].on && events[0].note == 0);
    CHECK (events[1].sample == 3000 && ! events[1].on); // half the step
    CHECK (events[2].sample == 6000 && events[2].on);

    groove.swing = 0.75f;
    events = run (256, EveryStep (0, groove).pattern, 1.0);
    const auto ons = noteOns (events, 48000);
    CHECK (ons[0] == 0 && ons[1] == 9000 && ons[2] == 12000); // the second step comes a half step late
}

static void velocitiesFollowTheLevels()
{
    Pattern p;
    for (int i = 0; i < kMaxSteps; ++i)
        p.set (i, { true, 0, i % 3 });
    const auto events = run (480, p, 1.0);
    int seen = 0;
    for (const auto& e : events)
        if (e.on)
        {
            CHECK (e.velocity == kSeqVelocities[seen % 3]);
            ++seen;
        }
    CHECK (seen == 8);
    CHECK (kSeqVelocities[0] < kSeqVelocities[1] && kSeqVelocities[1] < kSeqVelocities[2] && kSeqVelocities[2] <= 1.0f);
}

static void restsPlayNothing()
{
    Pattern p;
    p.clear();
    CHECK (run (256, p, 2.0).empty());
    p.set (4, { true, 5, kNormalLevel });
    const auto ons = noteOns (run (256, p, 3.0), 144000);
    CHECK (ons.size() == 2 && ons[0] == 4 * 6000 && ons[1] == 4 * 6000 + 16 * 6000);
}

// A shorter pattern loops on its own length.
static void lengthLoops()
{
    Groove groove;
    groove.length = 3;
    const auto events = run (256, EveryStep (10, groove).pattern, 1.0);
    std::vector<int> notes;
    for (const auto& e : events)
        if (e.on)
            notes.push_back (e.note);
    CHECK (notes.size() >= 6 && notes[0] == 10 && notes[1] == 11 && notes[2] == 12 && notes[3] == 10);
}

// A loop in the host, a jump, or stopping leaves no note hanging, whatever the block size.
static void nothingHangs()
{
    for (int block : { 64, 480, 2000 })
    {
        const EveryStep every (0, Groove {});
        const auto& pattern = every.pattern;
        CHECK (nothingStuck (run (block, pattern, 3.0)));
        CHECK (nothingStuck (run (block, pattern, 3.0, 1.37)));   // a loop in the middle of a step
        CHECK (nothingStuck (run (block, pattern, 3.0, 2.0, 0.5))); // starting off the grid
    }
    // stopping in the middle of a held note sends its off
    Groove groove;
    groove.gate = 1.0f;
    CHECK (nothingStuck (run (333, EveryStep (0, groove).pattern, 0.55)));
}

static void silentWhenDisabledOrStopped()
{
    SequencerEngine engine;
    PatternBank bank;
    SequencerSettings settings; // not enabled
    int events = 0;
    auto emit = [&] (int, bool, int, float) { ++events; };
    engine.process (true, 0.0, bpm, sampleRate, 4800, settings, bank, emit);
    CHECK (events == 0);
    settings.enabled = true;
    engine.process (false, 0.0, bpm, sampleRate, 4800, settings, bank, emit);
    CHECK (events == 0);
    engine.process (true, -8.0, bpm, sampleRate, 480, settings, bank, emit); // a pre-roll before beat 0
    CHECK (events == 0);
}

// Asking for another slot waits for the playing pattern to start over, and the new one begins on
// its first step.
static void slotSwitchLandsOnTheLoop()
{
    SequencerEngine engine;
    PatternBank bank;
    Groove groove;
    groove.length = 4;
    bank.slot (0).copyFrom (EveryStep (0, groove).pattern);
    bank.slot (1).copyFrom (EveryStep (20, groove).pattern);
    SequencerSettings settings;
    settings.enabled = true;
    settings.rootNote = 0;
    std::vector<Event> events;
    double ppq = 0.0;
    long t = 0;
    auto emit = [&] (int s, bool on, int note, float velocity) { events.push_back ({ t + s, on, note, velocity }); };
    for (int block = 0; block < 100; ++block)
    {
        if (block == 5)
            bank.request (1); // in the middle of the first pass
        engine.process (true, ppq, bpm, sampleRate, 480, settings, bank, emit);
        ppq += 480 * bpm / 60.0 / sampleRate;
        t += 480;
    }
    std::vector<int> notes;
    for (const auto& e : events)
        if (e.on)
            notes.push_back (e.note);
    CHECK (notes.size() >= 8);
    CHECK (notes[0] == 0 && notes[1] == 1 && notes[2] == 2 && notes[3] == 3); // the first pass finishes
    CHECK (notes[4] == 20 && notes[5] == 21 && notes[6] == 22 && notes[7] == 23); // then the new one, from its start
    CHECK (bank.active() == 1);
}

static void textRoundTrips()
{
    Pattern p;
    p.set (0, { true, -7, 0 });
    p.set (1, { false, 24, 2 });
    p.set (15, { true, 12, 2 });
    Pattern q;
    CHECK (q.fromString (p.toString()));
    for (int i = 0; i < kMaxSteps; ++i)
        CHECK (q.get (i) == p.get (i));
    CHECK (! q.fromString ("1:0:1 nonsense"));
    CHECK (! q.fromString (""));
    // an out-of-range note or level from a damaged file is clamped
    CHECK (q.fromString ("1:99:9 1:-99:-3 1:0:1 1:0:1 1:0:1 1:0:1 1:0:1 1:0:1 1:0:1 1:0:1 1:0:1 1:0:1 1:0:1 1:0:1 1:0:1 1:0:1"));
    CHECK (q.get (0).note == kMaxStepNote && q.get (0).level == kNumStepLevels - 1);
    CHECK (q.get (1).note == kMinStepNote && q.get (1).level == 0);

    Groove odd;
    odd.length = 11;
    odd.rate = 3;
    odd.swing = 0.6180339f;
    odd.gate = 0.123456789f;
    Groove parsed;
    CHECK (Groove {}.toString() == "length 16 rate 1/16 swing 0.5 gate 0.5");
    CHECK (parsed.fromString (odd.toString()) && parsed.toString() == odd.toString());
    CHECK (! parsed.fromString ("length 5"));
    Pattern damaged;
    damaged.setSwing (std::nanf (""));
    damaged.setGate (std::nanf (""));
    CHECK (damaged.groove().swing == 0.5f && damaged.groove().gate == 0.5f);
}

static void randomLinesAreMusical()
{
    std::mt19937 rng (7);
    int totalOn = 0;
    for (int n = 0; n < 200; ++n)
    {
        Pattern p;
        p.randomize (rng);
        CHECK (p.get (0).gate && p.get (0).note == 0 && p.get (0).level == 2); // the first step is the root, accented
        for (int i = 0; i < kMaxSteps; ++i)
        {
            const auto s = p.get (i);
            totalOn += s.gate ? 1 : 0;
            CHECK (s.note >= kMinStepNote && s.note <= kMaxStepNote && s.level >= 0 && s.level < kNumStepLevels);
        }
    }
    CHECK (totalOn > 200 * 6 && totalOn < 200 * 11); // sparse, but not empty
}

// The MIDI clip has the notes, velocities, gate and swing the sequencer plays.
static void clipPlaysLikeTheSequencer()
{
    Groove groove;
    groove.swing = 0.58f;
    groove.gate = 0.4f;
    groove.length = 12;
    EveryStep scale (3, groove);
    Pattern& p = scale.pattern;
    p.set (2, { false, 0, 1 });
    p.set (5, { true, 9, 0 });
    p.set (6, { true, 7, 2 });
    const auto clip = patternToClip (p, 0);
    CHECK (clip.lengthBeats == 12 * 0.25);
    CHECK (clip.notes.size() == 11);

    const auto events = run (500, p, 3.0 * 4 * 0.25 * 60.0 / bpm, -1.0, 0.0, false);
    std::vector<Event> first;
    for (const auto& e : events)
        if (e.sample < (long) (clip.lengthBeats * 60.0 / bpm * sampleRate) - 10)
            first.push_back (e);
    // the first pass, on and off, in the same order with the same notes
    size_t k = 0;
    for (const auto& n : clip.notes)
    {
        CHECK (k + 1 < first.size());
        if (k + 1 >= first.size())
            break;
        CHECK (first[k].on && first[k].note == n.note);
        CHECK (first[k].velocity > 0.0f && midiVelocity (first[k].velocity) == n.velocity);
        CHECK (std::abs ((double) first[k].sample / sampleRate * bpm / 60.0 - n.start) < 1.0e-3);
        CHECK (std::abs ((double) first[k + 1].sample / sampleRate * bpm / 60.0 - n.end) < 1.0e-3);
        k += 2;
    }
    // the root is added
    CHECK (patternToClip (p, 40).notes.front().note == 43);
}

int main()
{
    timingIsBlockSizeIndependent();
    gateAndSwing();
    velocitiesFollowTheLevels();
    restsPlayNothing();
    lengthLoops();
    nothingHangs();
    silentWhenDisabledOrStopped();
    slotSwitchLandsOnTheLoop();
    textRoundTrips();
    randomLinesAreMusical();
    clipPlaysLikeTheSequencer();
    return finish ("sequencer");
}

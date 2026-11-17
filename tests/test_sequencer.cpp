// Copyright (C) 2026 Henrik Juhl. Part of BleepWerk, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

// Sequencer engine: timing independent of the host block size, swing and gate, no stuck notes
// after loops, jumps or stopping, layers of different lengths that stay in step, drift, hits
// spread evenly, the two velocities, text round trips, and MIDI clips that play like the sequencer,
// also with several layers at once (LayerClip.h).
#include "LayerClip.h"
#include "PatternLibrary.h"
#include "Sequencer.h"
#include "TestUtils.h"

#include <cmath>
#include <cstdlib>
#include <tuple>
#include <string>
#include <vector>

using namespace bleepwerk;

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
        engine.process (true, ppq, bpm, sampleRate, blockSize, settings, bank.slot (0), emit);
        ppq += blockSize * bpm / 60.0 / sampleRate;
        t += blockSize;
    }
    if (stopAtEnd)
        engine.process (false, 0.0, bpm, sampleRate, 64, settings, bank.slot (0), emit);
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
        p.set (i, { true, 0, i % 2 });
    const auto events = run (480, p, 1.0);
    int seen = 0;
    for (const auto& e : events)
        if (e.on)
        {
            CHECK (e.velocity == kSeqVelocities[seen % 2]);
            ++seen;
        }
    CHECK (seen == 8);
    CHECK (kSeqVelocities[kNormalLevel] < kSeqVelocities[kAccentLevel] && kSeqVelocities[kAccentLevel] <= 1.0f);
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
    engine.process (true, 0.0, bpm, sampleRate, 4800, settings, bank.slot (0), emit);
    CHECK (events == 0);
    settings.enabled = true;
    engine.process (false, 0.0, bpm, sampleRate, 4800, settings, bank.slot (0), emit);
    CHECK (events == 0);
    engine.process (true, -8.0, bpm, sampleRate, 480, settings, bank.slot (0), emit); // a pre-roll before beat 0
    CHECK (events == 0);
}

// Layers: two patterns play at once, each round its own length, so 4 and 3 steps meet again after 12.
static void layersFormAPolymeter()
{
    PatternBank bank;
    Groove four, three;
    four.length = 4;
    three.length = 3;
    EveryStep a (10, four), b (20, three);
    bank.slot (0).copyFrom (a.pattern);
    bank.slot (1).copyFrom (b.pattern);
    bank.setPlayingMask (3);
    CHECK (bank.isPlaying (0) && bank.isPlaying (1) && ! bank.isPlaying (2));

    SequencerEngine engines[2];
    SequencerSettings settings;
    settings.enabled = true;
    settings.rootNote = 0;
    std::vector<int> notes[2];
    double ppq = 0.0;
    for (int block = 0; block < 300; ++block) // three seconds, 24 steps
    {
        for (int layer = 0; layer < 2; ++layer)
            engines[layer].process (true, ppq, bpm, sampleRate, 480, settings, bank.slot (layer),
                                    [&] (int, bool on, int note, float) { if (on) notes[layer].push_back (note); });
        ppq += 480 * bpm / 60.0 / sampleRate;
    }
    CHECK (notes[0].size() >= 13 && notes[1].size() >= 13);
    for (int j = 0; j < 13; ++j)
    {
        CHECK (notes[0][(size_t) j] == 10 + j % 4);
        CHECK (notes[1][(size_t) j] == 20 + j % 3);
    }
    CHECK (notes[0][12] == 10 && notes[1][12] == 20); // together again at step 12

    // a layer switched on later joins in step, not from its own first step
    SequencerEngine late;
    std::vector<int> joined;
    ppq = 5 * 0.25; // five steps in
    late.process (true, ppq, bpm, sampleRate, 480, settings, bank.slot (0), [&] (int, bool on, int note, float) { if (on) joined.push_back (note); });
    CHECK (! joined.empty() && joined[0] == 10 + 5 % 4);

    bank.playOnly (5);
    CHECK (bank.playing() == 32u && bank.isPlaying (5) && ! bank.isPlaying (0));
    bank.setPlaying (0, true);
    bank.setPlaying (5, false);
    CHECK (bank.playing() == 1u);
}

// Drift: every time the pattern has played through it starts a step later.
static void driftMovesThePattern()
{
    Groove g;
    g.length = 4;
    g.drift = 1;
    CHECK (patternIndexAt (0, g) == 0 && patternIndexAt (3, g) == 3);
    CHECK (patternIndexAt (4, g) == 1 && patternIndexAt (7, g) == 0); // the second pass: 1 2 3 0
    CHECK (patternIndexAt (8, g) == 2);                               // the third: 2 3 0 1
    g.drift = -1;
    CHECK (patternIndexAt (4, g) == 3 && patternIndexAt (5, g) == 0);
    g.drift = 0;
    g.length = 5;
    CHECK (patternIndexAt (9, g) == 4);
    for (int j = 0; j < 200; ++j)
        for (int drift : { -4, -1, 0, 3 })
            for (int length : { 1, 3, 7, 16 })
                for (int vary : { 0, 2 })
                    for (int shape = 0; shape < kNumLengthShapes; ++shape)
                    {
                        Groove each;
                        each.length = length;
                        each.drift = drift;
                        each.vary = std::min (vary, length - 1);
                        each.shape = shape;
                        const int index = patternIndexAt (j, each);
                        CHECK (index >= 0 && index < length);
                    }

    Groove groove;
    groove.length = 4;
    groove.drift = 1;
    const EveryStep steps (0, groove);
    std::vector<int> notes;
    for (const auto& e : run (480, steps.pattern, 2.0))
        if (e.on)
            notes.push_back (e.note);
    const int expected[] = { 0, 1, 2, 3, 1, 2, 3, 0, 2, 3, 0, 1 };
    CHECK (notes.size() >= 12);
    for (size_t k = 0; k < 12 && k < notes.size(); ++k)
        CHECK (notes[k] == expected[k]);
}

// Vary: the length changes from pass to pass, between Length - Vary and Length, the way the
// shape says, and the series repeats.
static void varyChangesTheLength()
{
    Groove g;
    g.length = 8;
    CHECK (lengthPeriod (g) == 1 && lengthOfPass (0, g) == 8 && lengthOfPass (5, g) == 8); // off

    g.vary = 2;
    g.shape = (int) LengthShape::Walk; // 8 7 6 7 8 7 6 7 ...
    const int walk[] = { 8, 7, 6, 7, 8, 7, 6, 7 };
    for (int p = 0; p < 8; ++p)
        CHECK (lengthOfPass (p, g) == walk[p]);
    CHECK (lengthPeriod (g) == 4);

    g.shape = (int) LengthShape::Skip; // 8 8 6 8 8 6
    const int skip[] = { 8, 8, 6, 8, 8, 6 };
    for (int p = 0; p < 6; ++p)
        CHECK (lengthOfPass (p, g) == skip[p]);

    g.shape = (int) LengthShape::Random;
    bool different = false;
    for (int p = 0; p < 64; ++p)
    {
        CHECK (lengthOfPass (p, g) >= 6 && lengthOfPass (p, g) <= 8);
        CHECK (lengthOfPass (p, g) == lengthOfPass (p + 32, g)); // repeats after 32
        different = different || lengthOfPass (p, g) != lengthOfPass (0, g);
    }
    CHECK (different);

    // Vary can't take the length below one step
    g.length = 3;
    g.vary = 15;
    for (int shape = 0; shape < kNumLengthShapes; ++shape)
    {
        g.shape = shape;
        for (int p = 0; p < 100; ++p)
            CHECK (lengthOfPass (p, g) >= 1 && lengthOfPass (p, g) <= 3);
    }

    // played: passes of 4, 3, 4, 3 steps, so the step at the start of every pass is the first
    Groove skipGroove;
    skipGroove.length = 4;
    skipGroove.vary = 1;
    skipGroove.shape = (int) LengthShape::Walk; // 4 3 4 3 ...
    const EveryStep steps (0, skipGroove);
    std::vector<int> notes;
    for (const auto& e : run (480, steps.pattern, 4.0))
        if (e.on)
            notes.push_back (e.note);
    const int expected[] = { 0, 1, 2, 3, 0, 1, 2, 0, 1, 2, 3, 0, 1, 2 };
    CHECK (notes.size() >= 14);
    for (size_t k = 0; k < 14 && k < notes.size(); ++k)
        CHECK (notes[k] == expected[k]);

    // a layer switched on late is in step (the position, not the time since it was switched on)
    SequencerEngine late;
    SequencerSettings settings;
    settings.enabled = true;
    settings.rootNote = 0;
    std::vector<int> joined;
    late.process (true, 9 * 0.25, bpm, sampleRate, 480, settings, steps.pattern, [&] (int, bool on, int note, float) { if (on) joined.push_back (note); });
    CHECK (! joined.empty() && joined[0] == expected[9]);
}

// Hits spreads a number of steps evenly over the length and leaves the notes.
static void hitsSpreadEvenly()
{
    Pattern p;
    for (int i = 0; i < kMaxSteps; ++i)
        p.set (i, { true, i, kNormalLevel });
    Groove groove;
    groove.length = 8;
    p.setGroove (groove);
    p.setHits (3);
    CHECK (p.hits() == 3);
    for (int i = 0; i < 8; ++i)
        CHECK (p.get (i).gate == (i == 0 || i == 3 || i == 6)); // the tresillo
    for (int i = 8; i < kMaxSteps; ++i)
        CHECK (p.get (i).gate); // past the length is left alone
    for (int i = 0; i < kMaxSteps; ++i)
        CHECK (p.get (i).note == i);
    for (int length = 1; length <= kMaxSteps; ++length)
    {
        groove.length = length;
        p.setGroove (groove);
        for (int hits = 0; hits <= length; ++hits)
        {
            p.setHits (hits);
            CHECK (p.hits() == hits);
        }
        p.setHits (99);
        CHECK (p.hits() == length);
    }
}

static void textRoundTrips()
{
    Pattern p;
    p.set (0, { true, -7, 0 });
    p.set (1, { false, 24, 1 });
    p.set (15, { true, 12, 1 });
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
    odd.drift = -3;
    odd.vary = 4;
    odd.shape = (int) LengthShape::Skip;
    Groove parsed;
    CHECK (Groove {}.toString() == "length 16 rate 1/16 swing 0.5 gate 0.5 drift 0 vary 0 shape Walk");
    CHECK (parsed.fromString ("length 8 rate 1/8 swing 0.55 gate 0.4") && parsed.drift == 0); // text without a drift
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
        CHECK (p.get (0).gate && p.get (0).note == 0 && p.get (0).level == kAccentLevel); // the first step is the root, accented
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
    p.set (6, { true, 7, 1 });
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

//==============================================================================
// Several layers as one clip.

// Steps from firstNote upwards on every step, with an accent on every fourth and some rests.
static void fillLayer (Pattern& p, const Groove& groove, int firstNote, int restEvery)
{
    for (int i = 0; i < kMaxSteps; ++i)
        p.set (i, { restEvery <= 0 || i % restEvery != restEvery - 1, firstNote + i, i % 4 == 0 ? kAccentLevel : kNormalLevel });
    p.setGroove (groove);
}

static void playOnly (PatternBank& bank, std::initializer_list<int> slots)
{
    bank.setPlayingMask (0);
    for (int slot : slots)
        bank.setPlaying (slot, true);
}

struct TickNoteKey
{
    int64_t start, end;
    int note, velocity;
};

// What the engine plays for one layer, as a clip would hold it (ticks), for notes that start
// before lengthTicks. The layers use different pitches, so they can be compared one by one.
static std::vector<TickNoteKey> engineNotes (const Pattern& p, int64_t lengthTicks, int blockSize)
{
    const double seconds = (double) lengthTicks / kClipTicksPerQuarter * 60.0 / bpm + 2.0;
    const auto events = run (blockSize, p, seconds, -1.0, 0.0, false);
    auto toTicks = [] (long sample) { return (double) sample / sampleRate * bpm / 60.0 * kClipTicksPerQuarter; };
    std::vector<TickNoteKey> notes;
    std::vector<double> exactStart;
    for (const auto& e : events)
    {
        if (e.on)
        {
            if (toTicks (e.sample) >= (double) lengthTicks - 0.5)
                break;
            notes.push_back ({ (int64_t) std::llround (toTicks (e.sample)), -1, e.note, midiVelocity (e.velocity) });
        }
        else if (! notes.empty() && notes.back().end < 0)
            notes.back().end = (int64_t) std::llround (toTicks (e.sample));
    }
    for (auto& n : notes)
        n.end = std::min (n.end, lengthTicks);
    return notes;
}

static bool sameNotes (std::vector<TickNoteKey> a, std::vector<TickNoteKey> b, int64_t tolerance)
{
    auto order = [] (const TickNoteKey& x, const TickNoteKey& y) { return std::tie (x.start, x.note) < std::tie (y.start, y.note); };
    std::sort (a.begin(), a.end(), order);
    std::sort (b.begin(), b.end(), order);
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (std::llabs (a[i].start - b[i].start) > tolerance || std::llabs (a[i].end - b[i].end) > tolerance || a[i].note != b[i].note
            || a[i].velocity != b[i].velocity)
            return false;
    return true;
}

static std::vector<TickNoteKey> keysOf (const LayeredClip& clip)
{
    std::vector<TickNoteKey> keys;
    for (const auto& n : clip.notes)
        keys.push_back ({ n.start, n.end, n.note, n.velocity });
    return keys;
}

static bool noPitchOverlaps (const LayeredClip& clip)
{
    for (size_t i = 0; i < clip.notes.size(); ++i)
    {
        if (clip.notes[i].end <= clip.notes[i].start || clip.notes[i].end > clip.lengthTicks)
            return false;
        for (size_t k = i + 1; k < clip.notes.size(); ++k)
            if (clip.notes[k].note == clip.notes[i].note && clip.notes[k].start < clip.notes[i].end)
                return false;
    }
    return true;
}

// The clip has the notes the engine plays, with layers of different length, rate, swing, gate,
// drift and every shape of vary.
static void layeredClipMatchesTheEngine()
{
    PatternBank bank;
    Groove a;                       // 16 steps of 1/16, straight
    Groove b;                       // 12 steps of 1/16T with swing, drifting
    b.length = 12;
    b.rate = 4;
    b.swing = 0.6f;
    b.gate = 0.8f;
    b.drift = 1;
    Groove c;                       // 7 steps of 1/8, varied by Walk, gate to the next step
    c.length = 7;
    c.rate = 0;
    c.gate = 1.0f;
    c.vary = 3;
    c.shape = (int) LengthShape::Walk;
    Groove d;                       // 11 steps of 1/32 varied at random, drifting back
    d.length = 11;
    d.rate = 2;
    d.swing = 0.68f;
    d.gate = 0.3f;
    d.drift = -2;
    d.vary = 4;
    d.shape = (int) LengthShape::Random;
    Groove e;                       // 14 steps of 1/16, varied by Skip
    e.length = 14;
    e.vary = 5;
    e.shape = (int) LengthShape::Skip;
    e.drift = 3;
    fillLayer (bank.slot (0), a, 0, 0);
    fillLayer (bank.slot (1), b, 20, 5);
    fillLayer (bank.slot (2), c, 40, 3);
    fillLayer (bank.slot (3), d, 60, 0);
    fillLayer (bank.slot (4), e, 80, 4);
    playOnly (bank, { 0, 1, 2, 3, 4 });

    const int bars = 6;
    const auto clip = layersToClip (bank, 0, bars);
    CHECK (clip.lengthTicks == bars * kBarTicks);
    CHECK (noPitchOverlaps (clip));

    std::vector<TickNoteKey> expected;
    for (int block : { 480, 512 })
    {
        expected.clear();
        for (int layer = 0; layer < 5; ++layer)
        {
            const auto notes = engineNotes (bank.slot (layer), clip.lengthTicks, block);
            expected.insert (expected.end(), notes.begin(), notes.end());
        }
        CHECK (sameNotes (keysOf (clip), expected, 1));
    }
    CHECK (clip.notes.size() > 100);

    // only the layers that play are in it, and the root moves every note
    playOnly (bank, { 2 });
    const auto only = layersToClip (bank, 36, 4);
    CHECK (sameNotes (keysOf (only), [&]
                      {
                          auto notes = engineNotes (bank.slot (2), 4 * kBarTicks, 480);
                          for (auto& n : notes)
                              n.note += 36;
                          return notes;
                      }(),
                      1));
    playOnly (bank, {});
    CHECK (layersToClip (bank, 0, 8).notes.empty());
}

// The same pitch at the same time is one note: the later layer's velocity, the first end.
static void samePitchAtTheSameTimeIsOneNote()
{
    for (const bool laterIsAccent : { false, true })
    {
        PatternBank bank;
        Groove slow, quick; // one hit a beat: 1/16 steps, length 4
        slow.length = quick.length = 4;
        slow.gate = 0.5f;   // 120 ticks
        quick.gate = 0.25f; // 60 ticks
        for (auto* p : { &bank.slot (0), &bank.slot (3) })
        {
            p->clear();
            p->setGroove (p == &bank.slot (0) ? slow : quick);
        }
        bank.slot (0).set (0, { true, 60, laterIsAccent ? kNormalLevel : kAccentLevel });
        bank.slot (3).set (0, { true, 60, laterIsAccent ? kAccentLevel : kNormalLevel });
        playOnly (bank, { 0, 3 });

        const auto clip = layersToClip (bank, 0, 4);
        CHECK (clip.notes.size() == 16); // one a beat for four bars, not two
        for (size_t i = 0; i < clip.notes.size(); ++i)
        {
            CHECK (clip.notes[i].start == (int64_t) i * kClipTicksPerQuarter);
            CHECK (clip.notes[i].end == clip.notes[i].start + 60); // ends where the first of them ends
            CHECK (clip.notes[i].velocity == midiVelocity (kSeqVelocities[laterIsAccent ? kAccentLevel : kNormalLevel]));
        }
        CHECK (noPitchOverlaps (clip));
    }
}

// A pitch that starts while it still sounds cuts the earlier note off at that moment.
static void aNewNoteCutsTheOldOneOff()
{
    PatternBank bank;
    Groove a, b;
    a.length = 8; // a hit every two beats, held to the end of its step (a quarter of a beat)
    a.gate = 1.0f;
    b.length = 8; // a hit every beat, an eighth of a beat later, held for a sixteenth
    b.rate = 2;
    bank.slot (0).clear();
    bank.slot (0).setGroove (a);
    bank.slot (0).set (0, { true, 60, kNormalLevel });
    bank.slot (1).clear();
    bank.slot (1).setGroove (b);
    bank.slot (1).set (1, { true, 60, kAccentLevel });
    playOnly (bank, { 0, 1 });

    const auto clip = layersToClip (bank, 0, 2);
    CHECK (noPitchOverlaps (clip));
    // A sounds from 0 and is cut at 120 where B starts; B ends at 180. A's own end at 240 comes
    // when the pitch is silent again and is ignored.
    CHECK (clip.notes.size() >= 3);
    CHECK (clip.notes[0].start == 0 && clip.notes[0].end == 120 && clip.notes[0].velocity == midiVelocity (kSeqVelocities[kNormalLevel]));
    CHECK (clip.notes[1].start == 120 && clip.notes[1].end == 180 && clip.notes[1].velocity == midiVelocity (kSeqVelocities[kAccentLevel]));
    CHECK (clip.notes[2].start == kClipTicksPerQuarter + 120); // the next beat, with A silent
    // two beats on A starts again, and B's next note (a beat and an eighth in) cuts it off at 2040
    bool cutAgain = false;
    for (const auto& n : clip.notes)
        cutAgain = cutAgain || (n.start == 2 * kClipTicksPerQuarter && n.end == 2 * kClipTicksPerQuarter + 120);
    CHECK (cutAgain);
}

// The cycle is the number of whole bars after which the layers play exactly the same again.
static void cycleLengthIsFound()
{
    PatternBank bank;
    Groove sixteen, twelve;
    twelve.length = 12;
    fillLayer (bank.slot (0), sixteen, 0, 0);
    fillLayer (bank.slot (1), twelve, 30, 0);
    playOnly (bank, { 0 });
    CHECK (barsUntilRepeat (bank) == 1);
    playOnly (bank, { 0, 1 });
    CHECK (barsUntilRepeat (bank) == 3); // 16 and 12 steps meet after 48 steps, three bars
    playOnly (bank, {});
    CHECK (barsUntilRepeat (bank) == 0);

    // a layer with no hits has nothing to repeat
    bank.slot (1).clear();
    playOnly (bank, { 0, 1 });
    CHECK (barsUntilRepeat (bank) == 1);
    CHECK (bank.slot (1).hits() == 0);

    // Drift 1 on 16 steps has to go round all 16 places: 256 steps, 16 bars
    Groove drifting;
    drifting.drift = 1;
    fillLayer (bank.slot (2), drifting, 0, 3);
    playOnly (bank, { 2 });
    CHECK (barsUntilRepeat (bank) == 16);

    // too long for any choice
    Groove odd1, odd2, odd3;
    odd1.length = 13;
    odd1.drift = 1;
    odd2.length = 11;
    odd2.drift = 1;
    odd3.length = 15;
    odd3.rate = 3;
    fillLayer (bank.slot (3), odd1, 0, 0);
    fillLayer (bank.slot (4), odd2, 20, 0);
    fillLayer (bank.slot (5), odd3, 40, 0);
    playOnly (bank, { 3, 4, 5 });
    CHECK (barsUntilRepeat (bank) == -1);
}

// Playing twice the cycle gives the cycle twice, and nothing shorter than the cycle does.
static void cycleReallyRepeats()
{
    auto secondHalfIsTheFirst = [] (const PatternBank& bank, int bars)
    {
        const auto clip = layersToClip (bank, 0, 2 * bars);
        const int64_t shift = bars * kBarTicks;
        std::vector<TickNoteKey> first, second;
        for (const auto& n : clip.notes)
            (n.start < shift ? first : second).push_back ({ n.start < shift ? n.start : n.start - shift, n.end < shift || n.start < shift ? n.end : n.end - shift, n.note, n.velocity });
        // a note that starts in the first half and ends after it is cut by neither: same on both sides
        for (auto& n : second)
            n.end = std::min (n.end, clip.lengthTicks);
        return ! first.empty() && sameNotes (first, second, 0);
    };

    PatternBank bank;
    Groove a, b;
    b.length = 12;
    fillLayer (bank.slot (0), a, 0, 0);
    fillLayer (bank.slot (1), b, 12, 5); // shares pitches with A, so notes merge and cut
    playOnly (bank, { 0, 1 });
    CHECK (barsUntilRepeat (bank) == 3);
    CHECK (secondHalfIsTheFirst (bank, 3));
    CHECK (! secondHalfIsTheFirst (bank, 1));
    CHECK (! secondHalfIsTheFirst (bank, 2));

    Groove c, d;
    c.length = 12; // Skip: two full passes and a short one, with swing
    c.swing = 0.66f;
    c.vary = 3;
    c.shape = (int) LengthShape::Skip;
    d.length = 12; // drifting back two steps a pass, in triplets
    d.rate = 4;
    d.drift = -2;
    fillLayer (bank.slot (2), c, 0, 4);
    fillLayer (bank.slot (3), d, 5, 0);
    playOnly (bank, { 2, 3 });
    const int bars = barsUntilRepeat (bank);
    CHECK (bars > 0);
    CHECK (secondHalfIsTheFirst (bank, bars));
    CHECK (noPitchOverlaps (layersToClip (bank, 0, 2 * bars)));
}

static void factoryPatternsParse()
{
    for (int i = 0; i < kNumFactoryPatterns; ++i)
    {
        const auto& factory = kFactoryPatterns[i];
        Pattern pattern;
        CHECK (parseCompactPattern (factory.steps, pattern));
        CHECK (factory.length >= 1 && factory.length <= kMaxSteps);
        bool anyStep = false;
        for (int s = 0; s < factory.length; ++s)
            anyStep = anyStep || pattern.get (s).gate;
        CHECK (anyStep);
        CHECK (factory.rate >= 0 && factory.rate < kNumSeqRates && factory.gate >= 0.05f && factory.gate <= 1.0f);
        CHECK (std::abs (factory.drift) <= kMaxDrift && factory.vary >= 0 && factory.vary < factory.length);
        CHECK (factory.shape >= 0 && factory.shape < kNumLengthShapes);
        // the one-layer polymeters must differ from the bar: an odd length, or Drift or Vary
        if (i >= kFirstPolymeterPattern)
            CHECK (factory.length % 4 != 0 || factory.drift != 0 || factory.vary != 0);
        for (int j = 0; j < i; ++j) // names pick the preset back up from a saved project
            CHECK (std::string (kFactoryPatterns[j].name) != factory.name);
    }
}

static long gcdOf (long a, long b) { return b == 0 ? a : gcdOf (b, a % b); }

static void factorySetsArePolymeters()
{
    for (int i = 0; i < kNumFactorySets; ++i)
    {
        const auto& set = kFactorySets[i];
        CHECK (set.numLayers >= 2 && set.numLayers <= kMaxSetLayers && set.numLayers <= kNumPatternSlots);
        long meet = 1;
        for (int l = 0; l < set.numLayers; ++l)
        {
            const auto& layer = set.layers[l];
            Pattern pattern;
            CHECK (parseCompactPattern (layer.steps, pattern));
            CHECK (layer.length >= 1 && layer.length <= kMaxSteps);
            CHECK (layer.rate >= 0 && layer.rate < kNumSeqRates && layer.gate >= 0.05f && layer.gate <= 1.0f);
            bool anyStep = false;
            for (int s = 0; s < layer.length; ++s)
                anyStep = anyStep || pattern.get (s).gate;
            CHECK (anyStep);
            for (int s = 0; s < layer.length; ++s) // notes inside the step range
                CHECK (pattern.get (s).note >= kMinStepNote && pattern.get (s).note <= kMaxStepNote);
            meet = meet / gcdOf (meet, layer.length) * layer.length;
        }
        CHECK (meet > 16); // the layers need more than one pass of the longest to line up again
        for (int j = 0; j < kNumFactoryPatterns; ++j)
            CHECK (std::string (kFactoryPatterns[j].name) != set.name);
        for (int j = 0; j < i; ++j)
            CHECK (std::string (kFactorySets[j].name) != set.name);
    }
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
    layersFormAPolymeter();
    driftMovesThePattern();
    varyChangesTheLength();
    hitsSpreadEvenly();
    textRoundTrips();
    randomLinesAreMusical();
    clipPlaysLikeTheSequencer();
    layeredClipMatchesTheEngine();
    samePitchAtTheSameTimeIsOneNote();
    aNewNoteCutsTheOldOneOff();
    cycleLengthIsFound();
    cycleReallyRepeats();
    factoryPatternsParse();
    factorySetsArePolymeters();
    return finish ("sequencer");
}

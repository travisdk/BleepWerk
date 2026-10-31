// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>
#include <locale>
#include <random>
#include <sstream>
#include <string>
#include <vector>

// Step sequencer, adapted from SubForge's: a 16-step pattern (on or off, a note and an accent per
// step) played against the host timeline, eight patterns A-H. A step is a short strike, so there
// is no slide: every note ends at its gate. Any of the eight can play at the same time, each as
// a layer of its own length, so two layers of 16 and 12 steps first meet again after 48 (a
// polymeter). A pattern's Drift moves it on a few steps every time it has played through, so a
// sparse line slowly shifts against the beat, and Vary makes its length change from one pass to
// the next, so it never settles. JUCE-free so it can be tested on its own.
namespace bleepforge
{

constexpr int kMaxSteps = 16;
constexpr int kMinStepNote = -12, kMaxStepNote = 24; // semitones from the root

// Pattern slots A-H. Each can play as a layer; the Pattern parameter, which a host can automate,
// makes one of them the only one that plays.
constexpr int kNumPatternSlots = 8;

// Sequencer rate choices, in quarter-note beats per step.
inline constexpr float kSeqRateBeats[] = { 0.5f, 0.25f, 0.125f, 1.0f / 3.0f, 1.0f / 6.0f };
inline constexpr const char* kSeqRateNames[] = { "1/8", "1/16", "1/32", "1/8T", "1/16T" };
constexpr int kNumSeqRates = (int) std::size (kSeqRateBeats);

// A step's velocity: normal or accent.
constexpr int kNumStepLevels = 2;
inline constexpr float kSeqVelocities[kNumStepLevels] = { 0.65f, 1.0f };
constexpr int kNormalLevel = 0, kAccentLevel = 1;
constexpr int kMaxDrift = 4; // steps a pattern moves on per pass, either way

// How Vary moves the length from pass to pass (always between Length - Vary and Length).
enum class LengthShape { Walk, Random, Skip };
inline constexpr const char* kLengthShapeNames[] = { "Walk", "Random", "Skip" };
constexpr int kNumLengthShapes = (int) std::size (kLengthShapeNames);

struct Step
{
    bool gate = true;
    int note = 0;
    int level = kNormalLevel; // index into kSeqVelocities

    bool operator== (const Step& other) const { return gate == other.gate && note == other.note && level == other.level; }
};

// How a pattern plays: its length, the length of each step, swing and gate. Each
// slot has its own, as a pattern file does.
struct Groove
{
    int length = kMaxSteps;
    int rate = 1;        // index into kSeqRateBeats, 1/16
    float swing = 0.5f;  // 0.5 straight .. 0.75 heavy shuffle; delays every second step
    float gate = 0.5f;   // fraction of the step a note is held
    int drift = 0;       // steps the pattern moves on every time it has played through, -4..4
    int vary = 0;        // how many steps shorter than length a pass may be, 0 (off) .. length - 1
    int shape = 0;       // index into kLengthShapeNames

    double stepBeats() const { return kSeqRateBeats[std::clamp (rate, 0, kNumSeqRates - 1)]; }

    // "length 16 rate 1/16 swing 0.5 gate 0.5", stored with the project. Numbers keep
    // full float precision and always use a decimal point, whatever the host's locale.
    std::string toString() const
    {
        std::ostringstream out;
        out.imbue (std::locale::classic());
        out.precision (9);
        out << "length " << length << " rate " << kSeqRateNames[std::clamp (rate, 0, kNumSeqRates - 1)]
            << " swing " << swing << " gate " << gate << " drift " << drift << " vary " << vary << " shape "
            << kLengthShapeNames[std::clamp (shape, 0, kNumLengthShapes - 1)];
        return out.str();
    }

    bool fromString (const std::string& text)
    {
        std::istringstream in (text);
        in.imbue (std::locale::classic());
        Groove parsed;
        int found = 0;
        for (std::string key, value; in >> key >> value;)
        {
            std::istringstream number (value);
            number.imbue (std::locale::classic());
            if (key == "length" && number >> parsed.length)
                found |= 1;
            else if (key == "rate")
            {
                for (int i = 0; i < kNumSeqRates; ++i)
                    if (value == kSeqRateNames[i])
                    {
                        parsed.rate = i;
                        found |= 2;
                    }
            }
            else if (key == "swing" && number >> parsed.swing)
                found |= 4;
            else if (key == "gate" && number >> parsed.gate)
                found |= 8;
            else if (key == "drift" && number >> parsed.drift) // not in older text: stays 0
                parsed.drift = std::clamp (parsed.drift, -kMaxDrift, kMaxDrift);
            else if (key == "vary" && number >> parsed.vary)
                parsed.vary = std::clamp (parsed.vary, 0, kMaxSteps - 1);
            else if (key == "shape")
            {
                for (int i = 0; i < kNumLengthShapes; ++i)
                    if (value == kLengthShapeNames[i])
                        parsed.shape = i;
            }
        }
        if (found != 15)
            return false;
        *this = parsed;
        return true;
    }
};

// Steps packed into atomics so the editor can change them while the audio thread
// plays. The groove fields are atomics of their own for the same reason.
class Pattern
{
public:
    Pattern() { setDefault(); }

    Step get (int index) const { return unpack (steps[(size_t) index].load (std::memory_order_relaxed)); }
    void set (int index, const Step& step) { steps[(size_t) index].store (pack (step), std::memory_order_relaxed); }

    Groove groove() const { return { grooveLength.load(), grooveRate.load(), grooveSwing.load(), grooveGate.load(), grooveDrift.load(), grooveVary.load(), grooveShape.load() }; }
    void setGroove (const Groove& g)
    {
        setLength (g.length);
        setRate (g.rate);
        setSwing (g.swing);
        setGate (g.gate);
        setDrift (g.drift);
        setVary (g.vary);
        setShape (g.shape);
    }
    void setLength (int value) { grooveLength = std::clamp (value, 1, kMaxSteps); }
    void setRate (int value) { grooveRate = std::clamp (value, 0, kNumSeqRates - 1); }
    // a value that is not a number (from a damaged file) gives the default, as clamp would keep it
    void setSwing (float value) { grooveSwing = std::isnan (value) ? Groove {}.swing : std::clamp (value, 0.5f, 0.75f); }
    void setGate (float value) { grooveGate = std::isnan (value) ? Groove {}.gate : std::clamp (value, 0.05f, 1.0f); }
    void setDrift (int value) { grooveDrift = std::clamp (value, -kMaxDrift, kMaxDrift); }
    void setVary (int value) { grooveVary = std::clamp (value, 0, kMaxSteps - 1); }
    void setShape (int value) { grooveShape = std::clamp (value, 0, kNumLengthShapes - 1); }

    // How many of the first length steps are on, and spreading a number of hits evenly over them
    // (3 of 8 is the tresillo): the notes stay where they are, steps that come on keep theirs.
    int hits() const
    {
        int count = 0;
        for (int i = 0; i < std::clamp (grooveLength.load(), 1, kMaxSteps); ++i)
            count += get (i).gate ? 1 : 0;
        return count;
    }
    void setHits (int count)
    {
        const int length = std::clamp (grooveLength.load(), 1, kMaxSteps);
        count = std::clamp (count, 0, length);
        for (int i = 0; i < length; ++i)
        {
            auto step = get (i);
            step.gate = (i * count) % length < count; // exactly count of them, as evenly as it goes
            set (i, step);
        }
    }

    // A short off-beat line so the sequencer does something sensible straight away.
    void setDefault()
    {
        const Step line[kMaxSteps] = {
            { true, 0, 1 },  { false, 0, 0 }, { true, 12, 0 }, { false, 0, 0 }, { true, 0, 0 },  { false, 0, 0 }, { true, 7, 0 },  { false, 0, 0 },
            { true, 0, 1 },  { false, 0, 0 }, { true, 12, 0 }, { true, 10, 0 }, { true, 0, 0 },  { false, 0, 0 }, { true, 3, 0 },  { true, 7, 0 },
        };
        for (int i = 0; i < kMaxSteps; ++i)
            set (i, line[i]);
    }

    // every step off: a bare grid to write a pattern on
    void clear()
    {
        for (int i = 0; i < kMaxSteps; ++i)
            set (i, { false, 0, kNormalLevel });
    }

    void copyFrom (const Pattern& other)
    {
        for (int i = 0; i < kMaxSteps; ++i)
            set (i, other.get (i));
        setGroove (other.groove());
    }

    // "gate:note:level" per step, separated by spaces, e.g. "1:0:2 0:0:1 1:12:0 ..."
    std::string toString() const
    {
        std::string text;
        for (int i = 0; i < kMaxSteps; ++i)
        {
            const auto s = get (i);
            text += (i > 0 ? " " : "") + std::to_string (s.gate) + ":" + std::to_string (s.note) + ":" + std::to_string (s.level);
        }
        return text;
    }

    bool fromString (const std::string& text)
    {
        Step parsed[kMaxSteps];
        std::istringstream in (text);
        for (auto& step : parsed)
        {
            int gate = 0, note = 0, level = 0;
            char c1 = 0, c2 = 0;
            if (! (in >> gate >> c1 >> note >> c2 >> level) || c1 != ':' || c2 != ':')
                return false;
            step = { gate != 0, std::clamp (note, kMinStepNote, kMaxStepNote), std::clamp (level, 0, kNumStepLevels - 1) };
        }
        for (int i = 0; i < kMaxSteps; ++i)
            set (i, parsed[i]);
        return true;
    }

    // A random line for a melodic strike: sparse, with the steps on the beat the most likely,
    // mostly on the root and the notes of a minor scale, now and then an octave up, and a few
    // accents.
    void randomize (std::mt19937& rng)
    {
        static constexpr int scales[3][8] = {
            { 0, 3, 5, 7, 10, -1, -1, -1 }, // minor pentatonic
            { 0, 2, 3, 5, 7, 8, 10, -1 },   // natural minor
            { 0, 1, 3, 5, 7, 8, 10, -1 },   // phrygian
        };
        std::uniform_real_distribution<float> uniform (0.0f, 1.0f);
        auto chance = [&] (float p) { return uniform (rng) < p; };
        const auto& scale = scales[std::uniform_int_distribution<int> (0, 2) (rng)];
        const int scaleSize = (int) (std::find (std::begin (scale), std::end (scale), -1) - std::begin (scale));

        for (int i = 0; i < kMaxSteps; ++i)
        {
            Step s;
            s.gate = i == 0 || chance (i % 4 == 0 ? 0.85f : i % 2 == 0 ? 0.6f : 0.4f);
            s.note = (i == 0 || chance (0.5f)) ? 0 : scale[std::uniform_int_distribution<int> (1, scaleSize - 1) (rng)];
            if (i > 0 && chance (0.15f))
                s.note += 12;
            s.level = i == 0 || chance (0.2f) ? kAccentLevel : kNormalLevel;
            set (i, s);
        }
    }

private:
    static uint32_t pack (const Step& s)
    {
        return (s.gate ? 1u : 0u) | ((uint32_t) std::clamp (s.level, 0, kNumStepLevels - 1) << 1) | ((uint32_t) (s.note + 128) << 8);
    }
    static Step unpack (uint32_t v)
    {
        return { (v & 1u) != 0, (int) ((v >> 8) & 0xffu) - 128, std::min ((int) ((v >> 1) & 3u), kNumStepLevels - 1) };
    }

    std::array<std::atomic<uint32_t>, kMaxSteps> steps;
    std::atomic<int> grooveLength { kMaxSteps }, grooveRate { 1 }, grooveDrift { 0 }, grooveVary { 0 }, grooveShape { 0 };
    std::atomic<float> grooveSwing { 0.5f }, grooveGate { 0.5f };
};

// The eight pattern slots, which of them plays (any number, each as a layer) and which one the
// panel shows and edits. Changes may come from any thread.
class PatternBank
{
public:
    Pattern& slot (int index) { return slots[(size_t) std::clamp (index, 0, kNumPatternSlots - 1)]; }
    const Pattern& slot (int index) const { return slots[(size_t) std::clamp (index, 0, kNumPatternSlots - 1)]; }
    Pattern& activePattern() { return slot (active()); }
    const Pattern& activePattern() const { return slot (active()); }

    int active() const { return editSlot.load(); } // the slot the panel edits
    void select (int index) { editSlot.store (std::clamp (index, 0, kNumPatternSlots - 1)); }

    bool isPlaying (int index) const { return (playMask.load() >> std::clamp (index, 0, kNumPatternSlots - 1) & 1u) != 0; }
    void setPlaying (int index, bool on)
    {
        const uint32_t bit = 1u << std::clamp (index, 0, kNumPatternSlots - 1);
        on ? (void) playMask.fetch_or (bit) : (void) playMask.fetch_and (~bit);
    }
    uint32_t playing() const { return playMask.load(); }
    void setPlayingMask (uint32_t mask) { playMask.store (mask & ((1u << kNumPatternSlots) - 1u)); }
    void playOnly (int index) { setPlayingMask (1u << std::clamp (index, 0, kNumPatternSlots - 1)); }

private:
    std::array<Pattern, kNumPatternSlots> slots;
    std::atomic<int> editSlot { 0 };
    std::atomic<uint32_t> playMask { 1u };
};

struct SequencerSettings
{
    bool enabled = false;
    int rootNote = 36;
};

// Start of step k in beats from the timeline's zero; swing delays every second step.
inline double stepStartBeats (int64_t k, const Groove& groove)
{
    const double beats = groove.stepBeats();
    const auto pairStart = (double) (k - (k & 1)) * beats;
    return (k & 1) ? pairStart + 2.0 * beats * groove.swing : pairStart;
}

// How many steps pass number p plays: the pattern's Length, or fewer by up to Vary, as the shape
// goes. Walk steps down and up again (16, 15, 14, 15, 16...), Random picks from a fixed series
// that repeats every 32 passes, so a bounce comes out the same each time, and Skip plays two
// full passes and then a short one. The series repeat after lengthPeriod() passes.
inline int lengthPeriod (const Groove& groove)
{
    const int vary = std::clamp (groove.vary, 0, std::clamp (groove.length, 1, kMaxSteps) - 1);
    if (vary == 0)
        return 1;
    switch ((LengthShape) std::clamp (groove.shape, 0, kNumLengthShapes - 1))
    {
        case LengthShape::Walk: return 2 * vary;
        case LengthShape::Random: return 32;
        case LengthShape::Skip: return 3;
    }
    return 1;
}

inline int lengthOfPass (int64_t pass, const Groove& groove)
{
    const int length = std::clamp (groove.length, 1, kMaxSteps);
    const int vary = std::clamp (groove.vary, 0, length - 1);
    if (vary == 0)
        return length;
    const int period = lengthPeriod (groove);
    const int p = (int) (((pass % period) + period) % period);
    switch ((LengthShape) std::clamp (groove.shape, 0, kNumLengthShapes - 1))
    {
        case LengthShape::Walk: return length - (p <= vary ? p : 2 * vary - p);
        case LengthShape::Random:
        {
            uint32_t h = (uint32_t) p * 2654435761u + 0x9e3779b9u; // a fixed scramble of the pass number
            h ^= h >> 15;
            h *= 2246822519u;
            h ^= h >> 13;
            return length - (int) (h % (uint32_t) (vary + 1));
        }
        case LengthShape::Skip: return p == 2 ? length - vary : length;
    }
    return length;
}

// Which step of a pattern plays at step j of the timeline: they go round, every pass as long as
// lengthOfPass says, and every time round the pattern has moved on by its drift (a pass cut
// short plays a window of the pattern, from where the drift has got to).
inline int patternIndexAt (int64_t j, const Groove& groove)
{
    const int length = std::clamp (groove.length, 1, kMaxSteps);
    const int period = lengthPeriod (groove);
    int64_t cycleSteps = 0;
    for (int p = 0; p < period; ++p)
        cycleSteps += lengthOfPass (p, groove);

    const int64_t cycle = j >= 0 ? j / cycleSteps : -((-j + cycleSteps - 1) / cycleSteps);
    int64_t rest = j - cycle * cycleSteps;
    int pass = 0;
    while (pass < period - 1 && rest >= lengthOfPass (pass, groove))
        rest -= lengthOfPass (pass++, groove);

    const int64_t passNumber = cycle * period + pass;
    const int64_t shifted = rest + passNumber * groove.drift;
    return (int) (((shifted % length) + length) % length);
}

// Turns one pattern into note on/off events for one audio block. Steps are counted from the
// start of the timeline (beat 0), so a layer that is switched on joins in step with the others,
// and loops and jumps land on the right step. Swing follows the step grid of the timeline.
// Each layer has an engine of its own.
class SequencerEngine
{
public:
    void reset()
    {
        soundingNote = -1;
        pendingOffPpq = -1.0;
        lastPpqEnd = -1.0;
        currentStep.store (-1);
    }

    int getCurrentStep() const { return currentStep.load(); }

    // emit (int sampleOffset, bool noteOn, int note, float velocity)
    template <typename Emit>
    void process (bool playing, double ppqStart, double bpm, double sampleRate, int numSamples,
                  const SequencerSettings& settings, const Pattern& pattern, Emit&& emit)
    {
        // Silent before beat 0 (a host's pre-roll), so the pattern starts on its first
        // step at bar 1, and note times are never negative.
        const double ppqPerSample = bpm / 60.0 / sampleRate;
        const double ppqEnd = ppqStart + numSamples * ppqPerSample;
        if (! settings.enabled || ! playing || bpm <= 0.0 || numSamples <= 0 || ppqEnd <= 0.0)
        {
            if (soundingNote >= 0)
                emit (0, false, soundingNote, 0.0f);
            reset();
            return;
        }

        // the host jumped or looped: stop what is sounding and pick up from here
        if (lastPpqEnd >= 0.0 && std::abs (ppqStart - lastPpqEnd) > ppqPerSample * 4.0)
        {
            if (soundingNote >= 0)
                emit (0, false, soundingNote, 0.0f);
            soundingNote = -1;
            pendingOffPpq = -1.0;
        }
        lastPpqEnd = ppqEnd;

        // An event happens on the first sample at or after its beat position. Deciding
        // block membership by that sample keeps timing independent of the block size.
        auto sampleFor = [&] (double ppq) { return (int64_t) std::ceil ((ppq - ppqStart) / ppqPerSample - 1.0e-6); };
        auto flushNoteOffUpTo = [&] (int64_t sample) // note-offs at or before sample
        {
            if (pendingOffPpq >= 0.0 && soundingNote >= 0)
            {
                const auto offSample = sampleFor (pendingOffPpq);
                if (offSample <= sample)
                {
                    emit ((int) std::clamp<int64_t> (offSample, 0, numSamples - 1), false, soundingNote, 0.0f);
                    soundingNote = -1;
                    pendingOffPpq = -1.0;
                }
            }
        };

        const auto groove = pattern.groove();
        const double beats = groove.stepBeats();
        const auto first = (int64_t) std::floor (ppqStart / beats) - 1;
        const auto last = (int64_t) std::ceil (ppqEnd / beats) + 1;

        for (int64_t j = first; j <= last; ++j)
        {
            const double start = stepStartBeats (j, groove);
            const auto sample = sampleFor (start);
            if (start < 0.0 || sample < 0 || sample >= numSamples)
                continue;

            flushNoteOffUpTo (sample);
            const int index = patternIndexAt (j, groove);
            currentStep.store (index);
            const auto step = pattern.get (index);

            // a step ends the one before it, whatever it is
            if (soundingNote >= 0)
                emit ((int) sample, false, soundingNote, 0.0f);
            soundingNote = -1;
            pendingOffPpq = -1.0;
            if (! step.gate)
                continue;

            const int note = std::clamp (settings.rootNote + step.note, 0, 127);
            emit ((int) sample, true, note, kSeqVelocities[std::clamp (step.level, 0, kNumStepLevels - 1)]);
            soundingNote = note;
            pendingOffPpq = start + (stepStartBeats (j + 1, groove) - start) * std::clamp ((double) groove.gate, 0.05, 1.0);
        }

        flushNoteOffUpTo (numSamples - 1);
    }

private:
    int soundingNote = -1;
    double pendingOffPpq = -1.0;
    double lastPpqEnd = -1.0;
    std::atomic<int> currentStep { -1 };
};

//==============================================================================
// One pass of a pattern as notes for a MIDI clip, played as the sequencer plays it: the same
// velocities, gate and swing.
struct ClipNote
{
    double start, end; // beats from the start of the clip
    int note;
    int velocity;      // 1..127
};

struct PatternClip
{
    std::vector<ClipNote> notes;
    double lengthBeats = 0.0;
};

inline int midiVelocity (float velocity) { return std::clamp ((int) std::lround (velocity * 127.0f), 1, 127); }

inline PatternClip patternToClip (const Pattern& pattern, int rootNote)
{
    PatternClip clip;
    const auto groove = pattern.groove();
    const int length = std::clamp (groove.length, 1, kMaxSteps);
    clip.lengthBeats = length * groove.stepBeats();

    for (int i = 0; i < length; ++i)
    {
        const auto step = pattern.get (i);
        if (! step.gate)
            continue;
        const double start = stepStartBeats (i, groove);
        const double duration = stepStartBeats (i + 1, groove) - start;
        const double end = std::min (start + duration * std::clamp ((double) groove.gate, 0.05, 1.0), clip.lengthBeats);
        clip.notes.push_back ({ start, end, std::clamp (rootNote + step.note, 0, 127),
                                midiVelocity (kSeqVelocities[std::clamp (step.level, 0, kNumStepLevels - 1)]) });
    }
    return clip;
}

} // namespace bleepforge

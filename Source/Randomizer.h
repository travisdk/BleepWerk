// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "FactoryPresets.h"
#include "Sequencer.h"
#include "Euclid.h"

#include <cstdint>
#include <cstring>
#include <vector>

// The dice: a random sound for the selected engine and a random pattern. Neither picks
// uniformly over a parameter's whole range, which mostly gives noise, endless rings or a
// fully wet delay; each draws from ranges that suit short percussive tones, with the effects
// off more often than on. Runs on the message thread; the generator is our own so a seed
// gives the same result on every platform (the tests rely on it). JUCE-free.
namespace bleepforge
{
// xorshift64*: small, fast and the same everywhere, unlike std::uniform_*_distribution.
class DiceRng
{
public:
    explicit DiceRng (uint64_t seed) : state (seed != 0 ? seed : 0x9e3779b97f4a7c15ull) {}

    uint64_t next()
    {
        state ^= state >> 12;
        state ^= state << 25;
        state ^= state >> 27;
        return state * 0x2545f4914f6cdd1dull;
    }

    float uniform() { return (float) (next() >> 40) * (1.0f / 16777216.0f); } // 0..1, below 1
    float between (float lo, float hi) { return lo + (hi - lo) * uniform(); }
    float logBetween (float lo, float hi) { return lo * std::pow (hi / lo, uniform()); }
    int below (int n) { return (int) (uniform() * (float) n); }
    bool chance (float p) { return uniform() < p; }

    // an index picked with the given relative weights
    template <size_t N>
    int pick (const float (&weights)[N])
    {
        float total = 0.0f;
        for (float w : weights)
            total += w;
        float x = uniform() * total;
        for (size_t i = 0; i < N; ++i)
            if ((x -= weights[i]) < 0.0f)
                return (int) i;
        return (int) N - 1;
    }

private:
    uint64_t state;
};

// A random sound for one engine: its own parameters, voices, Chaos and the effects. The
// engine choice and the other engine's parameters are left out, and so is the output gain,
// which the caller matches with matchedOutputDb().
inline std::vector<PresetValue> randomSound (Engine engine, DiceRng& rng)
{
    std::vector<PresetValue> v;
    auto set = [&v] (const char* id, float value)
    {
        const auto* p = findSoundParam (id);
        v.push_back ({ id, p != nullptr ? std::clamp (value, p->min, p->max) : value });
    };
    auto round = [] (float value, float to) { return std::round (value / to) * to; };

    if (engine == Engine::Bleep)
    {
        constexpr float waves[] = { 3, 2.5f, 2, 2.5f }; // sine, triangle, pulse, FM
        set ("bleepWave", (float) rng.pick (waves));
        set ("bleepPulseWidth", round (rng.between (0.1f, 0.9f), 0.01f));
        // mostly harmonic ratios, sometimes an inharmonic, metallic one
        constexpr float ratios[] = { 0.5f, 1, 1.5f, 2, 3, 3.5f, 4, 5, 7 };
        set ("bleepFmRatio", rng.chance (0.7f) ? ratios[rng.below ((int) std::size (ratios))] : round (rng.between (0.5f, 8.0f), 0.01f));
        set ("bleepFmAmount", round (rng.between (0.05f, 0.8f), 0.01f));

        // a zap down to the note, a small blip, or a rise into it
        constexpr float sweeps[] = { 4, 4, 2 };
        const int sweep = rng.pick (sweeps);
        set ("bleepPitchAmount", std::round (sweep == 0 ? rng.between (7, 36) : sweep == 1 ? rng.between (-3, 7) : rng.between (-24, -5)));
        set ("bleepPitchDecay", round (rng.logBetween (3, 120), 0.1f));

        set ("bleepAttack", round (rng.chance (0.8f) ? rng.between (0, 1) : rng.logBetween (1, 10), 0.1f));
        set ("bleepDecay", std::round (rng.chance (0.85f) ? rng.logBetween (20, 700) : rng.logBetween (700, 1500)));
        set ("bleepRelease", std::round (rng.logBetween (5, 150)));
        set ("bleepNoise", rng.chance (0.6f) ? 0.0f : round (rng.between (0, 0.4f), 0.01f));
    }
    else if (engine == Engine::Foley)
    {
        set ("foleyExciter", (float) rng.below (kNumFoleyExciters));
        set ("foleyLength", round (rng.logBetween (0.1f, 30), 0.01f));
        set ("foleyMaterial", round (rng.uniform(), 0.01f));
        set ("foleySize", round (rng.between (0.1f, 0.9f), 0.01f));
        // never the seconds-long rings at the bottom of the range: a percussive part
        set ("foleyDamping", round (rng.between (0.25f, 0.9f), 0.01f));
        set ("foleyBrightness", round (rng.between (0.2f, 0.9f), 0.01f));
        set ("foleySweep", rng.chance (0.7f) ? 0.0f : std::round (rng.between (-12, 12)));
    }
    else
    {
        // from a short burst to a sound that lasts a bar, and still more often than moving
        set ("machineType", (float) rng.below (kNumMachineTypes));
        set ("machineLength", std::round (rng.logBetween (60, 2500)));
        set ("machineTone", round (rng.between (0.1f, 0.9f), 0.01f));
        set ("machineMotion", rng.chance (0.4f) ? 0.0f : round (rng.between (0.05f, 0.7f), 0.01f));
        set ("machineFocus", round (rng.between (0.1f, 1.0f), 0.01f));
        set ("machineMetal", round (rng.between (0.2f, 1.0f), 0.01f));
    }

    const bool mono = rng.chance (0.2f);
    set ("voiceMode", mono ? 0.0f : 1.0f);
    set ("glide", mono && rng.chance (0.5f) ? std::round (rng.logBetween (10, 150)) : 0.0f);

    set ("chaosAmount", round (rng.between (0, 0.6f), 0.01f));
    set ("chaosPitch", round (rng.between (0, 0.4f), 0.01f));
    for (const char* id : { "chaosDecay", "chaosTone", "chaosPan", "chaosVelocity" })
        set (id, round (rng.uniform(), 0.01f));

    // the filter: each type in the band where it shapes a short tone instead of muting it
    constexpr float filters[] = { 4, 3, 2, 1.5f }; // off, low-, band-, high-pass
    const int filter = rng.pick (filters);
    set ("filterType", (float) filter);
    const float cutoff = filter == 1 ? rng.logBetween (800, 12000) : filter == 2 ? rng.logBetween (400, 5000)
                       : filter == 3 ? rng.logBetween (100, 2000) : soundDefault ("filterCutoff");
    set ("filterCutoff", std::round (cutoff));
    set ("filterResonance", round (rng.between (0, 0.7f), 0.01f));

    const bool crush = rng.chance (0.15f);
    set ("crushBits", crush ? std::round (rng.between (4, 12)) : kCrushBitsOff);
    set ("crushRate", crush && rng.chance (0.5f) ? std::round (rng.logBetween (4000, 20000)) : kCrushRateOff);

    const bool delay = rng.chance (0.6f);
    set ("delayTime", (float) rng.below (kNumDelayTimes));
    set ("delayFeedback", round (rng.between (0.2f, 0.6f), 0.01f));
    set ("delayMix", delay ? round (rng.between (0.1f, 0.35f), 0.01f) : 0.0f);

    const bool room = rng.chance (0.6f);
    set ("roomSize", round (rng.between (0.2f, 0.8f), 0.01f));
    set ("roomMix", room ? round (rng.between (0.1f, 0.35f), 0.01f) : 0.0f);
    return v;
}

// A random sound with its output gain matched like a factory preset's. A sound that stays
// too quiet even at the most gain (a narrow band-pass far from the note, say) is rolled
// again. current (id) gives the parameters the dice leave alone, except the engine.
template <typename Get>
std::vector<PresetValue> randomMatchedSound (Engine engine, DiceRng& rng, Get&& current)
{
    std::vector<PresetValue> values;
    for (int attempt = 0; attempt < 8; ++attempt)
    {
        values = randomSound (engine, rng);
        const auto patch = readPatch ([&] (const char* id)
                                      {
                                          if (std::strcmp (id, "engine") == 0)
                                              return (float) engine;
                                          for (const auto& v : values)
                                              if (std::strcmp (v.id, id) == 0)
                                                  return v.value;
                                          return current (id);
                                      });
        const float outputDb = matchedOutputDb (patch);
        const auto level = measurePatch (patch);
        const float gain = std::pow (10.0f, outputDb / 20.0f);
        values.push_back ({ "outputGain", outputDb });
        if (level.loudest * gain > 0.1f || level.peak * gain > 0.6f)
            break;
    }
    return values;
}

// Replaces the whole pattern with a random one in rows firstRow .. firstRow + numRows - 1
// (the rows in view), keeping the grid's length. A few rows carry the pattern, led by a root
// note, each on a rotated Euclidean rhythm; hits on downbeats are harder and more often certain.
// The new pattern is built aside and copied in, so the audio thread never plays a half-cleared
// grid or the default length.
inline void randomPattern (Grid& target, int scale, int firstRow, int numRows, DiceRng& rng)
{
    const int length = target.getLength();
    const int size = scaleAt (scale).size;
    Grid grid;
    grid.setLength (length);
    if (numRows <= 0)
    {
        target.copyFrom (grid);
        return;
    }

    auto degreeOf = [size] (int row) { return row % size; };
    auto randomRow = [&] { return firstRow + rng.below (numRows); };

    // the lead row: a root in view when there is one
    std::vector<int> roots;
    for (int row = firstRow; row < firstRow + numRows; ++row)
        if (degreeOf (row) == 0)
            roots.push_back (row);
    std::vector<int> lanes { roots.empty() ? randomRow() : roots[(size_t) rng.below ((int) roots.size())] };

    // the others favour the chord tones (third and fifth) of seven-note scales
    const int extra = 1 + rng.below (std::min (4, numRows));
    for (int i = 0, tries = 0; i < extra && tries < 50; ++tries)
    {
        const int row = randomRow();
        const int degree = degreeOf (row);
        const bool chordTone = degree == 0 || (size == 7 && (degree == 2 || degree == 4));
        if (std::find (lanes.begin(), lanes.end(), row) != lanes.end() || (! chordTone && rng.chance (0.5f)))
            continue;
        lanes.push_back (row);
        ++i;
    }

    // Each row plays its own Euclidean rhythm, rotated against the others, so the hits roll
    // round each other. The lead row is busier and usually starts on the first step.
    std::vector<bool> taken ((size_t) length, false);
    int placed = 0;
    for (size_t lane = 0; lane < lanes.size(); ++lane)
    {
        const bool lead = lane == 0;
        const int pulses = std::max (1, (int) std::lround ((float) length * (lead ? rng.between (0.2f, 0.45f) : rng.between (0.1f, 0.35f))));
        const int rotation = lead && rng.chance (0.7f) ? 0 : rng.below (length);
        const EuclidPattern rhythm (length, pulses, rotation);

        for (int step = 0; step < length; ++step)
        {
            // where another row already plays, mostly leave the step to it
            if (! rhythm.isOnset (step) || (taken[(size_t) step] && rng.chance (0.7f)))
                continue;

            const bool downbeat = step % 4 == 0;
            Cell c;
            c.on = true;
            const float certain = lead ? 0.8f : downbeat ? 0.6f : 0.45f;
            c.probability = rng.chance (certain) ? 1.0f : std::round (rng.between (0.25f, 0.9f) * 20.0f) / 20.0f;
            c.velocity = downbeat ? rng.between (0.75f, 1.0f) : rng.between (0.45f, 0.9f);
            constexpr float ratchets[] = { 83, 10, 4, 3 };
            c.ratchet = 1 + rng.pick (ratchets);
            grid.set (rowSlot (lanes[lane], scale), step, c);
            taken[(size_t) step] = true;
            ++placed;
        }
    }

    if (placed == 0) // never an empty grid: the lead row on the first step
    {
        Cell c;
        c.on = true;
        c.velocity = 0.9f;
        grid.set (rowSlot (lanes.front(), scale), 0, c);
    }
    target.copyFrom (grid);
}
} // namespace bleepforge

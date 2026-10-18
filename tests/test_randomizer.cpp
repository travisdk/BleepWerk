// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

// The dice. Random sounds: every value names a sound parameter of the right engine and sits
// in its range, and each sound, at its matched gain, plays, stays below clipping and falls
// silent by itself, like a factory preset; the output gain is the matched one. Random patterns: only rows in view, the length
// kept, never empty, and the same seed gives the same pattern.
#include "Randomizer.h"
#include "TestUtils.h"

#include <cstdio>
#include <cstring>

using namespace bleepforge;

static Patch patchFor (const std::vector<PresetValue>& values, Engine engine)
{
    return readPatch ([&] (const char* id)
                      {
                          if (std::strcmp (id, "engine") == 0)
                              return (float) engine;
                          float value = soundDefault (id);
                          for (const auto& v : values)
                              if (std::strcmp (v.id, id) == 0)
                                  value = v.value;
                          return value;
                      });
}

static void testSounds()
{
    // each engine's parameters start with its name in lower case
    constexpr const char* prefixes[] = { "bleep", "foley", "machine" };
    static_assert (std::size (prefixes) == kNumEngines);
    for (int e = 0; e < kNumEngines; ++e)
        for (uint64_t seed = 1; seed <= 60; ++seed)
        {
            const auto engine = (Engine) e;
            DiceRng rng (seed);
            const auto values = randomMatchedSound (engine, rng, soundDefault);
            for (const auto& v : values)
            {
                const auto* param = findSoundParam (v.id);
                CHECK (param != nullptr);
                CHECK (param == nullptr || (v.value >= param->min && v.value <= param->max));
                CHECK (std::strcmp (v.id, "engine") != 0);
                for (int other = 0; other < kNumEngines; ++other) // the other engines are left alone
                    CHECK (other == e || std::strncmp (v.id, prefixes[other], std::strlen (prefixes[other])) != 0);
            }

            const auto patch = patchFor (values, engine);
            const float gain = std::pow (10.0f, patch.outputDb / 20.0f);
            const auto level = measurePatch (patch);
            const auto [left, right] = renderPatch (patch, 48000.0, 8.0);
            float tail = 0.0f;
            bool finite = true;
            for (size_t i = 0; i < left.size(); ++i)
            {
                finite = finite && std::isfinite (left[i]) && std::isfinite (right[i]);
                if (i >= left.size() - 24000)
                    tail = std::max ({ tail, std::abs (left[i]), std::abs (right[i]) });
            }
            tail *= gain;

            const bool loudEnough = level.loudest * gain > 0.1f || level.peak * gain > 0.6f;
            const bool ok = finite && loudEnough && level.peak * gain <= kMaxPeak + 1.0e-3f && tail < 1.0e-3f;
            if (! ok)
            {
                std::printf ("FAIL %s seed %d: loudness %5.3f  peak %5.3f  tail %.5f\n", kEngineNames[e], (int) seed,
                             level.loudest * gain, level.peak * gain, tail);
                ++testFailures();
            }
        }
}

static void testPatterns()
{
    const int scale = kDefaultScale, firstRow = 35, inView = 15; // C4 to C6 in minor
    for (uint64_t seed = 1; seed <= 500; ++seed)
    {
        Grid grid;
        grid.setLength (12);
        DiceRng rng (seed);
        randomPattern (grid, scale, firstRow, inView, rng);
        CHECK (grid.getLength() == 12);

        int hits = 0;
        for (int row = 0; row < numRows (scale, 0); ++row)
            for (int step = 0; step < kMaxSteps; ++step)
                if (grid.get (rowSlot (row, scale), step).on)
                {
                    ++hits;
                    CHECK (row >= firstRow && row < firstRow + inView);
                    CHECK (step < 12);
                }
        CHECK (hits > 0);

        Grid again;
        again.setLength (12);
        DiceRng sameSeed (seed);
        randomPattern (again, scale, firstRow, inView, sameSeed);
        CHECK (again.toString() == grid.toString());
    }
}

int main()
{
    testSounds();
    testPatterns();
    return finish ("randomizer");
}

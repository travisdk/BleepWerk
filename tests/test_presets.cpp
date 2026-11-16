// Copyright (C) 2026 Henrik Juhl. Part of BleepWerk, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

// The factory presets: every value names a real sound parameter and sits in its range, names
// are unique, and each preset, rendered at its matched output gain, sounds, stays below
// clipping, stays finite and falls silent by itself. Run with -v to print every preset's level.
#include "FactoryPresets.h"
#include "TestUtils.h"

#include <cstdio>
#include <cstring>
#include <set>
#include <string>

using namespace bleepwerk;

static Patch patchFor (const FactoryPreset& preset)
{
    return readPatch ([&preset] (const char* id)
                      {
                          float value = soundDefault (id);
                          for (const auto& v : preset.values)
                              if (std::strcmp (v.id, id) == 0)
                                  value = v.value;
                          return value;
                      });
}

int main (int argc, char** argv)
{
    const bool verbose = argc > 1 && std::strcmp (argv[1], "-v") == 0;
    std::set<std::string> names;

    for (const auto& preset : factoryPresets())
    {
        CHECK (names.insert (preset.name).second);
        for (const auto& v : preset.values)
        {
            const auto* param = findSoundParam (v.id);
            CHECK (param != nullptr);
            if (param != nullptr && (v.value < param->min || v.value > param->max))
            {
                std::printf ("FAIL %s: %s = %g is outside %g..%g\n", preset.name, v.id, v.value, param->min, param->max);
                ++testFailures();
            }
            CHECK (std::strcmp (v.id, "outputGain") != 0); // matched on loading
        }

        auto patch = patchFor (preset);
        const float outputDb = matchedOutputDb (patch);
        const float gain = std::pow (10.0f, outputDb / 20.0f);
        const auto level = measurePatch (patch);

        // the tail: 8 s covers the longest ring and a delay at high feedback
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

        // loud enough: near the target loudness, or a short tick held back by the peak limit
        const bool loudEnough = level.loudest * gain > 0.1f || level.peak * gain > 0.6f;
        const bool ok = finite && loudEnough && level.peak * gain <= kMaxPeak + 1.0e-3f && tail < 1.0e-3f;
        if (verbose || ! ok)
            std::printf ("%s %-10s %-18s gain %+6.1f dB  loudness %5.3f  peak %5.3f  tail %.5f\n", ok ? "    " : "FAIL",
                         preset.category, preset.name, outputDb, level.loudest * gain, level.peak * gain, tail);
        if (! ok)
            ++testFailures();
    }
    std::printf ("%d factory presets\n", (int) factoryPresets().size());
    return finish ("presets");
}

// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "Sequencer.h"

#include <sstream>

// Factory sequencer patterns: original lines for melodic percussion in hypnotic and minimal
// techno (not transcriptions of existing records).
namespace bleepforge
{

struct FactoryPattern
{
    const char* name;
    const char* steps; // compact notation, see parseCompactPattern
    int length;
    float swing;
};

// Sixteen space-separated tokens: "-" is a rest, otherwise semitones from the root followed by
// an optional "a" (accent) or "p" (soft), e.g. "0a 12p -". Steps past the length are still read.
inline bool parseCompactPattern (const char* text, Pattern& pattern)
{
    std::istringstream in (text);
    Step steps[kMaxSteps];
    for (auto& step : steps)
    {
        std::string token;
        if (! (in >> token))
            return false;

        step = {};
        if (token == "-")
        {
            step.gate = false;
            continue;
        }
        size_t used = 0;
        try
        {
            step.note = std::clamp (std::stoi (token, &used), kMinStepNote, kMaxStepNote);
        }
        catch (const std::exception&) // pasted text that isn't a pattern
        {
            return false;
        }
        for (auto c : token.substr (used))
        {
            if (c == 'a')
                step.level = kNumStepLevels - 1;
            else if (c == 'p')
                step.level = 0;
        }
    }
    for (int i = 0; i < kMaxSteps; ++i)
        pattern.set (i, steps[i]);
    return true;
}

inline std::string toCompactPattern (const Pattern& pattern)
{
    std::string text;
    for (int i = 0; i < kMaxSteps; ++i)
    {
        const auto s = pattern.get (i);
        text += i > 0 ? " " : "";
        text += s.gate ? std::to_string (s.note) + (s.level == kNumStepLevels - 1 ? "a" : s.level == 0 ? "p" : "") : "-";
    }
    return text;
}

inline const FactoryPattern kFactoryPatterns[] = {
    // Minimal: few notes, a lot of space
    { "Lonely Pulse",     "0a - - - - - - - 0p - - - - - - -",         16, 0.5f },
    { "Pulse and Echo",   "0a - - 0p - - 0p - - - - - - - - -",        16, 0.5f },
    { "Off-beat Tick",    "- - 0 - - - 0p - - - 0 - - - 0p -",         16, 0.5f },
    { "Slow Drip",        "0a - - - 7p - - - - - 3 - - - - -",         16, 0.52f },
    { "Dust",             "0a - 0p - - 0p - - 0 - - 0p - - 7p -",      16, 0.5f },

    // Hypnotic: short phrases that repeat and shift
    { "Hypnotic Loop",    "0a - 0p 12p 0 - 0p 3p 0a - 0p 12p 0 - 7p 5p", 16, 0.5f },
    { "Spiral",           "0 3p 7p 12a 0 3p 7p 12p 0 3p 7p 15a 0 3p 7p 10p", 16, 0.5f },
    { "Tidal",            "0a - 0p - 7 - 0p - 0a - 0p - 10 - 7p -",    16, 0.55f },
    { "Circle Walk",      "0a 0p - 7p - 0p 3 - 0a 0p - 10p - 0p 5 -",  16, 0.5f },
    { "Fold",             "0a 7p 0p 12 0a 7p 0p 15 0a 7p 0p 12 0a 10p 0p 8", 16, 0.5f },

    // Groove: swung and syncopated
    { "Late Night",       "0a - - 0p - 0 - 7p - - 0a - - 0p - 3",       16, 0.58f },
    { "Shuffle Blip",     "0a - 0p - 0a - 12p - 0a - 0p - 0a 12p 10p -", 16, 0.62f },
    { "Rolling",          "0a 0p 0p 0 0p 0p 0 0p 0a 0p 0p 0 0p 0p 12 7p", 16, 0.5f },
    { "Push and Pull",    "0a - - 0p - - 0 - 0a - - - 0p - 7 -",       16, 0.56f },
    { "Offbeat Stab",     "- - 0a - - - 0a - - - 0a - - - 7a 5p",      16, 0.5f },

    // Melodic: a line to build on
    { "Minor Walk",       "0a - 3p - 5 - 7p - 10a - 7p - 5 - 3p -",    16, 0.5f },
    { "Pentatonic Run",   "0 3p 5p 7 10p 12 10p 7p 5 3p 0a - 0p 3p 5 7p", 16, 0.5f },
    { "Octave Bounce",    "0a 12p 0p 12p 0a 12p 0p 12p 0a 12p 0p 12p 0a 12p 0p 12p", 16, 0.5f },
    { "Phrygian Shadow",  "0a 1p - 0p - 1p 0 - 0a 1p - 0p - 5p 3 1p",  16, 0.5f },
    { "Call and Answer",  "0a 0p 7p 0p - - - - 3a 3p 10p 3p - - 5p 7p", 16, 0.5f },

    // Odd lengths: loops that drift against the 4/4 grid
    { "Twelve",           "0a - - 0p - - 0a - - 0p 12p - - - - -",     12, 0.5f },
    { "Seven",            "0a 12p 0p 3 5p 0p 7p - - - - - - - - -",     7, 0.5f },
    { "Five Wheel",       "0a - 0p 7p - - - - - - - - - - - -",         5, 0.5f },
    { "Nine Drift",       "0a - 0p - 7p 0p - 3p 10p - - - - - - -",     9, 0.5f },
};

inline constexpr int kNumFactoryPatterns = (int) (sizeof (kFactoryPatterns) / sizeof (kFactoryPatterns[0]));

} // namespace bleepforge

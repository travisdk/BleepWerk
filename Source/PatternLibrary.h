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
// an optional "a" (accent), e.g. "0a 12 -". Steps past the length are still read.
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
            if (c == 'a')
                step.level = kAccentLevel; // "p" (soft) is read as a normal step

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
        text += s.gate ? std::to_string (s.note) + (s.level == kAccentLevel ? "a" : "") : "-";
    }
    return text;
}

inline const FactoryPattern kFactoryPatterns[] = {
    // Minimal: few notes, a lot of space
    { "Lonely Pulse",     "0a - - - - - - - 0 - - - - - - -",         16, 0.5f },
    { "Pulse and Echo",   "0a - - 0 - - 0 - - - - - - - - -",        16, 0.5f },
    { "Off-beat Tick",    "- - 0 - - - 0 - - - 0 - - - 0 -",         16, 0.5f },
    { "Slow Drip",        "0a - - - 7 - - - - - 3 - - - - -",         16, 0.52f },
    { "Dust",             "0a - 0 - - 0 - - 0 - - 0 - - 7 -",      16, 0.5f },

    // Hypnotic: short phrases that repeat and shift
    { "Hypnotic Loop",    "0a - 0 12 0 - 0 3 0a - 0 12 0 - 7 5", 16, 0.5f },
    { "Spiral",           "0 3 7 12a 0 3 7 12 0 3 7 15a 0 3 7 10", 16, 0.5f },
    { "Tidal",            "0a - 0 - 7 - 0 - 0a - 0 - 10 - 7 -",    16, 0.55f },
    { "Circle Walk",      "0a 0 - 7 - 0 3 - 0a 0 - 10 - 0 5 -",  16, 0.5f },
    { "Fold",             "0a 7 0 12 0a 7 0 15 0a 7 0 12 0a 10 0 8", 16, 0.5f },

    // Groove: swung and syncopated
    { "Late Night",       "0a - - 0 - 0 - 7 - - 0a - - 0 - 3",       16, 0.58f },
    { "Shuffle Blip",     "0a - 0 - 0a - 12 - 0a - 0 - 0a 12 10 -", 16, 0.62f },
    { "Rolling",          "0a 0 0 0 0 0 0 0 0a 0 0 0 0 0 12 7", 16, 0.5f },
    { "Push and Pull",    "0a - - 0 - - 0 - 0a - - - 0 - 7 -",       16, 0.56f },
    { "Offbeat Stab",     "- - 0a - - - 0a - - - 0a - - - 7a 5",      16, 0.5f },

    // Melodic: a line to build on
    { "Minor Walk",       "0a - 3 - 5 - 7 - 10a - 7 - 5 - 3 -",    16, 0.5f },
    { "Pentatonic Run",   "0 3 5 7 10 12 10 7 5 3 0a - 0 3 5 7", 16, 0.5f },
    { "Octave Bounce",    "0a 12 0 12 0a 12 0 12 0a 12 0 12 0a 12 0 12", 16, 0.5f },
    { "Phrygian Shadow",  "0a 1 - 0 - 1 0 - 0a 1 - 0 - 5 3 1",  16, 0.5f },
    { "Call and Answer",  "0a 0 7 0 - - - - 3a 3 10 3 - - 5 7", 16, 0.5f },

    // Odd lengths: loops that drift against the 4/4 grid
    { "Twelve",           "0a - - 0 - - 0a - - 0 12 - - - - -",     12, 0.5f },
    { "Seven",            "0a 12 0 3 5 0 7 - - - - - - - - -",     7, 0.5f },
    { "Five Wheel",       "0a - 0 7 - - - - - - - - - - - -",         5, 0.5f },
    { "Nine Drift",       "0a - 0 - 7 0 - 3 10 - - - - - - -",     9, 0.5f },
};

inline constexpr int kNumFactoryPatterns = (int) (sizeof (kFactoryPatterns) / sizeof (kFactoryPatterns[0]));

} // namespace bleepforge

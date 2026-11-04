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
    int rate = 1;      // index into kSeqRateNames, 1/16
    float gate = 0.5f;
    int drift = 0;     // steps the line moves on per pass
    int vary = 0;      // steps a pass may be shorter
    int shape = 0;     // index into kLengthShapeNames
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

    // Polymeter in one layer: an odd length drifts against the host's bar, Drift slides the
    // line along, and Vary shortens some passes, so the accent keeps landing somewhere new.
    // Sparse on purpose: two to four notes, mostly root, fifth and octave, with room around them.
    { "Seven Arp",       "0a - 7 - 3 - - - - - - - - - - -", 7, 0.5f, 0, 0.8f, 0, 0, 0 },
    { "Nine Arp",        "0a - - 7 - - 3 - - - - - - - - -", 9, 0.5f, 0, 0.8f, 0, 0, 0 },
    { "Eleven Arp",      "0a - 7 - - 10 - - 5 - - - - - - -", 11, 0.5f, 0, 0.8f, 0, 0, 0 },
    { "Thirteen Arp",    "0a - - 7 - - - 12 - - 10 - - - - -", 13, 0.5f, 0, 0.8f, 0, 0, 0 },
    { "Fifteen Arp",     "0a - - - 7 - - - 3 - - - 10 - - -", 15, 0.5f, 1, 0.8f, 0, 0, 0 },
    { "Five Bleeps",     "0a - - 7 - - - - - - - - - - - -", 5, 0.5f, 0, 0.5f, 0, 0, 0 },
    { "Seven Pulse",     "0a - - 0 - 12 - - - - - - - - - -", 7, 0.5f, 0, 0.5f, 0, 0, 0 },
    { "Eleven Pulse",    "0a - - - 0 - - 7 - - - - - - - -", 11, 0.5f, 0, 0.5f, 0, 0, 0 },
    { "Thirteen Walk",   "0a - - 1 - - - 0 - - 7 - - - - -", 13, 0.5f, 0, 0.7f, 0, 0, 0 },
    { "Fifteen Pulse",   "0a - - - - - 0 - - - 12 - - - - -", 15, 0.5f, 1, 0.5f, 0, 0, 0 },
    { "Triplet Seven",   "0a - - 7 - - - - - - - - - - - -", 7, 0.5f, 3, 0.6f, 0, 0, 0 },
    { "Slow Seven",      "0a - - - 7 - - - - - - - - - - -", 7, 0.5f, 0, 0.95f, 0, 0, 0 },
    { "Quick Five",      "0a - - 12 - - - - - - - - - - - -", 5, 0.5f, 1, 0.4f, 0, 0, 0 },
    { "Creeping Arp",    "0a - - - 7 - - - 3 - - - 10 - - -", 16, 0.5f, 1, 0.8f, 1, 0, 0 },
    { "Falling Behind",  "0a - - 0 - - 7 - - 0 - - - 5 - -", 16, 0.5f, 1, 0.8f, -1, 0, 0 },
    { "Leap Two",        "0a - - 7 - - 0 - - 3 - - - - - -", 12, 0.5f, 1, 0.7f, 2, 0, 0 },
    { "Quick Slide",     "0a - - 7 - 3 - - - - - - - - - -", 8, 0.5f, 0, 0.7f, 3, 0, 0 },
    { "Breathing Arp",   "0a - - - 7 - - - 0 - - - 3 - - -", 16, 0.5f, 1, 0.8f, 0, 2, 0 },
    { "Shrinking Loop",  "0a - - 7 - - - - 0 - - 3 - - - -", 16, 0.5f, 1, 0.7f, 0, 4, 1 },
    { "Skipped Beat",    "0a - - - - - 7 - - - 0 - - - 5 -", 16, 0.5f, 1, 0.7f, 0, 1, 2 },
    { "Limping Pulse",   "0a - - - 0 - - - 12 - - - - - - -", 12, 0.5f, 1, 0.5f, 0, 3, 0 },
    { "Slow Wander",     "0a - - 7 - - - 3 - - - - - - - -", 13, 0.5f, 0, 0.9f, 1, 2, 0 },
};

inline constexpr int kNumFactoryPatterns = (int) (sizeof (kFactoryPatterns) / sizeof (kFactoryPatterns[0]));
// the single-layer polymeter patterns are the last ones; the browser puts them in a submenu
inline constexpr int kFirstPolymeterPattern = kNumFactoryPatterns - 22;

// Polymeter sets: several layers that load into slots A, B, C... at once and play together.
// The layers have different lengths, so they drift apart and meet again only after the least
// common multiple of the lengths (16 against 12 after 48 steps). The steps are semitones from
// the one shared root, so a bass layer sits low (-12 and up) and the others above it. Sparse on
// purpose: two to four notes a layer, with room around them.
struct FactoryLayer
{
    const char* steps; // as FactoryPattern
    int length;
    int rate;          // index into kSeqRateNames
    float gate;
};

constexpr int kMaxSetLayers = 4;

struct FactorySet
{
    const char* name;
    int numLayers;
    FactoryLayer layers[kMaxSetLayers];
};

inline const FactorySet kFactorySets[] = {
    { "Slow Orbit 16:12", 2, {
        { "0a - - - 7 - - - 3 - - - 10 - - -", 16, 0, 0.8f },
        { "12a - - - - 7 - - - 10 - - - - - -", 12, 0, 0.8f } } },
    { "Three Clocks 16:12:9", 3, {
        { "0a - - - 0 - - - 7 - - - 0 - - -", 16, 0, 0.8f },
        { "12 - - 15 - - - 10 - - - - - - - -", 12, 0, 0.8f },
        { "24a - - - - 19 - - - - - - - - - -", 9, 1, 0.3f } } },
    { "Seven Against Four", 2, {
        { "0a - - - 0 - - - 0 - - - 7 - - -", 16, 0, 0.7f },
        { "12a - 15 - - 10 - - - - - - - - - -", 7, 0, 0.7f } } },
    { "Phase Drift 16:15", 2, {
        { "0a - - - 7 - - - 0 - - - 5 - - -", 16, 1, 0.9f },
        { "12a - - - 19 - - - 12 - - - 17 - - -", 15, 1, 0.9f } } },
    { "Quintuple Clock", 3, {
        { "0a - - - - - - - -3 - - - - - - -", 16, 0, 0.95f },
        { "12a - - 15 - - - - - - - - - - - -", 5, 0, 0.6f },
        { "24a - - - - - - - - - - - - - - -", 3, 0, 0.3f } } },
    { "Cold Machine 16:12:8", 3, {
        { "0a - - 7 - 0 - - - - - - - - - -", 8, 0, 0.7f },
        { "12a - - - 15 - - - 10 - - - - - - -", 12, 0, 0.7f },
        { "0a - - - - - - - 5 - - - - - - -", 16, 0, 0.95f } } },
    { "Three Over Four", 2, {
        { "0a - - - 0 - - - 7a - - - 5 - - -", 16, 1, 0.7f },
        { "12a - - 7 - - - - - - - - - - - -", 6, 3, 0.6f } } },
    { "Cathedral Seq", 2, {
        { "0a - - - - - - - 3 - - - - - - -", 16, 0, 0.95f },
        { "12a - - 15 - - 19 - - - - - - - - -", 12, 0, 0.8f } } },
    { "Nine and Eleven", 2, {
        { "0a - - 7 - - 3 - - - - - - - - -", 9, 0, 0.7f },
        { "12a - - 15 - - - 10 - - - - - - - -", 11, 0, 0.7f } } },
    { "Phrygian Wheels 16:14", 2, {
        { "0a - - 1 - - - 0 - - 8 - - - - -", 16, 0, 0.7f },
        { "12a - - - 13 - - - 8 - - - - - - -", 14, 0, 0.7f } } },
    { "Bass Under Twelve", 2, {
        { "-12a - - - - - -9 - - - - - - - - -", 12, 0, 0.95f },
        { "0a - - 7 - - - 12 - - 7 - - - 3 -", 16, 0, 0.7f } } },
    { "Glass Canon 16:13", 2, {
        { "0a - - - 7 - - - 3a - - - 10 - - -", 16, 0, 0.7f },
        { "12a - - 19 - - - 15 - - 22 - - - - -", 13, 0, 0.7f } } },
    { "Gears 6:4:5", 3, {
        { "0a - - 7 - - - - - - - - - - - -", 6, 0, 0.6f },
        { "24a - - - - - - - - - - - - - - -", 4, 0, 0.4f },
        { "12a - - 15 - - - - - - - - - - - -", 5, 0, 0.6f } } },
    { "Octave in Seven", 2, {
        { "0a - - 7 - - 3 - - - - - - - - -", 8, 0, 0.7f },
        { "12a - - 10 - - - - - - - - - - - -", 7, 0, 0.7f } } },
    { "Deep Time", 3, {
        { "-12a - - - - - - - -9 - - - - - - -", 16, 0, 0.95f },
        { "0a - - 7 - - - 3 - - - - - - - -", 12, 0, 0.6f },
        { "24a - - 19 - - - - - - - - - - - -", 7, 1, 0.3f } } },
    { "Staccato Rain 11:13:16", 3, {
        { "12a - - 15 - - - 19 - - - - - - - -", 11, 0, 0.25f },
        { "0a - - - 0 - - - - 7 - - - - - -", 13, 0, 0.25f },
        { "24a - - - - - - - 19a - - - - - - -", 16, 0, 0.25f } } },
    { "Seven Eight Nine", 3, {
        { "0a - 7 - - 3 - - - - - - - - - -", 7, 0, 0.7f },
        { "12a - - 15 - - 10 - - - - - - - - -", 8, 0, 0.7f },
        { "0a - - - 5 - - - - - - - - - - -", 9, 0, 0.8f } } },
    { "Five Over Seven", 2, {
        { "0a - - 3 - - - - - - - - - - - -", 5, 0, 0.7f },
        { "12a - - 15 - 10 - - - - - - - - - -", 7, 0, 0.7f } } },
};

inline constexpr int kNumFactorySets = (int) (sizeof (kFactorySets) / sizeof (kFactorySets[0]));
// the browser lists the single patterns first, then the sets, then the user's files
inline constexpr int kNumFactoryEntries = kNumFactoryPatterns + kNumFactorySets;

} // namespace bleepforge

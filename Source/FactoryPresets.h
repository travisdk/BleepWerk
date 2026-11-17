// Copyright (C) 2026 Henrik Juhl. Part of BleepWerk, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "Patch.h"

#include <vector>

// The factory sound presets, in the order the browser shows them. Each lists only what it
// changes from the defaults in kSoundParams; everything else, the output gain included, is
// reset on loading. The output gain is set by matchedOutputDb() so they all play about
// equally loud. A preset never touches the pitch bend range, a setting of the player. JUCE-free.
namespace bleepwerk
{
struct PresetValue
{
    const char* id;
    float value;
};

struct FactoryPreset
{
    const char* name;
    const char* category;
    std::vector<PresetValue> values;
};

namespace preset
{
// readable names for the choice values
inline constexpr float Sine = 0, Saw = 1, Pulse = 2, FM = 3;
inline constexpr float Knock = 0, Brush = 1, Blow = 2, Squeal = 3; // Play
inline constexpr float Bar = 0, Skin = 1;                           // On
inline constexpr float LP = 1, BP = 2, HP = 3;
inline constexpr float N32 = 0, N16 = 1, N8 = 2, N4 = 3, N2 = 4;
inline constexpr float Straight = 0, Triplet = 1, Dotted = 2;
inline constexpr float Mono = 0;
inline constexpr float Warm = 0, Hard = 1, Fold = 2;
inline constexpr float Pre = 0, Post = 1;

inline constexpr PresetValue Strike { "engine", 1 };
} // namespace preset

inline const std::vector<FactoryPreset>& factoryPresets()
{
    using namespace preset;
    static const std::vector<FactoryPreset> presets {
        // ---- Bleeps: sine and saw
        { "Init", "Bleeps", {} },
        { "Sine Tom", "Bleeps", { { "bleepPitchAmount", 12 }, { "bleepPitchDecay", 60 }, { "bleepAttack", 1 }, { "bleepDecay", 320 } } },
        { "Dot Drop", "Bleeps", { { "bleepPitchAmount", -7 }, { "bleepPitchDecay", 18 }, { "bleepAttack", 2 }, { "bleepDecay", 110 }, { "filterType", LP }, { "filterCutoff", 2500 } } },
        { "Laser Drop", "Bleeps", { { "bleepPitchAmount", 36 }, { "bleepPitchDecay", 60 }, { "bleepDecay", 300 } } },
        { "Long Sine", "Bleeps", { { "bleepPitchAmount", 2 }, { "bleepDecay", 900 }, { "bleepRelease", 200 },
                                   { "roomSize", 0.4f }, { "roomDecay", 0.7f }, { "roomMix", 0.25f } } },
        { "Saw Blip", "Bleeps", { { "bleepWave", Saw }, { "bleepPitchAmount", 7 }, { "bleepDecay", 140 },
                                  { "filterType", LP }, { "filterCutoff", 3000 } } },
        { "Bounce Blip", "Bleeps", { { "bleepPitchAmount", 24 }, { "bleepPitchDecay", 45 },
                                     { "bleepDecay", 220 }, { "chaosPitch", 0.3f } } },
        { "Hollow Sine", "Bleeps", { { "bleepPitchAmount", 0 }, { "bleepDecay", 400 },
                                     { "filterType", BP }, { "filterCutoff", 1200 }, { "filterResonance", 0.5f } } },
        { "Glide Sine", "Bleeps", { { "voiceMode", Mono }, { "glide", 80 }, { "bleepPitchAmount", 0 }, { "bleepDecay", 400 },
                                    { "bleepRelease", 120 } } },
        // Played high (around C6) so the band-pass sits near the note
        { "Sonar Ping", "Bleeps", { { "bleepPitchAmount", 1 }, { "bleepPitchDecay", 300 }, { "bleepAttack", 2 }, { "bleepDecay", 1500 },
                                    { "bleepRelease", 2000 }, { "filterType", BP }, { "filterCutoff", 1200 }, { "filterResonance", 0.15f },
                                    { "delayNote", N4 }, { "delayFeel", Dotted }, { "delayFeedback", 0.4f }, { "delayMix", 0.25f },
                                    { "roomSize", 0.85f }, { "roomDecay", 4 }, { "roomTone", 0.3f }, { "roomMix", 0.35f } } },
        { "Radar Blip", "Bleeps", { { "bleepPitchAmount", 2 }, { "bleepPitchDecay", 80 }, { "bleepAttack", 1 }, { "bleepDecay", 350 },
                                    { "bleepRelease", 300 }, { "filterType", BP }, { "filterCutoff", 1800 }, { "filterResonance", 0.2f },
                                    { "delayNote", N8 }, { "delayFeel", Dotted }, { "delayFeedback", 0.45f }, { "delayMix", 0.3f },
                                    { "roomSize", 0.7f }, { "roomDecay", 2.5f }, { "roomTone", 0.4f }, { "roomMix", 0.2f } } },

        // ---- Pulses
        { "Square Bleep", "Pulses", { { "bleepWave", Pulse }, { "bleepPitchAmount", 0 }, { "bleepDecay", 150 },
                                      { "filterType", LP }, { "filterCutoff", 4000 } } },
        { "Retro Beep", "Pulses", { { "bleepWave", Pulse }, { "bleepPitchAmount", 0 }, { "bleepDecay", 300 }, { "bleepRelease", 40 } } },
        { "Square Pluck", "Pulses", { { "bleepWave", Pulse }, { "bleepPulseWidth", 0.7f }, { "bleepPitchAmount", 0 }, { "bleepDecay", 350 },
                                      { "filterType", LP }, { "filterCutoff", 2500 }, { "filterResonance", 0.6f } } },
        { "Resonant Square", "Pulses", { { "bleepWave", Pulse }, { "bleepPitchAmount", 0 }, { "bleepDecay", 200 },
                                         { "filterType", LP }, { "filterCutoff", 800 }, { "filterResonance", 0.8f } } },
        { "Glide Square", "Pulses", { { "bleepWave", Pulse }, { "voiceMode", Mono }, { "glide", 60 }, { "bleepPitchAmount", 0 },
                                      { "bleepDecay", 300 }, { "bleepRelease", 80 }, { "filterType", LP }, { "filterCutoff", 3000 } } },
        { "Pulse Tom", "Pulses", { { "bleepWave", Pulse }, { "bleepPulseWidth", 0.35f }, { "bleepPitchAmount", 12 }, { "bleepPitchDecay", 50 }, { "bleepDecay", 260 },
                                   { "filterType", LP }, { "filterCutoff", 1200 }, { "filterResonance", 0.3f } } },
        // ---- FM
        { "FM Bell", "FM", { { "bleepWave", FM }, { "bleepFmRatio", 3.5f }, { "bleepFmAmount", 0.45f }, { "bleepPitchAmount", 0 },
                             { "bleepDecay", 900 }, { "bleepRelease", 300 }, { "roomSize", 0.5f }, { "roomDecay", 0.9f }, { "roomMix", 0.3f } } },
        { "FM Tine", "FM", { { "bleepWave", FM }, { "bleepFmRatio", 4 }, { "bleepFmAmount", 0.3f }, { "bleepPitchAmount", 0 },
                             { "bleepDecay", 500 } } },
        { "FM Metal", "FM", { { "bleepWave", FM }, { "bleepFmRatio", 1.5f }, { "bleepFmAmount", 0.8f }, { "bleepPitchAmount", 0 },
                              { "bleepDecay", 300 } } },
        { "FM Woodblock", "FM", { { "bleepWave", FM }, { "bleepFmRatio", 3.5f }, { "bleepFmAmount", 0.2f }, { "bleepPitchAmount", 12 },
                                  { "bleepPitchDecay", 6 }, { "bleepDecay", 70 } } },
        { "FM Glass", "FM", { { "bleepWave", FM }, { "bleepFmRatio", 7 }, { "bleepFmAmount", 0.25f }, { "bleepPitchAmount", 0 },
                              { "bleepDecay", 1200 }, { "bleepRelease", 400 }, { "roomDecay", 0.7f }, { "roomMix", 0.35f } } },
        { "FM Gong Tink", "FM", { { "bleepWave", FM }, { "bleepFmRatio", 5.5f }, { "bleepFmAmount", 0.6f }, { "bleepPitchAmount", 0 },
                                  { "bleepDecay", 1500 }, { "bleepRelease", 400 }, { "roomDecay", 0.7f }, { "roomMix", 0.2f } } },
        { "FM Rise", "FM", { { "bleepWave", FM }, { "bleepFmRatio", 3 }, { "bleepFmAmount", 0.5f }, { "bleepPitchAmount", -24 },
                             { "bleepPitchDecay", 60 }, { "bleepDecay", 300 } } },
        { "FM Pluck", "FM", { { "bleepWave", FM }, { "bleepFmRatio", 1 }, { "bleepFmAmount", 0.5f }, { "bleepPitchAmount", 0 },
                              { "bleepDecay", 220 } } },
        { "FM Bright Ping", "FM", { { "bleepWave", FM }, { "bleepFmRatio", 8 }, { "bleepFmAmount", 0.35f }, { "bleepPitchAmount", 0 },
                                    { "bleepDecay", 400 } } },
        { "FM Clang", "FM", { { "bleepWave", FM }, { "bleepFmRatio", 7.5f }, { "bleepFmAmount", 1 }, { "bleepPitchAmount", 0 },
                              { "bleepDecay", 600 }, { "filterType", HP }, { "filterCutoff", 400 } } },
        // ---- Lo-fi
        { "Lo-fi Bleep", "Lo-fi", { { "bleepWave", Pulse }, { "bleepPitchAmount", 0 }, { "bleepDecay", 180 }, { "crushBits", 6 },
                                    { "crushRate", 8000 } } },
        { "8-bit Blip", "Lo-fi", { { "bleepWave", Pulse }, { "bleepPulseWidth", 0.25f }, { "bleepPitchAmount", 12 }, { "bleepPitchDecay", 8 },
                                   { "bleepDecay", 120 }, { "crushBits", 4 } } },
        { "Aliased Zap", "Lo-fi", { { "bleepPitchAmount", 48 }, { "bleepPitchDecay", 30 }, { "bleepDecay", 200 }, { "crushRate", 3000 } } },
        { "Broken Toy", "Lo-fi", { { "bleepPitchAmount", 0 }, { "bleepDecay", 300 }, { "crushBits", 7 },
                                   { "crushRate", 2500 }, { "chaosAmount", 0.6f }, { "chaosPitch", 0.5f } } },
        { "Lo-fi FM", "Lo-fi", { { "bleepWave", FM }, { "bleepFmRatio", 3.5f }, { "bleepFmAmount", 0.5f }, { "bleepPitchAmount", 0 },
                                 { "bleepDecay", 400 }, { "crushBits", 6 }, { "crushRate", 6000 } } },
        { "Grit Pulse", "Lo-fi", { { "bleepWave", Pulse }, { "bleepPulseWidth", 0.15f }, { "bleepPitchAmount", 0 }, { "bleepDecay", 160 },
                                   { "crushRate", 1500 }, { "filterType", LP }, { "filterCutoff", 6000 } } },

        // ---- Dub and delay
        { "Space Stab", "Dub", { { "bleepWave", Pulse }, { "bleepPitchAmount", 0 }, { "bleepDecay", 250 }, { "filterType", LP },
                                 { "filterCutoff", 1500 }, { "filterResonance", 0.4f }, { "delayNote", N4 }, { "delayFeel", Straight }, { "delayFeedback", 0.62f },
                                 { "delayMix", 0.4f }, { "roomSize", 0.7f }, { "roomDecay", 1.1f }, { "roomMix", 0.3f } } },
        { "Triplet Echo", "Dub", { { "bleepDecay", 150 }, { "delayNote", N8 }, { "delayFeel", Triplet }, { "delayFeedback", 0.55f },
                                   { "delayMix", 0.4f } } },
        { "Dub Bell", "Dub", { { "bleepWave", FM }, { "bleepFmRatio", 3.5f }, { "bleepFmAmount", 0.4f }, { "bleepPitchAmount", 0 },
                               { "bleepDecay", 700 }, { "delayNote", N4 }, { "delayFeel", Triplet }, { "delayFeedback", 0.55f }, { "delayMix", 0.35f },
                               { "roomDecay", 0.7f }, { "roomMix", 0.3f } } },
        { "Echo Plate", "Dub", { Strike, { "strikeBody", Skin }, { "strikeRing", 0.57f }, { "strikeTone", 0.6f }, { "delayNote", 2 }, { "delayFeel", 0 }, { "delayFeedback", 0.5f }, { "delayMix", 0.35f } } },
        { "Long Echo", "Dub", { { "bleepPitchAmount", 0 }, { "bleepDecay", 150 }, { "filterType", LP }, { "filterCutoff", 1800 },
                                { "delayNote", N8 }, { "delayFeel", Dotted }, { "delayFeedback", 0.65f }, { "delayMix", 0.45f } } },
        { "Slapback Blip", "Dub", { { "bleepDecay", 100 }, { "delayNote", N32 }, { "delayFeel", Straight }, { "delayFeedback", 0.2f }, { "delayMix", 0.35f } } },
        // ---- Bar: struck bars of wood and metal (a bar body)
        { "Log Drum", "Bar", { Strike, { "strikeRing", 0.78f }, { "strikeTone", 0.35f } } },
        { "Hollow Wood", "Bar", { Strike, { "strikeRing", 0.84f }, { "strikeTone", 0.4f }, { "roomDecay", 0.7f }, { "roomMix", 0.25f } } },
        { "Bar Mallet", "Bar", { Strike, { "strikeRing", 0.93f }, { "strikeTone", 0.45f } } },
        { "Tok", "Bar", { Strike, { "strikeRing", 0.32f }, { "strikeTone", 0.7f } } },
        { "Pipe Bar", "Bar", { Strike, { "strikeRing", 0.85f }, { "strikeTone", 0.55f }, { "chaosAmount", 0.25f }, { "roomSize", 0.6f }, { "roomDecay", 1.0f }, { "roomMix", 0.2f } } },
        { "Bar Drop", "Bar", { Strike, { "strikeRing", 0.66f }, { "strikeTone", 0.5f }, { "strikeSweep", 12 } } },

        // ---- Skin: struck skins and plates (a skin body)
        { "Metal Tink", "Skin", { Strike, { "strikeBody", Skin }, { "strikeRing", 0.62f }, { "strikeTone", 0.8f }, { "roomDecay", 0.7f }, { "roomMix", 0.2f } } },
        { "Bell Plate", "Skin", { Strike, { "strikeBody", Skin }, { "strikeRing", 0.87f }, { "strikeTone", 0.6f }, { "roomDecay", 0.7f }, { "roomMix", 0.25f } } },
        { "Gong Tap", "Skin", { Strike, { "strikeBody", Skin }, { "strikeRing", 0.94f }, { "strikeTone", 0.35f }, { "roomSize", 0.8f }, { "roomDecay", 1.2f }, { "roomMix", 0.3f } } },
        { "Anvil", "Skin", { Strike, { "strikeBody", Skin }, { "strikeRing", 0.8f }, { "strikeTone", 1 } } },
        { "Low Tom", "Skin", { Strike, { "strikeBody", Skin }, { "strikeRing", 0.72f }, { "strikeTone", 0.3f }, { "strikeSweep", 5 } } },
        { "Conga", "Skin", { Strike, { "strikeBody", Skin }, { "strikeRing", 0.6f }, { "strikeTone", 0.6f }, { "strikeSweep", 2 } } },
        { "Bongo", "Skin", { Strike, { "strikeBody", Skin }, { "strikeRing", 0.48f }, { "strikeTone", 0.78f } } },
        { "Skin Kick", "Skin", { Strike, { "strikeBody", Skin }, { "strikeRing", 0.58f }, { "strikeTone", 0.2f }, { "strikeSweep", 14 },
                                 { "driveType", Warm }, { "drivePost", Pre }, { "driveAmount", 0.45f }, { "driveMix", 0.6f } } },
        { "Boom Tom", "Skin", { Strike, { "strikeBody", Skin }, { "strikeRing", 0.84f }, { "strikeTone", 0.15f }, { "strikeSweep", 7 },
                                { "roomSize", 0.7f }, { "roomDecay", 1.4f }, { "roomMix", 0.22f }, { "roomDuck", 0.15f } } },
        { "Tabla Dha", "Skin", { Strike, { "strikeBody", Skin }, { "strikeRing", 0.74f }, { "strikeTone", 0.5f }, { "strikeSweep", 4 }, { "chaosAmount", 0.15f } } },
        { "Rim Pop", "Skin", { Strike, { "strikeBody", Skin }, { "strikeRing", 0.26f }, { "strikeTone", 0.92f }, { "strikeSweep", -5 } } },
        { "Frame Drum", "Skin", { Strike, { "strikeBody", Skin }, { "strikeRing", 0.78f }, { "strikeTone", 0.42f }, { "chaosAmount", 0.3f }, { "chaosPitch", 0.3f },
                                  { "roomSize", 0.5f }, { "roomDecay", 0.8f }, { "roomMix", 0.18f } } },
        { "Brushed Skin", "Skin", { Strike, { "strikeExciter", Brush }, { "strikeBody", Skin }, { "strikeLength", 80 }, { "strikeRing", 0.3f }, { "strikeTone", 0.7f } } },
        { "Dub Tom", "Skin", { Strike, { "strikeBody", Skin }, { "strikeRing", 0.62f }, { "strikeTone", 0.35f }, { "strikeSweep", 5 }, { "delayNote", N8 },
                               { "delayFeel", Dotted }, { "delayFeedback", 0.55f }, { "delayMix", 0.4f }, { "roomDecay", 0.8f }, { "roomMix", 0.15f } } },
        { "Friction Drum", "Skin", { Strike, { "strikeExciter", Squeal }, { "strikeBody", Skin }, { "strikeLength", 500 }, { "strikeRing", 0.5f }, { "strikeTone", 0.4f },
                                     { "strikeMotion", 0.4f } } },
        // ---- Industrial, blow and squeal: struck metal parts, and steady sources, tuned to the note
        { "Heavy Hit", "Industrial", { Strike, { "strikeBody", Bar }, { "strikeRing", 0.74f }, { "strikeTone", 0.3f }, { "chaosAmount", 0.4f },
                                       { "driveType", Warm }, { "drivePost", Pre }, { "driveAmount", 0.6f }, { "driveMix", 0.7f },
                                       { "roomSize", 0.7f }, { "roomDecay", 1.4f }, { "roomMix", 0.25f } } },
        { "Rattling Plate", "Industrial", { Strike, { "strikeBody", Bar }, { "strikeRing", 0.9f }, { "strikeTone", 0.65f }, { "strikeMotion", 0.55f }, { "chaosAmount", 0.6f }, { "chaosPitch", 0.7f } } },
        { "Pipe Tom", "Industrial", { Strike, { "strikeBody", Bar }, { "strikeRing", 0.72f }, { "strikeTone", 0.3f }, { "strikeSweep", 12 }, { "driveType", Hard },
                                     { "drivePost", Pre }, { "driveAmount", 0.5f }, { "driveMix", 0.5f }, { "roomSize", 0.5f }, { "roomDecay", 0.9f }, { "roomMix", 0.15f } } },
        { "Donk", "Industrial", { Strike, { "strikeBody", Bar }, { "strikeRing", 0.69f }, { "roomSize", 1 }, { "roomDecay", 4 }, { "roomMix", 0.12f }, { "roomDuck", 0.12f } } },
        { "Steam Valve", "Blow & Squeal", { Strike, { "strikeExciter", Blow }, { "strikeBody", Bar }, { "strikeRing", 0.15f }, { "strikeTone", 0.75f } } },
        { "Pressure Leak", "Blow & Squeal", { Strike, { "strikeExciter", Blow }, { "strikeBody", Bar }, { "strikeLength", 1200 }, { "strikeRing", 0.25f }, { "strikeTone", 0.85f }, { "strikeMotion", 0.35f }, { "roomDecay", 0.7f }, { "roomMix", 0.2f } } },
        { "Whistle Pipe", "Blow & Squeal", { Strike, { "strikeExciter", Blow }, { "strikeBody", Bar }, { "strikeLength", 600 }, { "strikeRing", 0.4f }, { "strikeTone", 0.25f } } },
        { "Air Brake", "Blow & Squeal", { Strike, { "strikeExciter", Blow }, { "strikeBody", Bar }, { "strikeLength", 180 }, { "strikeRing", 0.1f }, { "strikeTone", 0.9f }, { "chaosAmount", 0.4f }, { "chaosTone", 0.6f } } },

        { "Rail Squeal", "Blow & Squeal", { Strike, { "strikeExciter", Squeal }, { "strikeBody", Bar }, { "strikeLength", 1500 }, { "strikeRing", 0.6f }, { "strikeTone", 0.75f }, { "strikeMotion", 0.2f }, { "roomDecay", 0.7f }, { "roomMix", 0.2f } } },
        { "Brake Squeal", "Blow & Squeal", { Strike, { "strikeExciter", Squeal }, { "strikeBody", Bar }, { "strikeLength", 1200 }, { "strikeRing", 0.4f }, { "strikeTone", 0.85f }, { "strikeMotion", 0.75f }, { "chaosAmount", 0.3f }, { "chaosPitch", 0.3f } } },
        { "Metal Scrape", "Blow & Squeal", { Strike, { "strikeExciter", Squeal }, { "strikeBody", Bar }, { "strikeLength", 600 }, { "strikeTone", 0.45f }, { "strikeMotion", 0.5f } } },

        // ---- Cosmic: every section turned up as far as it will go
        { "Event Horizon", "Cosmic", { Strike, { "strikeExciter", Blow }, { "strikeBody", Skin }, { "strikeLength", 1800 }, { "strikeRing", 0.9f },
                                       { "strikeTone", 0.2f }, { "strikeMotion", 0.15f }, { "strikeSweep", -12 }, { "chaosAmount", 0.5f },
                                       { "delayNote", N16 }, { "delayFeel", Dotted }, { "delayFeedback", 0.75f }, { "delayMix", 0.45f },
                                       { "roomSize", 1 }, { "roomDecay", 3 }, { "roomTone", 0.2f }, { "roomMix", 0.6f },
                                       { "driveType", Warm }, { "drivePost", Post }, { "driveAmount", 0.6f }, { "driveMix", 0.5f } } },
        { "Pulsar", "Cosmic", { { "bleepWave", Pulse }, { "bleepPulseWidth", 0.08f }, { "bleepPitchAmount", 48 }, { "bleepPitchDecay", 6 }, { "bleepDecay", 90 },
                                { "chaosAmount", 1 }, { "chaosPitch", 0.8f }, { "chaosPan", 1 },
                                { "delayNote", N32 }, { "delayFeel", Straight }, { "delayFeedback", 0.9f }, { "delayMix", 0.5f },
                                { "filterType", HP }, { "filterCutoff", 700 }, { "filterResonance", 0.6f },
                                { "roomSize", 0.9f }, { "roomDecay", 3 }, { "roomMix", 0.35f } } },
        { "Alien Signal", "Cosmic", { { "bleepWave", FM }, { "bleepFmRatio", 7.3f }, { "bleepFmAmount", 1 }, { "bleepPitchAmount", -24 }, { "bleepPitchDecay", 120 },
                                      { "bleepDecay", 700 }, { "bleepNoise", 0.2f }, { "crushBits", 5 }, { "crushRate", 6000 },
                                      { "chaosAmount", 0.9f }, { "chaosTone", 1 }, { "chaosDecay", 0.8f },
                                      { "delayNote", N8 }, { "delayFeel", Triplet }, { "delayFeedback", 0.75f }, { "delayMix", 0.4f },
                                      { "roomSize", 0.8f }, { "roomDecay", 2.5f }, { "roomMix", 0.3f } } },
        { "Black Hole", "Cosmic", { { "bleepPitchAmount", 48 }, { "bleepPitchDecay", 500 }, { "bleepAttack", 20 }, { "bleepDecay", 2000 }, { "bleepRelease", 1500 },
                                    { "filterType", LP }, { "filterCutoff", 900 }, { "filterResonance", 0.8f },
                                    { "driveType", Fold }, { "drivePost", Pre }, { "driveAmount", 0.8f }, { "driveMix", 0.6f },
                                    { "roomSize", 1 }, { "roomDecay", 3.5f }, { "roomTone", 0.1f }, { "roomMix", 0.7f } } },
        { "Solar Flare", "Cosmic", { { "bleepWave", Saw }, { "bleepPitchAmount", 24 }, { "bleepPitchDecay", 200 }, { "bleepDecay", 500 }, { "bleepRelease", 400 },
                                     { "driveType", Hard }, { "drivePost", Pre }, { "driveAmount", 1 }, { "driveMix", 0.8f },
                                     { "filterType", LP }, { "filterCutoff", 6000 }, { "filterResonance", 0.7f },
                                     { "chaosAmount", 0.7f }, { "chaosPitch", 0.5f },
                                     { "delayNote", N16 }, { "delayFeel", Dotted }, { "delayFeedback", 0.7f }, { "delayMix", 0.35f },
                                     { "roomSize", 0.7f }, { "roomDecay", 2 }, { "roomMix", 0.3f } } },
        { "Wormhole", "Cosmic", { Strike, { "strikeExciter", Squeal }, { "strikeBody", Bar }, { "strikeLength", 1800 }, { "strikeRing", 0.8f },
                                  { "strikeTone", 0.9f }, { "strikeMotion", 1 }, { "strikeSweep", 24 }, { "chaosAmount", 0.6f }, { "chaosPitch", 0.6f },
                                  { "delayNote", N8 }, { "delayFeel", Triplet }, { "delayFeedback", 0.7f }, { "delayMix", 0.5f },
                                  { "roomSize", 0.95f }, { "roomDecay", 2.5f }, { "roomMix", 0.5f } } },
        { "Comet Tail", "Cosmic", { Strike, { "strikeExciter", Brush }, { "strikeBody", Skin }, { "strikeLength", 600 }, { "strikeRing", 1 },
                                    { "strikeTone", 1 }, { "strikeSweep", -24 }, { "chaosAmount", 0.8f }, { "chaosPan", 1 },
                                    { "delayNote", N16 }, { "delayFeel", Straight }, { "delayFeedback", 0.88f }, { "delayMix", 0.55f },
                                    { "roomSize", 1 }, { "roomDecay", 3 }, { "roomTone", 0.8f }, { "roomMix", 0.45f } } },

        // ---- Drive: the distortion before the filter (Pre) and after the reverb (Post)
        { "Fold Clang", "Drive", { Strike, { "strikeBody", Bar }, { "strikeRing", 0.85f }, { "driveType", 2 }, { "drivePost", 0 }, { "driveAmount", 0.55f }, { "driveMix", 0.8f }, { "filterType", 2 }, { "filterCutoff", 2500 }, { "filterResonance", 0.4f }, { "chaosAmount", 0.4f }, { "chaosPitch", 0.4f } } },
        { "Fuzz Grind", "Drive", { Strike, { "strikeExciter", Blow }, { "strikeBody", Bar }, { "strikeLength", 600 }, { "strikeRing", 0.7f }, { "strikeTone", 0.6f }, { "strikeMotion", 0.5f }, { "driveType", 1 }, { "drivePost", 0 }, { "driveAmount", 0.6f }, { "driveMix", 0.65f }, { "filterType", 1 }, { "filterCutoff", 3500 } } },
        // the echoes and the room go through the distortion too, and fold into a dense wall
        { "Industrial Wall", "Drive", { Strike, { "strikeExciter", Blow }, { "strikeBody", Bar }, { "strikeLength", 900 }, { "strikeRing", 0.75f }, { "strikeTone", 0.45f }, { "strikeMotion", 0.4f }, { "delayNote", 2 }, { "delayFeel", 2 }, { "delayFeedback", 0.55f }, { "delayMix", 0.4f }, { "roomSize", 0.7f }, { "roomDecay", 1.8f }, { "roomMix", 0.4f }, { "driveType", 1 }, { "drivePost", 1 }, { "driveAmount", 0.75f }, { "driveMix", 0.7f } } },
        { "Rusted Reverb", "Drive", { Strike, { "strikeBody", Bar }, { "strikeRing", 0.87f }, { "strikeTone", 0.4f }, { "roomSize", 0.8f }, { "roomDecay", 2.5f }, { "roomTone", 0.6f }, { "roomMix", 0.5f }, { "driveType", 2 }, { "drivePost", 1 }, { "driveAmount", 0.5f }, { "driveMix", 0.55f } } },
        { "Screech Fold", "Drive", { Strike, { "strikeExciter", Squeal }, { "strikeBody", Bar }, { "strikeLength", 900 }, { "strikeRing", 0.8f }, { "strikeTone", 0.6f }, { "strikeMotion", 0.45f }, { "driveType", 2 }, { "drivePost", 0 }, { "driveAmount", 0.5f }, { "driveMix", 0.75f }, { "chaosAmount", 0.3f }, { "chaosPitch", 0.3f } } },
        { "Scream Wall", "Drive", { Strike, { "strikeExciter", Squeal }, { "strikeBody", Bar }, { "strikeLength", 1400 }, { "strikeRing", 0.7f }, { "strikeTone", 0.7f }, { "strikeMotion", 0.5f }, { "delayNote", 2 }, { "delayFeel", 2 }, { "delayFeedback", 0.5f }, { "delayMix", 0.35f }, { "roomSize", 0.8f }, { "roomDecay", 2 }, { "roomMix", 0.4f }, { "driveType", 1 }, { "drivePost", 1 }, { "driveAmount", 0.7f }, { "driveMix", 0.65f } } },
        { "Folded Squeal", "Drive", { Strike, { "strikeExciter", Squeal }, { "strikeBody", Bar }, { "strikeLength", 700 }, { "strikeRing", 0.9f }, { "strikeMotion", 0.3f }, { "roomSize", 0.6f }, { "roomDecay", 1.5f }, { "roomMix", 0.35f }, { "driveType", 2 }, { "drivePost", 1 }, { "driveAmount", 0.6f }, { "driveMix", 0.6f } } },
    };
    return presets;
}
} // namespace bleepwerk

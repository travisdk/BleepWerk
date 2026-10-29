// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "Patch.h"

#include <vector>

// The factory sound presets, in the order the browser shows them. Each lists only what it
// changes from the defaults in kSoundParams; everything else, the output gain included, is
// reset on loading. The output gain is set by matchedOutputDb() so they all play about
// equally loud. A preset never touches the pitch bend range, a setting of the player. JUCE-free.
namespace bleepforge
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
inline constexpr float Noise = 0, Mallet = 1, Crackle = 2;
inline constexpr float LP = 1, BP = 2, HP = 3;
inline constexpr float N32 = 0, N16 = 1, N8 = 2, N4 = 3, N2 = 4;
inline constexpr float Straight = 0, Triplet = 1, Dotted = 2;
inline constexpr float Hit = 0, Hiss = 1, Grind = 2, Screech = 3;
inline constexpr float Mono = 0;
inline constexpr float Warm = 0, Hard = 1, Fold = 2;
inline constexpr float Pre = 0, Post = 1;

inline constexpr PresetValue Foley { "engine", 1 };
inline constexpr PresetValue Machine { "engine", 2 };
} // namespace preset

inline const std::vector<FactoryPreset>& factoryPresets()
{
    using namespace preset;
    static const std::vector<FactoryPreset> presets {
        // ---- Bleeps: sine and saw
        { "Init", "Bleeps", {} },
        { "Pure Blip", "Bleeps", { { "bleepPitchAmount", 0 }, { "bleepDecay", 120 }, { "bleepAttack", 0.3f } } },
        { "Soft Dot", "Bleeps", { { "bleepPitchAmount", 5 }, { "bleepPitchDecay", 15 }, { "bleepAttack", 3 }, { "bleepDecay", 90 },
                                  { "filterType", LP }, { "filterCutoff", 3000 } } },
        { "Ping Up", "Bleeps", { { "bleepPitchAmount", -12 }, { "bleepPitchDecay", 40 }, { "bleepDecay", 250 } } },
        { "Laser Drop", "Bleeps", { { "bleepPitchAmount", 36 }, { "bleepPitchDecay", 60 }, { "bleepDecay", 300 } } },
        { "Zap Blip", "Bleeps", { { "bleepPitchAmount", 48 }, { "bleepPitchDecay", 8 }, { "bleepDecay", 80 } } },
        { "Bubble", "Bleeps", { { "bleepPitchAmount", -24 }, { "bleepPitchDecay", 30 }, { "bleepAttack", 1 }, { "bleepDecay", 70 },
                                { "chaosAmount", 0.5f }, { "chaosPitch", 0.5f } } },
        { "Long Sine", "Bleeps", { { "bleepPitchAmount", 2 }, { "bleepDecay", 900 }, { "bleepRelease", 200 },
                                   { "roomSize", 0.4f }, { "roomDecay", 0.7f }, { "roomMix", 0.25f } } },
        { "Sine Kick Blip", "Bleeps", { { "bleepPitchAmount", 36 }, { "bleepPitchDecay", 35 }, { "bleepDecay", 220 } } },
        { "Click Sine", "Bleeps", { { "bleepPitchAmount", 24 }, { "bleepPitchDecay", 3 }, { "bleepDecay", 25 }, { "bleepNoise", 0.15f } } },
        { "Soft Blip", "Bleeps", { { "bleepPitchAmount", 7 }, { "bleepDecay", 140 } } },
        { "Saw Blip", "Bleeps", { { "bleepWave", Saw }, { "bleepPitchAmount", 7 }, { "bleepDecay", 140 },
                                  { "filterType", LP }, { "filterCutoff", 3000 } } },
        { "Bounce Blip", "Bleeps", { { "bleepPitchAmount", 24 }, { "bleepPitchDecay", 45 },
                                     { "bleepDecay", 220 }, { "chaosPitch", 0.3f } } },
        { "Hollow Sine", "Bleeps", { { "bleepPitchAmount", 0 }, { "bleepDecay", 400 },
                                     { "filterType", BP }, { "filterCutoff", 1200 }, { "filterResonance", 0.5f } } },
        { "Mellow Tone", "Bleeps", { { "bleepPitchAmount", 0 }, { "bleepAttack", 4 }, { "bleepDecay", 600 },
                                     { "bleepRelease", 150 }, { "filterType", LP }, { "filterCutoff", 1800 } } },
        { "Glide Sine", "Bleeps", { { "voiceMode", Mono }, { "glide", 80 }, { "bleepPitchAmount", 0 }, { "bleepDecay", 400 },
                                    { "bleepRelease", 120 } } },

        // ---- Pulses and ticks
        { "Square Bleep", "Pulses", { { "bleepWave", Pulse }, { "bleepPitchAmount", 0 }, { "bleepDecay", 150 },
                                      { "filterType", LP }, { "filterCutoff", 4000 } } },
        { "Retro Beep", "Pulses", { { "bleepWave", Pulse }, { "bleepPitchAmount", 0 }, { "bleepDecay", 300 }, { "bleepRelease", 40 } } },
        { "Narrow Pulse", "Pulses", { { "bleepWave", Pulse }, { "bleepPulseWidth", 0.12f }, { "bleepDecay", 120 } } },
        { "Needle Tick", "Pulses", { { "bleepWave", Pulse }, { "bleepPulseWidth", 0.08f }, { "bleepPitchAmount", 24 },
                                     { "bleepPitchDecay", 5 }, { "bleepDecay", 15 }, { "filterType", HP }, { "filterCutoff", 3000 } } },
        { "Digital Click", "Pulses", { { "bleepWave", Pulse }, { "bleepPitchAmount", 48 }, { "bleepPitchDecay", 2 }, { "bleepDecay", 8 },
                                       { "filterType", HP }, { "filterCutoff", 1500 } } },
        { "Reedy Blip", "Pulses", { { "bleepWave", Pulse }, { "bleepPulseWidth", 0.3f }, { "bleepPitchAmount", 3 }, { "bleepDecay", 200 },
                                    { "filterType", BP }, { "filterCutoff", 1500 }, { "filterResonance", 0.4f } } },
        { "Hollow Square", "Pulses", { { "bleepWave", Pulse }, { "bleepPitchAmount", 0 }, { "bleepDecay", 260 },
                                       { "filterType", HP }, { "filterCutoff", 600 }, { "filterResonance", 0.3f } } },
        { "Square Pluck", "Pulses", { { "bleepWave", Pulse }, { "bleepPulseWidth", 0.7f }, { "bleepPitchAmount", 0 }, { "bleepDecay", 350 },
                                      { "filterType", LP }, { "filterCutoff", 2500 }, { "filterResonance", 0.6f } } },
        { "Resonant Square", "Pulses", { { "bleepWave", Pulse }, { "bleepPitchAmount", 0 }, { "bleepDecay", 200 },
                                         { "filterType", LP }, { "filterCutoff", 800 }, { "filterResonance", 0.8f } } },
        { "Glide Square", "Pulses", { { "bleepWave", Pulse }, { "voiceMode", Mono }, { "glide", 60 }, { "bleepPitchAmount", 0 },
                                      { "bleepDecay", 300 }, { "bleepRelease", 80 }, { "filterType", LP }, { "filterCutoff", 3000 } } },
        { "Chip Arp", "Pulses", { { "bleepWave", Pulse }, { "bleepPulseWidth", 0.25f }, { "bleepPitchAmount", 12 }, { "bleepPitchDecay", 4 },
                                  { "bleepDecay", 110 }, { "crushBits", 8 } } },
        { "Tick Tock", "Pulses", { { "bleepWave", Pulse }, { "bleepPitchAmount", 0 }, { "bleepDecay", 30 },
                                   { "filterType", BP }, { "filterCutoff", 2500 }, { "filterResonance", 0.7f } } },
        { "Thin Buzz", "Pulses", { { "bleepWave", Pulse }, { "bleepPulseWidth", 0.05f }, { "bleepPitchAmount", -5 }, { "bleepPitchDecay", 20 },
                                   { "bleepDecay", 180 }, { "filterType", LP }, { "filterCutoff", 5000 } } },

        // ---- FM
        { "FM Bell", "FM", { { "bleepWave", FM }, { "bleepFmRatio", 3.5f }, { "bleepFmAmount", 0.45f }, { "bleepPitchAmount", 0 },
                             { "bleepDecay", 900 }, { "bleepRelease", 300 }, { "roomSize", 0.5f }, { "roomDecay", 0.9f }, { "roomMix", 0.3f } } },
        { "FM Tine", "FM", { { "bleepWave", FM }, { "bleepFmRatio", 4 }, { "bleepFmAmount", 0.3f }, { "bleepPitchAmount", 0 },
                             { "bleepDecay", 500 } } },
        { "FM Metal", "FM", { { "bleepWave", FM }, { "bleepFmRatio", 1.5f }, { "bleepFmAmount", 0.8f }, { "bleepPitchAmount", 0 },
                              { "bleepDecay", 300 } } },
        { "FM Zap", "FM", { { "bleepWave", FM }, { "bleepFmRatio", 2 }, { "bleepFmAmount", 0.7f }, { "bleepPitchAmount", 36 },
                            { "bleepPitchDecay", 20 }, { "bleepDecay", 150 } } },
        { "FM Woodblock", "FM", { { "bleepWave", FM }, { "bleepFmRatio", 3.5f }, { "bleepFmAmount", 0.2f }, { "bleepPitchAmount", 12 },
                                  { "bleepPitchDecay", 6 }, { "bleepDecay", 70 } } },
        { "FM Glass", "FM", { { "bleepWave", FM }, { "bleepFmRatio", 7 }, { "bleepFmAmount", 0.25f }, { "bleepPitchAmount", 0 },
                              { "bleepDecay", 1200 }, { "bleepRelease", 400 }, { "roomDecay", 0.7f }, { "roomMix", 0.35f } } },
        { "FM Gong Tink", "FM", { { "bleepWave", FM }, { "bleepFmRatio", 5.5f }, { "bleepFmAmount", 0.6f }, { "bleepPitchAmount", 0 },
                                  { "bleepDecay", 1500 }, { "bleepRelease", 400 }, { "roomDecay", 0.7f }, { "roomMix", 0.2f } } },
        { "FM Growl", "FM", { { "bleepWave", FM }, { "bleepFmRatio", 1 }, { "bleepFmAmount", 0.9f }, { "bleepPitchAmount", 0 },
                              { "bleepDecay", 180 }, { "filterType", LP }, { "filterCutoff", 2500 } } },
        { "FM Cowbell", "FM", { { "bleepWave", FM }, { "bleepFmRatio", 2.5f }, { "bleepFmAmount", 0.5f }, { "bleepPitchAmount", 0 },
                                { "bleepDecay", 250 }, { "filterType", BP }, { "filterCutoff", 1000 }, { "filterResonance", 0.3f } } },
        { "FM Rise", "FM", { { "bleepWave", FM }, { "bleepFmRatio", 3 }, { "bleepFmAmount", 0.5f }, { "bleepPitchAmount", -24 },
                             { "bleepPitchDecay", 60 }, { "bleepDecay", 300 } } },
        { "FM Pluck", "FM", { { "bleepWave", FM }, { "bleepFmRatio", 1 }, { "bleepFmAmount", 0.5f }, { "bleepPitchAmount", 0 },
                              { "bleepDecay", 220 } } },
        { "FM Bright Ping", "FM", { { "bleepWave", FM }, { "bleepFmRatio", 8 }, { "bleepFmAmount", 0.35f }, { "bleepPitchAmount", 0 },
                                    { "bleepDecay", 400 } } },
        { "FM Clang", "FM", { { "bleepWave", FM }, { "bleepFmRatio", 7.5f }, { "bleepFmAmount", 1 }, { "bleepPitchAmount", 0 },
                              { "bleepDecay", 600 }, { "filterType", HP }, { "filterCutoff", 400 } } },
        { "FM Sub Pop", "FM", { { "bleepWave", FM }, { "bleepFmRatio", 0.5f }, { "bleepFmAmount", 0.3f }, { "bleepPitchAmount", 24 },
                                { "bleepPitchDecay", 10 }, { "bleepDecay", 150 } } },
        { "FM Noise Hit", "FM", { { "bleepWave", FM }, { "bleepFmRatio", 6.5f }, { "bleepFmAmount", 1 }, { "bleepPitchAmount", 0 },
                                  { "bleepDecay", 60 }, { "bleepNoise", 0.2f }, { "filterType", HP }, { "filterCutoff", 2000 } } },
        { "FM Marimba", "FM", { { "bleepWave", FM }, { "bleepFmRatio", 4 }, { "bleepFmAmount", 0.15f }, { "bleepPitchAmount", 0 },
                                { "bleepAttack", 1 }, { "bleepDecay", 350 }, { "filterType", LP }, { "filterCutoff", 3500 } } },
        { "FM Chaos Tink", "FM", { { "bleepWave", FM }, { "bleepFmRatio", 5 }, { "bleepFmAmount", 0.4f }, { "bleepPitchAmount", 0 },
                                   { "bleepDecay", 200 }, { "chaosAmount", 0.8f }, { "chaosPitch", 0.4f }, { "chaosTone", 0.8f },
                                   { "chaosPan", 0.9f } } },

        // ---- Noise and hats
        { "Noise Blip", "Noise", { { "bleepNoise", 0.8f }, { "bleepDecay", 50 }, { "filterType", HP }, { "filterCutoff", 5000 } } },
        { "Closed Hat", "Noise", { { "bleepNoise", 1 }, { "bleepDecay", 35 }, { "filterType", HP }, { "filterCutoff", 7000 } } },
        { "Open Hat", "Noise", { { "bleepNoise", 1 }, { "bleepDecay", 300 }, { "bleepRelease", 200 }, { "filterType", HP },
                                 { "filterCutoff", 6500 } } },
        { "Shaker", "Noise", { { "bleepNoise", 1 }, { "bleepAttack", 6 }, { "bleepDecay", 70 }, { "filterType", BP }, { "filterCutoff", 5000 },
                               { "filterResonance", 0.3f }, { "chaosAmount", 0.5f }, { "chaosDecay", 0.8f } } },
        { "Snare Blip", "Noise", { { "bleepNoise", 0.55f }, { "bleepPitchAmount", 12 }, { "bleepPitchDecay", 15 }, { "bleepDecay", 140 },
                                   { "filterType", HP }, { "filterCutoff", 300 } } },
        { "Air Puff", "Noise", { { "bleepNoise", 1 }, { "bleepAttack", 15 }, { "bleepDecay", 120 }, { "filterType", LP },
                                 { "filterCutoff", 2500 } } },
        { "Whistle Hit", "Noise", { { "bleepNoise", 0.9f }, { "bleepDecay", 300 }, { "filterType", BP }, { "filterCutoff", 1200 },
                                    { "filterResonance", 0.6f } } },
        { "Rim Shot", "Noise", { { "bleepNoise", 0.4f }, { "bleepPitchAmount", 24 }, { "bleepPitchDecay", 4 },
                                 { "bleepDecay", 40 }, { "filterType", BP }, { "filterCutoff", 1800 }, { "filterResonance", 0.4f } } },
        { "Hiss Tick", "Noise", { { "bleepNoise", 1 }, { "bleepDecay", 12 }, { "filterType", HP }, { "filterCutoff", 9000 } } },
        { "Brush", "Noise", { { "bleepNoise", 1 }, { "bleepAttack", 10 }, { "bleepDecay", 160 }, { "filterType", BP }, { "filterCutoff", 3000 },
                              { "filterResonance", 0.1f }, { "chaosAmount", 0.6f }, { "chaosDecay", 0.7f }, { "chaosPan", 0.7f } } },

        // ---- Lo-fi
        { "Lo-fi Bleep", "Lo-fi", { { "bleepWave", Pulse }, { "bleepPitchAmount", 0 }, { "bleepDecay", 180 }, { "crushBits", 6 },
                                    { "crushRate", 8000 } } },
        { "8-bit Blip", "Lo-fi", { { "bleepWave", Pulse }, { "bleepPulseWidth", 0.25f }, { "bleepPitchAmount", 12 }, { "bleepPitchDecay", 8 },
                                   { "bleepDecay", 120 }, { "crushBits", 4 } } },
        { "Crushed Sine", "Lo-fi", { { "bleepPitchAmount", 0 }, { "bleepDecay", 250 }, { "crushBits", 3 } } },
        { "Aliased Zap", "Lo-fi", { { "bleepPitchAmount", 48 }, { "bleepPitchDecay", 30 }, { "bleepDecay", 200 }, { "crushRate", 3000 } } },
        { "Broken Toy", "Lo-fi", { { "bleepPitchAmount", 0 }, { "bleepDecay", 300 }, { "crushBits", 7 },
                                   { "crushRate", 2500 }, { "chaosAmount", 0.6f }, { "chaosPitch", 0.5f } } },
        { "Crushed Hat", "Lo-fi", { { "bleepNoise", 1 }, { "bleepDecay", 40 }, { "crushBits", 5 }, { "crushRate", 12000 },
                                    { "filterType", HP }, { "filterCutoff", 3000 } } },
        { "Radio Beep", "Lo-fi", { { "bleepWave", Pulse }, { "bleepPulseWidth", 0.4f }, { "bleepPitchAmount", 0 }, { "bleepDecay", 220 },
                                   { "filterType", BP }, { "filterCutoff", 1500 }, { "filterResonance", 0.5f }, { "crushBits", 8 },
                                   { "crushRate", 11000 } } },
        { "Lo-fi FM", "Lo-fi", { { "bleepWave", FM }, { "bleepFmRatio", 3.5f }, { "bleepFmAmount", 0.5f }, { "bleepPitchAmount", 0 },
                                 { "bleepDecay", 400 }, { "crushBits", 6 }, { "crushRate", 6000 } } },
        { "Decimated Wood", "Lo-fi", { Foley, { "foleyLength", 0.5f }, { "crushBits", 8 }, { "crushRate", 4000 } } },
        { "Grit Pulse", "Lo-fi", { { "bleepWave", Pulse }, { "bleepPulseWidth", 0.15f }, { "bleepPitchAmount", 0 }, { "bleepDecay", 160 },
                                   { "crushRate", 1500 }, { "filterType", LP }, { "filterCutoff", 6000 } } },

        // ---- Dub and delay
        { "Dub Bleep", "Dub", { { "bleepPitchAmount", 0 }, { "bleepDecay", 200 }, { "filterType", LP }, { "filterCutoff", 2500 },
                                { "delayNote", N8 }, { "delayFeel", Dotted }, { "delayFeedback", 0.6f }, { "delayMix", 0.4f } } },
        { "Echo Blip", "Dub", { { "bleepPitchAmount", 12 }, { "bleepPitchDecay", 15 }, { "bleepDecay", 120 },
                                { "delayNote", N16 }, { "delayFeel", Dotted }, { "delayFeedback", 0.5f }, { "delayMix", 0.35f } } },
        { "Space Stab", "Dub", { { "bleepWave", Pulse }, { "bleepPitchAmount", 0 }, { "bleepDecay", 250 }, { "filterType", LP },
                                 { "filterCutoff", 1500 }, { "filterResonance", 0.4f }, { "delayNote", N4 }, { "delayFeel", Straight }, { "delayFeedback", 0.62f },
                                 { "delayMix", 0.4f }, { "roomSize", 0.7f }, { "roomDecay", 1.1f }, { "roomMix", 0.3f } } },
        { "Triplet Echo", "Dub", { { "bleepDecay", 150 }, { "delayNote", N8 }, { "delayFeel", Triplet }, { "delayFeedback", 0.55f },
                                   { "delayMix", 0.4f } } },
        { "Dub Tick", "Dub", { { "bleepWave", Pulse }, { "bleepPulseWidth", 0.1f }, { "bleepDecay", 20 }, { "filterType", HP },
                               { "filterCutoff", 2000 }, { "delayNote", N16 }, { "delayFeel", Dotted }, { "delayFeedback", 0.7f }, { "delayMix", 0.45f } } },
        { "Dub Bell", "Dub", { { "bleepWave", FM }, { "bleepFmRatio", 3.5f }, { "bleepFmAmount", 0.4f }, { "bleepPitchAmount", 0 },
                               { "bleepDecay", 700 }, { "delayNote", N4 }, { "delayFeel", Triplet }, { "delayFeedback", 0.55f }, { "delayMix", 0.35f },
                               { "roomDecay", 0.7f }, { "roomMix", 0.3f } } },
        { "Echo Plate", "Dub", { Foley, { "foleyMaterial", 0.5f }, { "foleySize", 0.4f }, { "foleyDamping", 0.4f }, { "foleyBrightness", 0.6f },
                                 { "delayNote", N8 }, { "delayFeel", Straight }, { "delayFeedback", 0.5f }, { "delayMix", 0.35f } } },
        { "Long Echo", "Dub", { { "bleepPitchAmount", 0 }, { "bleepDecay", 150 }, { "filterType", LP }, { "filterCutoff", 1800 },
                                { "delayNote", N8 }, { "delayFeel", Dotted }, { "delayFeedback", 0.65f }, { "delayMix", 0.45f } } },
        { "Slapback Blip", "Dub", { { "bleepDecay", 100 }, { "delayNote", N32 }, { "delayFeel", Straight }, { "delayFeedback", 0.2f }, { "delayMix", 0.35f } } },
        { "Dub Hat", "Dub", { { "bleepNoise", 1 }, { "bleepDecay", 30 }, { "filterType", HP }, { "filterCutoff", 6000 },
                              { "delayNote", N16 }, { "delayFeel", Dotted }, { "delayFeedback", 0.55f }, { "delayMix", 0.4f } } },
        { "Chord Echo", "Dub", { { "bleepPitchAmount", 0 }, { "bleepDecay", 300 }, { "filterType", BP },
                                 { "filterCutoff", 900 }, { "filterResonance", 0.35f }, { "delayNote", N8 }, { "delayFeel", Dotted }, { "delayFeedback", 0.7f },
                                 { "delayMix", 0.5f }, { "roomSize", 0.8f }, { "roomDecay", 1.2f }, { "roomMix", 0.35f } } },

        // ---- Wood
        { "Wood Knock", "Wood", { Foley, { "foleyLength", 0.5f } } },
        { "Wood Block", "Wood", { Foley, { "foleyLength", 0.2f }, { "foleyMaterial", 0.05f }, { "foleySize", 0.3f }, { "foleyDamping", 0.35f },
                                  { "foleyBrightness", 0.65f } } },
        { "Log Drum", "Wood", { Foley, { "foleyLength", 2 }, { "foleySize", 0.8f }, { "foleyDamping", 0.3f }, { "foleyBrightness", 0.35f } } },
        { "Claves", "Wood", { Foley, { "foleyLength", 0.1f }, { "foleyMaterial", 0.08f }, { "foleySize", 0.2f }, { "foleyDamping", 0.25f },
                              { "foleyBrightness", 0.8f } } },
        { "Hollow Wood", "Wood", { Foley, { "foleyLength", 1 }, { "foleySize", 0.65f }, { "foleyDamping", 0.2f }, { "foleyBrightness", 0.4f },
                                   { "roomDecay", 0.7f }, { "roomMix", 0.25f } } },
        { "Dead Knock", "Wood", { Foley, { "foleyLength", 0.5f }, { "foleyDamping", 0.9f } } },
        { "Soft Tap", "Wood", { Foley, { "foleyLength", 4 }, { "foleyDamping", 0.6f }, { "foleyBrightness", 0.2f } } },
        { "Bar Mallet", "Wood", { Foley, { "foleyLength", 1.5f }, { "foleyMaterial", 0.1f }, { "foleySize", 0.55f }, { "foleyDamping", 0.15f },
                                  { "foleyBrightness", 0.45f } } },
        { "Knuckle Rap", "Wood", { Foley, { "foleyExciter", Noise }, { "foleyLength", 2 }, { "foleySize", 0.4f }, { "foleyDamping", 0.7f },
                                   { "foleyBrightness", 0.6f } } },
        { "Stick Click", "Wood", { Foley, { "foleyLength", 0.05f }, { "foleyMaterial", 0.15f }, { "foleySize", 0.1f }, { "foleyDamping", 0.6f },
                                   { "foleyBrightness", 1 } } },
        { "Wood Chaos", "Wood", { Foley, { "foleyLength", 0.4f }, { "foleySize", 0.35f }, { "foleyBrightness", 0.6f }, { "chaosAmount", 0.8f },
                                  { "chaosPitch", 0.5f }, { "chaosTone", 0.8f }, { "chaosPan", 0.8f } } },

        // ---- Metal
        { "Metal Tink", "Metal", { Foley, { "foleyLength", 0.1f }, { "foleyMaterial", 0.5f }, { "foleySize", 0.2f }, { "foleyDamping", 0.3f },
                                   { "foleyBrightness", 0.8f }, { "roomDecay", 0.7f }, { "roomMix", 0.2f } } },
        { "Pipe Clank", "Metal", { Foley, { "foleyLength", 0.5f }, { "foleyMaterial", 0.45f }, { "foleySize", 0.85f }, { "foleyDamping", 0.45f } } },
        { "Bell Plate", "Metal", { Foley, { "foleyMaterial", 0.55f }, { "foleySize", 0.6f }, { "foleyDamping", 0.15f }, { "foleyBrightness", 0.6f },
                                   { "roomDecay", 0.7f }, { "roomMix", 0.25f } } },
        { "Spoon Hit", "Metal", { Foley, { "foleyLength", 0.1f }, { "foleyMaterial", 0.5f }, { "foleySize", 0.1f }, { "foleyDamping", 0.55f },
                                  { "foleyBrightness", 0.9f } } },
        { "Gong Tap", "Metal", { Foley, { "foleyLength", 3 }, { "foleyMaterial", 0.5f }, { "foleySize", 1 }, { "foleyDamping", 0.2f },
                                 { "foleyBrightness", 0.35f }, { "roomSize", 0.8f }, { "roomDecay", 1.2f }, { "roomMix", 0.3f } } },
        { "Tin Can", "Metal", { Foley, { "foleyExciter", Noise }, { "foleyLength", 1 }, { "foleyMaterial", 0.42f }, { "foleySize", 0.35f },
                                { "foleyDamping", 0.6f }, { "foleyBrightness", 0.7f } } },
        { "Steel Pan", "Metal", { Foley, { "foleyLength", 1 }, { "foleyMaterial", 0.35f }, { "foleyDamping", 0.25f }, { "foleyBrightness", 0.45f } } },
        { "Chain Rattle", "Metal", { Foley, { "foleyExciter", Crackle }, { "foleyLength", 60 }, { "foleyMaterial", 0.5f }, { "foleySize", 0.2f },
                                     { "foleyDamping", 0.65f }, { "foleyBrightness", 0.8f }, { "chaosAmount", 0.6f } } },
        { "Metal Cowbell", "Metal", { Foley, { "foleyMaterial", 0.48f }, { "foleySize", 0.45f }, { "foleyDamping", 0.45f },
                                      { "foleyBrightness", 0.65f }, { "filterType", BP }, { "filterCutoff", 900 }, { "filterResonance", 0.3f } } },
        { "Anvil", "Metal", { Foley, { "foleyLength", 0.1f }, { "foleyMaterial", 0.52f }, { "foleySize", 0.7f }, { "foleyDamping", 0.25f },
                              { "foleyBrightness", 1 } } },
        { "Muted Metal", "Metal", { Foley, { "foleyLength", 0.4f }, { "foleyMaterial", 0.5f }, { "foleySize", 0.4f }, { "foleyDamping", 0.8f },
                                    { "foleyBrightness", 0.55f } } },
        { "Metal Chaos", "Metal", { Foley, { "foleyLength", 0.2f }, { "foleyMaterial", 0.5f }, { "foleySize", 0.3f }, { "foleyDamping", 0.4f },
                                    { "foleyBrightness", 0.75f }, { "chaosAmount", 0.9f }, { "chaosPitch", 0.6f }, { "chaosDecay", 0.8f },
                                    { "chaosPan", 0.9f } } },
        { "Steel Beam", "Metal", { Foley, { "foleyLength", 2 }, { "foleyMaterial", 0.47f }, { "foleySize", 0.95f }, { "foleyDamping", 0.35f },
                                   { "foleyBrightness", 0.55f }, { "roomSize", 0.7f }, { "roomDecay", 1.1f }, { "roomMix", 0.25f } } },
        { "Air Valve", "Metal", { Foley, { "foleyExciter", Noise }, { "foleyLength", 4 }, { "foleyMaterial", 0.5f }, { "foleySize", 0.15f },
                                  { "foleyDamping", 0.85f }, { "foleyBrightness", 1 }, { "filterType", HP }, { "filterCutoff", 1500 } } },
        { "Rivet Gun", "Metal", { Foley, { "foleyExciter", Crackle }, { "foleyLength", 25 }, { "foleyMaterial", 0.5f }, { "foleySize", 0.35f },
                                  { "foleyDamping", 0.6f }, { "foleyBrightness", 0.85f }, { "crushBits", 8 } } },
        { "Press Stamp", "Metal", { Foley, { "foleyExciter", Noise }, { "foleyLength", 1.5f }, { "foleyMaterial", 0.45f }, { "foleySize", 0.8f },
                                    { "foleyDamping", 0.7f }, { "foleyBrightness", 0.4f }, { "foleySweep", -5 } } },

        // ---- Machine: struck parts, steam, grinders and screeching metal, tuned to the note
        { "Press Clonk", "Machine", { Machine, { "machineType", Hit }, { "machineLength", 900 }, { "machineTone", 0.45f },
                                      { "machineFocus", 0.4f }, { "machineMetal", 0.75f }, { "chaosAmount", 0.5f }, { "chaosPitch", 0.6f },
                                      { "roomDecay", 0.7f }, { "roomMix", 0.2f } } },
        { "Heavy Hit", "Machine", { Machine, { "machineType", Hit }, { "machineLength", 1500 }, { "machineTone", 0.3f },
                                    { "machineFocus", 0.1f }, { "machineMetal", 0.6f }, { "chaosAmount", 0.4f } } },
        { "Rattling Plate", "Machine", { Machine, { "machineType", Hit }, { "machineLength", 700 }, { "machineTone", 0.65f },
                                         { "machineFocus", 0.7f }, { "machineMotion", 0.55f }, { "machineMetal", 0.9f },
                                         { "chaosAmount", 0.6f }, { "chaosPitch", 0.7f } } },
        { "Donk", "Machine", { Machine, { "machineType", Hit }, { "roomSize", 1.0f }, { "roomDecay", 4.0f }, { "roomMix", 0.12f },
                               { "roomDuck", 0.12f } } },
        { "Steam Valve", "Machine", { Machine, { "machineType", Hiss }, { "machineLength", 300 }, { "machineTone", 0.75f },
                                      { "machineFocus", 0.2f }, { "machineMetal", 0.15f } } },
        { "Pressure Leak", "Machine", { Machine, { "machineType", Hiss }, { "machineLength", 1200 }, { "machineTone", 0.85f },
                                        { "machineFocus", 0.1f }, { "machineMotion", 0.35f }, { "machineMetal", 0.25f },
                                        { "roomDecay", 0.7f }, { "roomMix", 0.2f } } },
        { "Whistle Pipe", "Machine", { Machine, { "machineType", Hiss }, { "machineLength", 600 }, { "machineTone", 0.25f },
                                       { "machineFocus", 0.95f }, { "machineMetal", 0.4f } } },
        { "Air Brake", "Machine", { Machine, { "machineType", Hiss }, { "machineLength", 180 }, { "machineTone", 0.9f },
                                    { "machineFocus", 0.35f }, { "machineMetal", 0.1f }, { "chaosAmount", 0.4f }, { "chaosTone", 0.6f } } },
        { "Grinder", "Machine", { Machine, { "machineType", Grind }, { "machineLength", 900 }, { "machineTone", 0.7f },
                                  { "machineFocus", 0.8f }, { "machineMotion", 0.4f }, { "machineMetal", 0.7f } } },
        { "Lathe", "Machine", { Machine, { "machineType", Grind }, { "machineLength", 2500 }, { "machineTone", 0.5f },
                                { "machineFocus", 0.9f }, { "machineMotion", 0.2f }, { "machineMetal", 0.8f }, { "roomDecay", 0.7f }, { "roomMix", 0.2f } } },
        { "Sander", "Machine", { Machine, { "machineType", Grind }, { "machineLength", 400 }, { "machineTone", 0.9f },
                                 { "machineFocus", 0.4f }, { "machineMotion", 0.7f }, { "machineMetal", 0.3f } } },
        { "Conveyor", "Machine", { Machine, { "machineType", Grind }, { "machineLength", 1500 }, { "machineTone", 0.35f },
                                   { "machineFocus", 0.6f }, { "machineMotion", 0.55f }, { "machineMetal", 0.55f }, { "delayNote", N8 }, { "delayFeel", Dotted },
                                   { "delayFeedback", 0.4f }, { "delayMix", 0.2f } } },
        { "Rail Squeal", "Machine", { Machine, { "machineType", Screech }, { "machineLength", 1500 }, { "machineTone", 0.75f },
                                      { "machineFocus", 0.7f }, { "machineMotion", 0.2f }, { "machineMetal", 0.6f },
                                      { "roomDecay", 0.7f }, { "roomMix", 0.2f } } },
        { "Brake Squeal", "Machine", { Machine, { "machineType", Screech }, { "machineLength", 1200 }, { "machineTone", 0.85f },
                                       { "machineFocus", 0.85f }, { "machineMotion", 0.75f }, { "machineMetal", 0.4f },
                                       { "chaosAmount", 0.3f }, { "chaosPitch", 0.3f } } },
        { "Metal Scrape", "Machine", { Machine, { "machineType", Screech }, { "machineLength", 600 }, { "machineTone", 0.45f },
                                       { "machineFocus", 0.3f }, { "machineMotion", 0.5f }, { "machineMetal", 0.5f } } },

        // ---- Glass and ceramic
        { "Glass Ping", "Glass", { Foley, { "foleyLength", 0.1f }, { "foleyMaterial", 1 }, { "foleySize", 0.4f }, { "foleyDamping", 0.2f },
                                   { "foleyBrightness", 0.8f } } },
        { "Wine Glass", "Glass", { Foley, { "foleyLength", 0.4f }, { "foleyMaterial", 0.95f }, { "foleySize", 0.6f }, { "foleyDamping", 0.1f },
                                   { "roomDecay", 0.7f }, { "roomMix", 0.2f } } },
        { "Bottle Tap", "Glass", { Foley, { "foleyLength", 0.2f }, { "foleyMaterial", 0.85f }, { "foleyDamping", 0.35f }, { "foleyBrightness", 0.6f } } },
        // a bottle struck with a key: a short noise burst scrapes every mode at once, the
        // metal-leaning blend makes the close modes beat, and a light sample-rate hold folds
        // the top back down as inharmonic grit, which together read as metal grating on glass
        { "Bottle Clink", "Glass", { Foley, { "foleyExciter", Noise }, { "foleyLength", 0.4f }, { "foleyMaterial", 0.7f }, { "foleySize", 0.25f },
                                     { "foleyDamping", 0.4f }, { "foleyBrightness", 1 }, { "filterType", HP }, { "filterCutoff", 900 },
                                     { "filterResonance", 0.35f }, { "crushRate", 14000 },
                                     { "delayNote", N8 }, { "delayFeel", Dotted }, { "delayFeedback", 0.35f }, { "delayMix", 0.18f }, { "roomSize", 0.3f },
                                     { "roomDecay", 0.6f }, { "roomMix", 0.15f }, { "chaosAmount", 0.35f }, { "chaosPitch", 0.12f }, { "chaosTone", 0.45f } } },
        { "Ice Clink", "Glass", { Foley, { "foleyLength", 0.05f }, { "foleyMaterial", 1 }, { "foleySize", 0.05f }, { "foleyDamping", 0.4f },
                                  { "foleyBrightness", 1 }, { "chaosAmount", 0.5f }, { "chaosPitch", 0.5f } } },
        { "Crystal", "Glass", { Foley, { "foleyLength", 0.2f }, { "foleyMaterial", 0.9f }, { "foleySize", 0.3f }, { "foleyDamping", 0.1f },
                                { "foleyBrightness", 0.7f }, { "delayNote", N8 }, { "delayFeel", Dotted }, { "delayFeedback", 0.4f }, { "delayMix", 0.25f },
                                { "roomDecay", 0.7f }, { "roomMix", 0.3f } } },
        { "Glass Shard", "Glass", { Foley, { "foleyExciter", Noise }, { "foleyLength", 0.5f }, { "foleyMaterial", 1 }, { "foleySize", 0.15f },
                                    { "foleyBrightness", 0.9f } } },
        { "Ceramic Tap", "Glass", { Foley, { "foleyLength", 0.2f }, { "foleyMaterial", 0.75f }, { "foleySize", 0.4f }, { "foleyDamping", 0.55f },
                                    { "foleyBrightness", 0.6f } } },
        { "Tiny Bell", "Glass", { Foley, { "foleyLength", 0.1f }, { "foleyMaterial", 0.8f }, { "foleySize", 0.1f }, { "foleyDamping", 0.15f },
                                  { "foleyBrightness", 0.75f }, { "roomSize", 0.3f }, { "roomDecay", 0.6f }, { "roomMix", 0.25f } } },

        // ---- Drips and water: the sweep slides each hit into its note
        { "Drip", "Drips", { Foley, { "foleyMaterial", 0.7f }, { "foleySize", 0.4f }, { "foleyDamping", 0.7f }, { "foleySweep", 12 } } },
        { "Water Drop", "Drips", { Foley, { "foleyLength", 0.5f }, { "foleyMaterial", 0.6f }, { "foleyDamping", 0.6f }, { "foleyBrightness", 0.4f },
                                   { "foleySweep", 19 }, { "delayMix", 0.2f } } },
        { "Faucet", "Drips", { Foley, { "foleyLength", 0.2f }, { "foleyMaterial", 0.8f }, { "foleySize", 0.3f }, { "foleyDamping", 0.65f },
                               { "foleyBrightness", 0.6f }, { "foleySweep", 7 }, { "chaosAmount", 0.6f }, { "chaosPitch", 0.6f },
                               { "roomDecay", 0.7f }, { "roomMix", 0.25f } } },
        { "Bloop", "Drips", { Foley, { "foleyLength", 1 }, { "foleyMaterial", 0.2f }, { "foleySize", 0.6f }, { "foleyDamping", 0.75f },
                              { "foleyBrightness", 0.3f }, { "foleySweep", 24 } } },
        { "Cave Drip", "Drips", { Foley, { "foleyMaterial", 0.7f }, { "foleySweep", 12 }, { "delayNote", N4 }, { "delayFeel", Straight }, { "delayFeedback", 0.4f },
                                  { "delayMix", 0.2f }, { "roomSize", 0.9f }, { "roomDecay", 1.4f }, { "roomMix", 0.45f } } },
        { "Falling Drop", "Drips", { Foley, { "foleyMaterial", 0.65f }, { "foleyDamping", 0.6f }, { "foleySweep", -12 } } },
        { "Plip", "Drips", { Foley, { "foleyLength", 0.1f }, { "foleyMaterial", 0.9f }, { "foleySize", 0.1f }, { "foleyDamping", 0.75f },
                             { "foleyBrightness", 0.8f }, { "foleySweep", 5 } } },
        { "Rain Drops", "Drips", { Foley, { "foleyExciter", Crackle }, { "foleyLength", 30 }, { "foleyMaterial", 0.7f }, { "foleySize", 0.3f },
                                   { "foleyDamping", 0.7f }, { "foleySweep", 7 }, { "chaosAmount", 0.8f } } },
        { "Sine Drip", "Drips", { { "bleepPitchAmount", -19 }, { "bleepPitchDecay", 18 }, { "bleepDecay", 60 }, { "roomDecay", 0.7f }, { "roomMix", 0.2f } } },

        // ---- Textures: crackles and scrapes
        { "Crackle", "Textures", { Foley, { "foleyExciter", Crackle }, { "foleyLength", 40 }, { "foleyMaterial", 0.2f }, { "foleySize", 0.3f },
                                   { "foleyDamping", 0.7f }, { "foleyBrightness", 0.8f }, { "chaosAmount", 0.7f }, { "chaosPan", 0.8f } } },
        { "Vinyl Dust", "Textures", { Foley, { "foleyExciter", Crackle }, { "foleyLength", 60 }, { "foleySize", 0.25f }, { "foleyDamping", 0.5f },
                                      { "foleyBrightness", 1 }, { "crushBits", 10 }, { "chaosAmount", 0.5f } } },
        { "Static Burst", "Textures", { Foley, { "foleyExciter", Crackle }, { "foleyLength", 80 }, { "foleyMaterial", 0.5f }, { "foleySize", 0.2f },
                                        { "foleyDamping", 0.85f }, { "foleyBrightness", 1 }, { "crushBits", 6 } } },
        { "Gravel", "Textures", { Foley, { "foleyExciter", Crackle }, { "foleyLength", 60 }, { "foleySize", 0.4f }, { "foleyDamping", 0.55f },
                                  { "foleyBrightness", 0.7f } } },
        { "Sizzle", "Textures", { Foley, { "foleyExciter", Crackle }, { "foleyLength", 120 }, { "foleyMaterial", 0.9f }, { "foleySize", 0.2f },
                                  { "foleyDamping", 0.5f }, { "foleyBrightness", 0.9f }, { "filterType", HP }, { "filterCutoff", 2000 } } },
        { "Embers", "Textures", { Foley, { "foleyExciter", Crackle }, { "foleyLength", 200 }, { "foleyMaterial", 0.3f }, { "foleyDamping", 0.75f },
                                  { "foleyBrightness", 0.4f }, { "roomDecay", 0.7f }, { "roomMix", 0.35f }, { "chaosAmount", 0.8f } } },
        { "Paper Crunch", "Textures", { Foley, { "foleyExciter", Noise }, { "foleyLength", 12 }, { "foleySize", 0.25f }, { "foleyDamping", 0.5f },
                                        { "foleyBrightness", 0.9f } } },
        { "Scrape", "Textures", { Foley, { "foleyExciter", Noise }, { "foleyLength", 80 }, { "foleyMaterial", 0.5f }, { "foleySize", 0.3f },
                                  { "foleyDamping", 0.7f }, { "foleyBrightness", 0.6f }, { "foleySweep", 5 } } },
        { "Rattle Echo", "Textures", { Foley, { "foleyExciter", Crackle }, { "foleyLength", 50 }, { "foleyMaterial", 0.4f }, { "foleySize", 0.3f },
                                       { "foleyDamping", 0.7f }, { "foleyBrightness", 0.7f }, { "delayNote", N16 }, { "delayFeel", Dotted }, { "delayFeedback", 0.5f },
                                       { "delayMix", 0.35f } } },

        // ---- Drive: the distortion before the filter (Pre) and after the reverb (Post)
        { "Tape Blip", "Drive", { { "bleepPitchAmount", 7 }, { "bleepPitchDecay", 30 }, { "bleepDecay", 220 }, { "driveType", Warm }, { "drivePost", Pre },
                                  { "driveAmount", 0.45f }, { "driveMix", 0.7f }, { "filterType", LP }, { "filterCutoff", 4000 } } },
        { "Fold Clang", "Drive", { Machine, { "machineType", Hit }, { "machineLength", 800 }, { "machineTone", 0.5f }, { "machineFocus", 0.5f },
                                   { "machineMetal", 0.8f }, { "driveType", Fold }, { "drivePost", Pre }, { "driveAmount", 0.55f }, { "driveMix", 0.8f },
                                   { "filterType", BP }, { "filterCutoff", 2500 }, { "filterResonance", 0.4f }, { "chaosAmount", 0.4f }, { "chaosPitch", 0.4f } } },
        { "Fuzz Grind", "Drive", { Machine, { "machineType", Grind }, { "machineLength", 600 }, { "machineTone", 0.6f }, { "machineFocus", 0.7f },
                                   { "machineMotion", 0.5f }, { "machineMetal", 0.7f }, { "driveType", Hard }, { "drivePost", Pre }, { "driveAmount", 0.6f },
                                   { "driveMix", 0.65f }, { "filterType", LP }, { "filterCutoff", 3500 } } },
        // the echoes and the room go through the distortion too, and fold into a dense wall
        { "Industrial Wall", "Drive", { Machine, { "machineType", Grind }, { "machineLength", 900 }, { "machineTone", 0.45f }, { "machineFocus", 0.6f },
                                        { "machineMotion", 0.4f }, { "machineMetal", 0.75f }, { "delayNote", N8 }, { "delayFeel", Dotted },
                                        { "delayFeedback", 0.55f }, { "delayMix", 0.4f }, { "roomSize", 0.7f }, { "roomDecay", 1.8f }, { "roomMix", 0.4f },
                                        { "driveType", Hard }, { "drivePost", Post }, { "driveAmount", 0.75f }, { "driveMix", 0.7f } } },
        { "Rusted Reverb", "Drive", { Machine, { "machineType", Hit }, { "machineLength", 500 }, { "machineTone", 0.4f }, { "machineFocus", 0.3f },
                                      { "machineMetal", 0.85f }, { "roomSize", 0.8f }, { "roomDecay", 2.5f }, { "roomTone", 0.6f }, { "roomMix", 0.5f },
                                      { "driveType", Fold }, { "drivePost", Post }, { "driveAmount", 0.5f }, { "driveMix", 0.55f } } },
        { "Screech Fold", "Drive", { Machine, { "machineType", Screech }, { "machineLength", 900 }, { "machineTone", 0.6f }, { "machineFocus", 0.6f },
                                     { "machineMotion", 0.45f }, { "machineMetal", 0.8f }, { "driveType", Fold }, { "drivePost", Pre }, { "driveAmount", 0.5f },
                                     { "driveMix", 0.75f }, { "chaosAmount", 0.3f }, { "chaosPitch", 0.3f } } },
        { "Scream Wall", "Drive", { Machine, { "machineType", Screech }, { "machineLength", 1400 }, { "machineTone", 0.7f }, { "machineFocus", 0.5f },
                                    { "machineMotion", 0.5f }, { "machineMetal", 0.7f }, { "delayNote", N8 }, { "delayFeel", Dotted }, { "delayFeedback", 0.5f },
                                    { "delayMix", 0.35f }, { "roomSize", 0.8f }, { "roomDecay", 2.0f }, { "roomMix", 0.4f },
                                    { "driveType", Hard }, { "drivePost", Post }, { "driveAmount", 0.7f }, { "driveMix", 0.65f } } },
        { "Folded Squeal", "Drive", { Machine, { "machineType", Screech }, { "machineLength", 700 }, { "machineTone", 0.5f }, { "machineFocus", 0.8f },
                                      { "machineMotion", 0.3f }, { "machineMetal", 0.9f }, { "roomSize", 0.6f }, { "roomDecay", 1.5f }, { "roomMix", 0.35f },
                                      { "driveType", Fold }, { "drivePost", Post }, { "driveAmount", 0.6f }, { "driveMix", 0.6f } } },
    };
    return presets;
}
} // namespace bleepforge

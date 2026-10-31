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
inline constexpr float Knock = 0, Noise = 1, Crackle = 2, Steam = 3, Friction = 4; // exciters
inline constexpr float Wood = 0, Metal = 1, Membrane = 2;                           // bodies
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

        // ---- Pulses
        { "Square Bleep", "Pulses", { { "bleepWave", Pulse }, { "bleepPitchAmount", 0 }, { "bleepDecay", 150 },
                                      { "filterType", LP }, { "filterCutoff", 4000 } } },
        { "Retro Beep", "Pulses", { { "bleepWave", Pulse }, { "bleepPitchAmount", 0 }, { "bleepDecay", 300 }, { "bleepRelease", 40 } } },
        { "Narrow Pulse", "Pulses", { { "bleepWave", Pulse }, { "bleepPulseWidth", 0.12f }, { "bleepDecay", 120 } } },
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
        { "Decimated Wood", "Lo-fi", { Strike, { "crushBits", 8 }, { "crushRate", 4000 } } },
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
        { "Dub Bell", "Dub", { { "bleepWave", FM }, { "bleepFmRatio", 3.5f }, { "bleepFmAmount", 0.4f }, { "bleepPitchAmount", 0 },
                               { "bleepDecay", 700 }, { "delayNote", N4 }, { "delayFeel", Triplet }, { "delayFeedback", 0.55f }, { "delayMix", 0.35f },
                               { "roomDecay", 0.7f }, { "roomMix", 0.3f } } },
        { "Echo Plate", "Dub", { Strike, { "strikeBody", Membrane }, { "strikeRing", 0.57f }, { "strikeTone", 0.6f }, { "delayNote", 2 }, { "delayFeel", 0 }, { "delayFeedback", 0.5f }, { "delayMix", 0.35f } } },
        { "Long Echo", "Dub", { { "bleepPitchAmount", 0 }, { "bleepDecay", 150 }, { "filterType", LP }, { "filterCutoff", 1800 },
                                { "delayNote", N8 }, { "delayFeel", Dotted }, { "delayFeedback", 0.65f }, { "delayMix", 0.45f } } },
        { "Slapback Blip", "Dub", { { "bleepDecay", 100 }, { "delayNote", N32 }, { "delayFeel", Straight }, { "delayFeedback", 0.2f }, { "delayMix", 0.35f } } },
        { "Chord Echo", "Dub", { { "bleepPitchAmount", 0 }, { "bleepDecay", 300 }, { "filterType", BP },
                                 { "filterCutoff", 900 }, { "filterResonance", 0.35f }, { "delayNote", N8 }, { "delayFeel", Dotted }, { "delayFeedback", 0.7f },
                                 { "delayMix", 0.5f }, { "roomSize", 0.8f }, { "roomDecay", 1.2f }, { "roomMix", 0.35f } } },

        // ---- Wood
        { "Wood Knock", "Wood", { Strike } },
        { "Wood Block", "Wood", { Strike, { "strikeRing", 0.63f }, { "strikeTone", 0.65f } } },
        { "Log Drum", "Wood", { Strike, { "strikeRing", 0.78f }, { "strikeTone", 0.35f } } },
        { "Claves", "Wood", { Strike, { "strikeRing", 0.72f }, { "strikeTone", 0.8f } } },
        { "Hollow Wood", "Wood", { Strike, { "strikeRing", 0.84f }, { "strikeTone", 0.4f }, { "roomDecay", 0.7f }, { "roomMix", 0.25f } } },
        { "Dead Knock", "Wood", { Strike, { "strikeRing", 0.1f } } },
        { "Soft Tap", "Wood", { Strike, { "strikeRing", 0.4f }, { "strikeTone", 0.2f } } },
        { "Bar Mallet", "Wood", { Strike, { "strikeRing", 0.93f }, { "strikeTone", 0.45f } } },

        { "Stick Click", "Wood", { Strike, { "strikeRing", 0.39f }, { "strikeTone", 1 } } },
        { "Wood Chaos", "Wood", { Strike, { "strikeRing", 0.46f }, { "strikeTone", 0.6f }, { "chaosAmount", 0.8f }, { "chaosPitch", 0.5f }, { "chaosTone", 0.8f }, { "chaosPan", 0.8f } } },

        // ---- Drums: struck skins and plates (a membrane body)
        { "Metal Tink", "Drums", { Strike, { "strikeBody", Membrane }, { "strikeRing", 0.62f }, { "strikeTone", 0.8f }, { "roomDecay", 0.7f }, { "roomMix", 0.2f } } },
        { "Pipe Clank", "Drums", { Strike, { "strikeBody", Membrane }, { "strikeRing", 0.61f } } },
        { "Bell Plate", "Drums", { Strike, { "strikeBody", Membrane }, { "strikeRing", 0.87f }, { "strikeTone", 0.6f }, { "roomDecay", 0.7f }, { "roomMix", 0.25f } } },
        { "Spoon Hit", "Drums", { Strike, { "strikeBody", Membrane }, { "strikeRing", 0.34f }, { "strikeTone", 0.9f } } },
        { "Gong Tap", "Drums", { Strike, { "strikeBody", Membrane }, { "strikeRing", 0.94f }, { "strikeTone", 0.35f }, { "roomSize", 0.8f }, { "roomDecay", 1.2f }, { "roomMix", 0.3f } } },
        { "Tin Can", "Drums", { Strike, { "strikeExciter", Noise }, { "strikeBody", Membrane }, { "strikeLength", 1 }, { "strikeRing", 0.3f }, { "strikeTone", 0.7f } } },
        { "Steel Pan", "Drums", { Strike, { "strikeBody", Membrane }, { "strikeRing", 0.65f }, { "strikeTone", 0.45f } } },
        { "Chain Rattle", "Drums", { Strike, { "strikeExciter", Crackle }, { "strikeBody", Membrane }, { "strikeLength", 60 }, { "strikeRing", 0.27f }, { "strikeTone", 0.8f }, { "chaosAmount", 0.6f } } },
        { "Metal Cowbell", "Drums", { Strike, { "strikeBody", Membrane }, { "strikeRing", 0.52f }, { "strikeTone", 0.65f }, { "filterType", 2 }, { "filterCutoff", 900 }, { "filterResonance", 0.3f } } },
        { "Anvil", "Drums", { Strike, { "strikeBody", Membrane }, { "strikeRing", 0.8f }, { "strikeTone", 1 } } },
        { "Muted Metal", "Drums", { Strike, { "strikeBody", Membrane }, { "strikeRing", 0.17f }, { "strikeTone", 0.55f } } },
        { "Metal Chaos", "Drums", { Strike, { "strikeBody", Membrane }, { "strikeRing", 0.54f }, { "strikeTone", 0.75f }, { "chaosAmount", 0.9f }, { "chaosPitch", 0.6f }, { "chaosDecay", 0.8f }, { "chaosPan", 0.9f } } },
        { "Steel Beam", "Drums", { Strike, { "strikeBody", Membrane }, { "strikeRing", 0.75f }, { "strikeTone", 0.55f }, { "roomSize", 0.7f }, { "roomDecay", 1.1f }, { "roomMix", 0.25f } } },
        { "Rivet Gun", "Drums", { Strike, { "strikeExciter", Crackle }, { "strikeBody", Membrane }, { "strikeLength", 25 }, { "strikeRing", 0.36f }, { "strikeTone", 0.85f }, { "crushBits", 8 } } },
        { "Press Stamp", "Drums", { Strike, { "strikeExciter", Noise }, { "strikeBody", Membrane }, { "strikeLength", 1.5f }, { "strikeRing", 0.35f }, { "strikeTone", 0.4f } } },

        // ---- Industrial, steam and friction: struck metal parts, and steady sources, tuned to the note
        { "Press Clonk", "Industrial", { Strike, { "strikeBody", Metal }, { "strikeRing", 0.82f }, { "strikeTone", 0.45f }, { "chaosAmount", 0.5f }, { "chaosPitch", 0.6f }, { "roomDecay", 0.7f }, { "roomMix", 0.2f } } },
        { "Heavy Hit", "Industrial", { Strike, { "strikeBody", Metal }, { "strikeRing", 0.74f }, { "strikeTone", 0.3f }, { "chaosAmount", 0.4f },
                                       { "driveType", Warm }, { "drivePost", Pre }, { "driveAmount", 0.6f }, { "driveMix", 0.7f },
                                       { "roomSize", 0.7f }, { "roomDecay", 1.4f }, { "roomMix", 0.25f } } },
        { "Rattling Plate", "Industrial", { Strike, { "strikeBody", Metal }, { "strikeRing", 0.9f }, { "strikeTone", 0.65f }, { "strikeMotion", 0.55f }, { "chaosAmount", 0.6f }, { "chaosPitch", 0.7f } } },
        { "Donk", "Industrial", { Strike, { "strikeBody", Metal }, { "strikeRing", 0.69f }, { "roomSize", 1 }, { "roomDecay", 4 }, { "roomMix", 0.12f }, { "roomDuck", 0.12f } } },
        { "Steam Valve", "Air & Friction", { Strike, { "strikeExciter", Steam }, { "strikeBody", Metal }, { "strikeRing", 0.15f }, { "strikeTone", 0.75f } } },
        { "Pressure Leak", "Air & Friction", { Strike, { "strikeExciter", Steam }, { "strikeBody", Metal }, { "strikeLength", 1200 }, { "strikeRing", 0.25f }, { "strikeTone", 0.85f }, { "strikeMotion", 0.35f }, { "roomDecay", 0.7f }, { "roomMix", 0.2f } } },
        { "Whistle Pipe", "Air & Friction", { Strike, { "strikeExciter", Steam }, { "strikeBody", Metal }, { "strikeLength", 600 }, { "strikeRing", 0.4f }, { "strikeTone", 0.25f } } },
        { "Air Brake", "Air & Friction", { Strike, { "strikeExciter", Steam }, { "strikeBody", Metal }, { "strikeLength", 180 }, { "strikeRing", 0.1f }, { "strikeTone", 0.9f }, { "chaosAmount", 0.4f }, { "chaosTone", 0.6f } } },

        { "Rail Squeal", "Air & Friction", { Strike, { "strikeExciter", Friction }, { "strikeBody", Metal }, { "strikeLength", 1500 }, { "strikeRing", 0.6f }, { "strikeTone", 0.75f }, { "strikeMotion", 0.2f }, { "roomDecay", 0.7f }, { "roomMix", 0.2f } } },
        { "Brake Squeal", "Air & Friction", { Strike, { "strikeExciter", Friction }, { "strikeBody", Metal }, { "strikeLength", 1200 }, { "strikeRing", 0.4f }, { "strikeTone", 0.85f }, { "strikeMotion", 0.75f }, { "chaosAmount", 0.3f }, { "chaosPitch", 0.3f } } },
        { "Metal Scrape", "Air & Friction", { Strike, { "strikeExciter", Friction }, { "strikeBody", Metal }, { "strikeLength", 600 }, { "strikeTone", 0.45f }, { "strikeMotion", 0.5f } } },

        // ---- Drips and water: the sweep slides each hit into its note
        { "Drip", "Drips", { Strike, { "strikeBody", Membrane }, { "strikeRing", 0.23f }, { "strikeSweep", 12 } } },
        { "Water Drop", "Drips", { Strike, { "strikeBody", Membrane }, { "strikeRing", 0.38f }, { "strikeTone", 0.4f }, { "strikeSweep", 19 }, { "delayMix", 0.2f } } },

        { "Bloop", "Drips", { Strike, { "strikeRing", 0.42f }, { "strikeTone", 0.3f }, { "strikeSweep", 24 } } },
        { "Cave Drip", "Drips", { Strike, { "strikeBody", Membrane }, { "strikeRing", 0.46f }, { "strikeSweep", 12 }, { "delayNote", 3 }, { "delayFeel", 0 }, { "delayFeedback", 0.4f }, { "delayMix", 0.2f }, { "roomSize", 0.9f }, { "roomDecay", 1.4f }, { "roomMix", 0.45f } } },
        { "Falling Drop", "Drips", { Strike, { "strikeBody", Membrane }, { "strikeRing", 0.37f } } },

        { "Rain Drops", "Drips", { Strike, { "strikeExciter", Crackle }, { "strikeBody", Membrane }, { "strikeLength", 30 }, { "strikeRing", 0.2f }, { "strikeSweep", 7 }, { "chaosAmount", 0.8f } } },
        { "Sine Drip", "Drips", { { "bleepPitchAmount", -19 }, { "bleepPitchDecay", 18 }, { "bleepDecay", 60 }, { "roomDecay", 0.7f }, { "roomMix", 0.2f } } },

        // ---- Textures: crackles and scrapes
        { "Crackle", "Textures", { Strike, { "strikeExciter", Crackle }, { "strikeLength", 40 }, { "strikeRing", 0.38f }, { "strikeTone", 0.8f }, { "chaosAmount", 0.7f }, { "chaosPan", 0.8f } } },
        { "Vinyl Dust", "Textures", { Strike, { "strikeExciter", Crackle }, { "strikeLength", 60 }, { "strikeRing", 0.43f }, { "strikeTone", 1 }, { "crushBits", 10 }, { "chaosAmount", 0.5f } } },
        { "Static Burst", "Textures", { Strike, { "strikeExciter", Crackle }, { "strikeBody", Membrane }, { "strikeLength", 80 }, { "strikeRing", 0.07f }, { "strikeTone", 1 }, { "crushBits", 6 } } },
        { "Gravel", "Textures", { Strike, { "strikeExciter", Crackle }, { "strikeLength", 60 }, { "strikeRing", 0.42f }, { "strikeTone", 0.7f } } },

        { "Embers", "Textures", { Strike, { "strikeExciter", Crackle }, { "strikeBody", Membrane }, { "strikeLength", 200 }, { "strikeRing", 0.11f }, { "strikeTone", 0.4f }, { "roomDecay", 0.7f }, { "roomMix", 0.35f }, { "chaosAmount", 0.8f } } },
        { "Paper Crunch", "Textures", { Strike, { "strikeExciter", Noise }, { "strikeLength", 12 }, { "strikeRing", 0.43f }, { "strikeTone", 0.9f } } },
        { "Scrape", "Textures", { Strike, { "strikeExciter", Noise }, { "strikeBody", Membrane }, { "strikeLength", 80 }, { "strikeRing", 0.24f }, { "strikeTone", 0.6f }, { "strikeSweep", 5 } } },
        { "Rattle Echo", "Textures", { Strike, { "strikeExciter", Crackle }, { "strikeBody", Membrane }, { "strikeLength", 50 }, { "strikeRing", 0.18f }, { "strikeTone", 0.7f }, { "delayNote", 1 }, { "delayFeel", 2 }, { "delayFeedback", 0.5f }, { "delayMix", 0.35f } } },

        // ---- Drive: the distortion before the filter (Pre) and after the reverb (Post)
        { "Tape Blip", "Drive", { { "bleepPitchAmount", 7 }, { "bleepPitchDecay", 30 }, { "bleepDecay", 220 }, { "driveType", Warm }, { "drivePost", Pre },
                                  { "driveAmount", 0.45f }, { "driveMix", 0.7f }, { "filterType", LP }, { "filterCutoff", 4000 } } },
        { "Fold Clang", "Drive", { Strike, { "strikeBody", Metal }, { "strikeRing", 0.85f }, { "driveType", 2 }, { "drivePost", 0 }, { "driveAmount", 0.55f }, { "driveMix", 0.8f }, { "filterType", 2 }, { "filterCutoff", 2500 }, { "filterResonance", 0.4f }, { "chaosAmount", 0.4f }, { "chaosPitch", 0.4f } } },
        { "Fuzz Grind", "Drive", { Strike, { "strikeExciter", Steam }, { "strikeBody", Metal }, { "strikeLength", 600 }, { "strikeRing", 0.7f }, { "strikeTone", 0.6f }, { "strikeMotion", 0.5f }, { "driveType", 1 }, { "drivePost", 0 }, { "driveAmount", 0.6f }, { "driveMix", 0.65f }, { "filterType", 1 }, { "filterCutoff", 3500 } } },
        // the echoes and the room go through the distortion too, and fold into a dense wall
        { "Industrial Wall", "Drive", { Strike, { "strikeExciter", Steam }, { "strikeBody", Metal }, { "strikeLength", 900 }, { "strikeRing", 0.75f }, { "strikeTone", 0.45f }, { "strikeMotion", 0.4f }, { "delayNote", 2 }, { "delayFeel", 2 }, { "delayFeedback", 0.55f }, { "delayMix", 0.4f }, { "roomSize", 0.7f }, { "roomDecay", 1.8f }, { "roomMix", 0.4f }, { "driveType", 1 }, { "drivePost", 1 }, { "driveAmount", 0.75f }, { "driveMix", 0.7f } } },
        { "Rusted Reverb", "Drive", { Strike, { "strikeBody", Metal }, { "strikeRing", 0.87f }, { "strikeTone", 0.4f }, { "roomSize", 0.8f }, { "roomDecay", 2.5f }, { "roomTone", 0.6f }, { "roomMix", 0.5f }, { "driveType", 2 }, { "drivePost", 1 }, { "driveAmount", 0.5f }, { "driveMix", 0.55f } } },
        { "Screech Fold", "Drive", { Strike, { "strikeExciter", Friction }, { "strikeBody", Metal }, { "strikeLength", 900 }, { "strikeRing", 0.8f }, { "strikeTone", 0.6f }, { "strikeMotion", 0.45f }, { "driveType", 2 }, { "drivePost", 0 }, { "driveAmount", 0.5f }, { "driveMix", 0.75f }, { "chaosAmount", 0.3f }, { "chaosPitch", 0.3f } } },
        { "Scream Wall", "Drive", { Strike, { "strikeExciter", Friction }, { "strikeBody", Metal }, { "strikeLength", 1400 }, { "strikeRing", 0.7f }, { "strikeTone", 0.7f }, { "strikeMotion", 0.5f }, { "delayNote", 2 }, { "delayFeel", 2 }, { "delayFeedback", 0.5f }, { "delayMix", 0.35f }, { "roomSize", 0.8f }, { "roomDecay", 2 }, { "roomMix", 0.4f }, { "driveType", 1 }, { "drivePost", 1 }, { "driveAmount", 0.7f }, { "driveMix", 0.65f } } },
        { "Folded Squeal", "Drive", { Strike, { "strikeExciter", Friction }, { "strikeBody", Metal }, { "strikeLength", 700 }, { "strikeRing", 0.9f }, { "strikeMotion", 0.3f }, { "roomSize", 0.6f }, { "roomDecay", 1.5f }, { "roomMix", 0.35f }, { "driveType", 2 }, { "drivePost", 1 }, { "driveAmount", 0.6f }, { "driveMix", 0.6f } } },
    };
    return presets;
}
} // namespace bleepforge

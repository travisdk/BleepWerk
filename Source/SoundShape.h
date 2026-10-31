// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "BleepSynth.h"
#include "FoleySynth.h"
#include "MachineSynth.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

// What the small picture of the sound in the engine panel draws, worked out from the parameters
// the way the engines use them (the formulas follow BleepVoice, FoleyVoice and MachineVoice, with
// Chaos and velocity left out): the Bleep's amplitude and pitch over time, and for Foley and
// Machine the tones of the resonator, each with its strength and how long it rings. JUCE-free.
namespace bleepforge
{
// The amplitude envelope of a Bleep, 0..1: a linear attack and then a decay that reaches
// silence (-80 dB) after the Decay time.
inline float bleepAmplitudeAt (const BleepParams& p, float ms)
{
    const float attack = std::max (1.0f, p.attackMs);
    if (ms <= 0.0f)
        return 0.0f;
    if (ms < attack)
        return ms / attack;
    return std::exp (-9.21f * (ms - attack) / std::max (1.0f, p.decayMs));
}

// The pitch offset in semitones: it starts at Pitch Env and falls 40 dB in the P Decay time.
inline float bleepPitchAt (const BleepParams& p, float ms)
{
    return p.pitchEnvSemis * std::exp (-4.6f * std::max (0.0f, ms) / std::max (1.0f, p.pitchEnvDecayMs));
}

// How far along the time axis the picture runs: until the sound is gone, but not so short
// that a click can't be seen.
inline float bleepShapeEndMs (const BleepParams& p)
{
    return std::max (20.0f, std::max (1.0f, p.attackMs) + p.decayMs);
}

// One tone of a resonator: its frequency relative to the note, its strength (the strongest is
// 1) and how long it rings.
struct ShapeMode
{
    float ratio, strength, ringSeconds;
};

inline std::vector<ShapeMode> foleyShape (const FoleyParams& p)
{
    const auto material = blendMaterial (p.material);
    const float size = std::clamp (p.size, 0.0f, 1.0f);
    const float baseT60 = std::exp (std::log (3.0f) + std::clamp (p.damping, 0.0f, 1.0f) * (std::log (0.02f) - std::log (3.0f)))
                        * material.decayScale * std::exp2 (2.0f * size - 1.0f);
    const float tilt = 1.6f - 1.3f * std::clamp (p.brightness, 0.0f, 1.0f);

    std::vector<ShapeMode> modes;
    for (size_t k = 0; k < (size_t) kNumModes; ++k)
        modes.push_back ({ material.ratios[k], std::pow ((float) k + 1.0f, -tilt),
                           std::max (0.005f, baseT60 * std::pow (material.ratios[k], -material.highModeDamping)) });
    return modes;
}

// Machine: the resonator's eight tones. Screech is a bright tone with its own partials above it,
// whose strengths follow Tone.
inline std::vector<ShapeMode> machineShape (const MachineParams& p)
{
    const float tone = std::clamp (p.tone, 0.0f, 1.0f);
    std::vector<ShapeMode> modes;
    if (p.type == MachineType::Screech)
    {
        constexpr std::array<float, 7> partials { 1.0f, 0.5f, 0.35f, 0.32f, 0.15f, 0.24f, 0.2f }; // as in MachineVoice
        for (size_t k = 0; k < partials.size(); ++k)
            modes.push_back ({ (float) k + 1.0f, partials[k] * std::pow ((float) k + 1.0f, -2.0f * (1.0f - tone) * (1.0f - tone)), 0.0f });
    }
    else
    {
        const float ring = machineRingSeconds (p.metal);
        const float tilt = 1.6f - 1.3f * tone;
        for (size_t k = 0; k < kMachineModeRatios.size(); ++k)
            modes.push_back ({ kMachineModeRatios[k], std::pow ((float) k + 1.0f, -tilt),
                               std::max (0.005f, ring * std::pow (kMachineModeRatios[k], -0.7f)) });
    }
    float strongest = 0.0f;
    for (const auto& m : modes)
        strongest = std::max (strongest, m.strength);
    for (auto& m : modes)
        m.strength /= std::max (1.0e-6f, strongest);
    return modes;
}

// How loud a resonator is `seconds` after the hit, 0..1: the tones added up, each dying away
// over its own ring time (-60 dB after it).
inline float ringLevelAt (const std::vector<ShapeMode>& modes, float seconds)
{
    float sum = 0.0f, start = 0.0f;
    for (const auto& m : modes)
    {
        start += m.strength;
        sum += m.strength * std::exp (-6.9f * std::max (0.0f, seconds) / std::max (0.005f, m.ringSeconds));
    }
    return start > 0.0f ? sum / start : 0.0f;
}

// The end of the time axis: about where the longest tone has died to 1 %, at least 50 ms.
inline float ringEndSeconds (const std::vector<ShapeMode>& modes)
{
    float end = 0.05f;
    for (const auto& m : modes)
        end = std::max (end, std::min (10.0f, m.ringSeconds * 0.67f));
    return end;
}
} // namespace bleepforge

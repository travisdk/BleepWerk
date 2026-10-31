// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "BleepSynth.h"
#include "StrikeSynth.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

// What the small picture of the sound in the engine panel draws, worked out from the parameters
// the way the engines use them (the formulas follow BleepVoice and StrikeVoice, with Chaos and
// velocity left out): the Bleep's amplitude and pitch over time, and for Strike the tones of
// the body, each with its strength and how long it rings. JUCE-free.
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

// Strike: the body's eight tones, with the ring time of each. Friction is a bright tone with its
// own partials above it, whose strengths follow Tone, and no ring.
inline std::vector<ShapeMode> strikeShape (const StrikeParams& p)
{
    const float tone = std::clamp (p.tone, 0.0f, 1.0f);
    std::vector<ShapeMode> modes;
    if (p.exciter == StrikeExciter::Friction)
    {
        for (size_t k = 0; k < StrikeVoice::kScreechPartials.size(); ++k)
            modes.push_back ({ (float) k + 1.0f, StrikeVoice::kScreechPartials[k] * std::pow ((float) k + 1.0f, -2.0f * (1.0f - tone) * (1.0f - tone)), 0.0f });
    }
    else
    {
        const auto& body = bodyModel (p.body);
        const float baseT60 = strikeRingSeconds (body, p.ring);
        const float tilt = 1.6f - 1.3f * tone;
        for (size_t k = 0; k < body.ratios.size(); ++k)
            modes.push_back ({ body.ratios[k], std::pow ((float) k + 1.0f, -tilt),
                               std::max (0.005f, baseT60 * std::pow (body.ratios[k], -body.highModeDamping)) });
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

// Steam and Friction: the level over time, 0..1: a fade in, a plateau to half of Length, then a
// fade that reaches -80 dB at Length (the same as the voice).
inline float steadyLevelAt (const StrikeParams& p, float ms)
{
    const float length = std::clamp (p.lengthMs, 5.0f, 6000.0f);
    const float attack = p.exciter == StrikeExciter::Friction ? std::clamp (0.15f * length, 10.0f, 80.0f)
                                                              : std::clamp (0.1f * length, 1.0f, 20.0f);
    if (ms <= 0.0f)
        return 0.0f;
    if (ms < attack)
        return ms / attack;
    return ms < 0.5f * length ? 1.0f : std::exp (-9.21f * (ms - 0.5f * length) / (0.5f * length));
}
} // namespace bleepforge

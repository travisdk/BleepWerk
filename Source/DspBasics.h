// Copyright (C) 2026 Henrik Juhl. Part of BleepWerk, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

// Small DSP building blocks shared by the engines. JUCE-free so the unit tests can use them.
// The band-limiting helpers and the envelope come from SubForge (same author, same licence).
namespace bleepwerk
{
inline constexpr float kPi = 3.14159265358979f;
inline constexpr float kTwoPi = 2.0f * kPi;

// Band-limited step (PolyBLEP), derived in SubForge's docs/LICENSE-REVIEW.md section 5.2:
//
// The ideal unit step H(x) is replaced by B(x), the integral of a triangular
// kernel k(s) = 1 - |s| reaching one sample to each side (x in samples, the
// discontinuity at x = 0):
//     B(x) = (x + 1)^2 / 2          for -1 <= x < 0
//     B(x) = 1 - (1 - x)^2 / 2      for  0 <= x < 1
// The correction added to a naive waveform is the residual r(x) = B(x) - H(x),
// scaled by the height of the jump:
//     r(x) =  (x + 1)^2 / 2         for -1 <= x < 0   (just before the jump)
//     r(x) = -(1 - x)^2 / 2         for  0 <= x < 1   (just after it)
//     r(x) =  0                     elsewhere
inline float stepResidual (float x)
{
    if (x <= -1.0f || x >= 1.0f)
        return 0.0f;
    if (x < 0.0f)
        return 0.5f * (x + 1.0f) * (x + 1.0f);
    return -0.5f * (1.0f - x) * (1.0f - x);
}

// Residual for a jump where an oscillator's phase (0..1, advancing dt per
// sample) wraps from 1 to 0. Phase distance divided by dt is sample distance.
inline float stepResidualAtWrap (float phase, float dt)
{
    if (phase < dt)
        return stepResidual (phase / dt);
    if (phase > 1.0f - dt)
        return stepResidual ((phase - 1.0f) / dt);
    return 0.0f;
}

inline float wrapPhase (float phase)
{
    return phase - std::floor (phase);
}

// Attack, exponential decay to the sustain level, exponential release. With sustain 0 it is
// a percussive AD envelope that ends by itself, which is what most BleepWerk sounds use.
struct Envelope
{
    enum class Stage { Idle, Attack, Decay, Release };

    static constexpr float kSilence = 1.0e-4f; // -80 dB: the voice is done

    Stage stage = Stage::Idle;
    float value = 0.0f;
    float attackInc = 1.0f, decayCoef = 0.0f, sustain = 0.0f, releaseCoef = 0.0f;

    static float coefFor (float ms, float fs)
    {
        // reaches kSilence after the given time, so a decay or release of n ms is silent after n ms
        return std::exp (-9.21f / std::max (1.0f, ms * 0.001f * fs));
    }

    void set (float fs, float attackMs, float decayMs, float sustainLevel, float releaseMs)
    {
        attackInc = 1.0f / std::max (1.0f, attackMs * 0.001f * fs);
        decayCoef = coefFor (decayMs, fs);
        sustain = sustainLevel;
        releaseCoef = coefFor (releaseMs, fs);
    }

    void noteOn() { stage = Stage::Attack; } // attacks from the current value, no click
    void noteOff() { if (stage != Stage::Idle) stage = Stage::Release; }
    void reset() { stage = Stage::Idle; value = 0.0f; }
    bool isActive() const { return stage != Stage::Idle; }
    bool isReleasing() const { return stage == Stage::Release; }

    float next()
    {
        switch (stage)
        {
            case Stage::Attack:
                value += attackInc;
                if (value >= 1.0f)
                {
                    value = 1.0f;
                    stage = Stage::Decay;
                }
                break;
            case Stage::Decay:
                value = sustain + (value - sustain) * decayCoef;
                if (sustain <= 0.0f && value < kSilence)
                    reset();
                break;
            case Stage::Release:
                value *= releaseCoef;
                if (value < kSilence)
                    reset();
                break;
            case Stage::Idle:
                break;
        }
        return value;
    }
};

// xorshift32 white noise in -1..1: allocation-free and cheap, for the audio thread.
struct WhiteNoise
{
    uint32_t state = 0x9e3779b9u;

    void seed (uint32_t s) { state = s != 0 ? s : 0x9e3779b9u; }

    float next()
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return (float) (int32_t) state * (1.0f / 2147483648.0f);
    }
};
} // namespace bleepwerk

// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

// The effects: at their defaults the chain passes the signal through untouched, the filter
// types cut where they should, Bits and Rate crush, the delay repeats on the beat and its
// repeats fade, and the room rings out and falls silent. Nothing ever goes non-finite.
#include "Effects.h"
#include "TestUtils.h"

#include <cstdio>
#include <vector>

using namespace bleepforge;

constexpr double fs = 48000.0;

static std::vector<float> sine (double hz, int numSamples, float level = 0.5f)
{
    std::vector<float> v ((size_t) numSamples);
    for (int i = 0; i < numSamples; ++i)
        v[(size_t) i] = level * (float) std::sin (2.0 * 3.14159265358979 * hz * i / fs);
    return v;
}

static double rms (const std::vector<float>& v, int from, int to)
{
    double sum = 0.0;
    for (int i = from; i < to; ++i)
        sum += (double) v[(size_t) i] * v[(size_t) i];
    return std::sqrt (sum / std::max (1, to - from));
}

static bool allFinite (const std::vector<float>& v)
{
    for (float x : v)
        if (! std::isfinite (x))
            return false;
    return true;
}

// Runs a mono signal through the chain in blocks, the way the plug-in does.
static std::vector<float> run (const EffectParams& p, std::vector<float> signal, double bpm = 120.0)
{
    Effects fx;
    fx.prepare (fs);
    for (size_t pos = 0; pos < signal.size(); pos += 256)
    {
        fx.setParams (p, bpm);
        fx.process (signal.data() + pos, nullptr, (int) std::min<size_t> (256, signal.size() - pos));
    }
    return signal;
}

static std::vector<float> impulse (int numSamples)
{
    std::vector<float> v ((size_t) numSamples, 0.0f);
    v[0] = 1.0f;
    return v;
}

static void testDefaultsPassThrough()
{
    const auto in = sine (440.0, 4800);
    const auto out = run ({}, in);
    float maxDiff = 0.0f;
    for (size_t i = 0; i < in.size(); ++i)
        maxDiff = std::max (maxDiff, std::abs (out[i] - in[i]));
    CHECK (maxDiff == 0.0f);
}

static void testFilter()
{
    EffectParams p;
    p.cutoffHz = 1000.0f;
    p.resonance = 0.0f;
    const int n = 9600, from = 4800; // skip the start-up

    p.filterType = FilterType::LowPass;
    CHECK (rms (run (p, sine (100.0, n)), from, n) > 0.3);
    CHECK (rms (run (p, sine (8000.0, n)), from, n) < 0.02);

    p.filterType = FilterType::HighPass;
    CHECK (rms (run (p, sine (100.0, n)), from, n) < 0.01);
    CHECK (rms (run (p, sine (8000.0, n)), from, n) > 0.3);

    p.filterType = FilterType::BandPass;
    const double centre = rms (run (p, sine (1000.0, n)), from, n);
    CHECK (centre > 0.3); // unity at the centre
    CHECK (rms (run (p, sine (100.0, n)), from, n) < centre * 0.25);
    CHECK (rms (run (p, sine (10000.0, n)), from, n) < centre * 0.25);

    // full resonance peaks at the cutoff but stays finite and bounded
    p.filterType = FilterType::LowPass;
    p.resonance = 1.0f;
    const auto ringing = run (p, sine (1000.0, n));
    CHECK (allFinite (ringing));
    CHECK (rms (ringing, from, n) > 0.5);
    CHECK (rms (ringing, from, n) < 20.0);
}

static void testCrusher()
{
    EffectParams p;
    p.crushBits = 2.0f; // 2 bits: steps of 0.5
    const auto crushed = run (p, sine (440.0, 4800, 0.9f));
    bool onSteps = true;
    for (float x : crushed)
        onSteps = onSteps && std::abs (x * 2.0f - std::round (x * 2.0f)) < 1.0e-6f;
    CHECK (onSteps);

    // Rate holds each sample for fs / rate samples
    p = {};
    p.crushRateHz = 4800.0f;
    const auto held = run (p, sine (440.0, 4800));
    int changes = 0;
    for (size_t i = 1; i < held.size(); ++i)
        changes += held[i] != held[i - 1] ? 1 : 0;
    CHECK (changes >= 470 && changes <= 481); // 480 holds in 100 ms
}

static void testDelay()
{
    EffectParams p;
    p.delayTime = 2; // 1/16 at 120 BPM: 125 ms
    p.delayMix = 1.0f;
    p.delayFeedback = 0.5f;
    const int echo = (int) (0.125 * fs);
    const auto out = run (p, impulse (echo * 4 + 100));

    auto peakAround = [&out] (int at)
    {
        float peak = 0.0f;
        for (int i = at - 4; i <= at + 4; ++i)
            peak = std::max (peak, std::abs (out[(size_t) i]));
        return peak;
    };
    CHECK (std::abs (out[0]) < 1.0e-6f);  // fully wet: no dry impulse
    CHECK (peakAround (echo) > 0.5f);
    CHECK (rms (out, 10, echo - 10) < 1.0e-6); // silent before the first repeat
    CHECK (peakAround (echo * 2) < peakAround (echo) * 0.6f); // repeats fade with feedback
    CHECK (peakAround (echo * 2) > 0.05f);

    // the time follows the tempo
    const auto slow = run (p, impulse (echo * 3), 60.0);
    float early = 0.0f;
    for (int i = echo - 4; i <= echo + 4; ++i)
        early = std::max (early, std::abs (slow[(size_t) i]));
    CHECK (early < 1.0e-3f);
    CHECK (std::abs (slow[(size_t) echo * 2]) > 0.3f || std::abs (slow[(size_t) echo * 2 + 1]) > 0.3f);
}

static void testRoom()
{
    EffectParams p;
    p.roomMix = 1.0f;
    for (float size : { 0.0f, 0.5f, 1.0f })
    {
        p.roomSize = size;
        const int n = (int) (3.0 * fs);
        const auto out = run (p, impulse (n));
        CHECK (allFinite (out));
        CHECK (rms (out, 0, (int) (0.1 * fs)) > 1.0e-3); // it rings
        CHECK (rms (out, n - (int) (0.2 * fs), n) < 1.0e-5); // and falls silent within its decay time
    }

    // stereo: the two sides differ, so a centred source gets width
    Effects fx;
    fx.prepare (fs);
    p.roomSize = 0.5f;
    fx.setParams (p, 120.0);
    std::vector<float> left = impulse (4800), right = impulse (4800);
    fx.process (left.data(), right.data(), 4800);
    double difference = 0.0;
    for (size_t i = 0; i < left.size(); ++i)
        difference += std::abs (left[i] - right[i]);
    CHECK (difference > 0.1);
}

int main()
{
    testDefaultsPassThrough();
    testFilter();
    testCrusher();
    testDelay();
    testRoom();
    return finish ("effects");
}

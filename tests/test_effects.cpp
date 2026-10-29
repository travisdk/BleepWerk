// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

// The effects: at their defaults the chain passes the signal through untouched, the filter
// types cut where they should, Bits and Rate crush, the delay repeats on the beat and its
// repeats fade, and the plate reverb rings for its decay time,
// darkens with Tone, ducks under a playing input and falls silent. Drive is untouched at Mix 0,
// stays within reason in level, doesn't alias much, and moves between the first and last place
// in the chain without a click or a tail left hanging. Nothing ever goes non-finite.
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

// Every old delayTime keeps its length as a time and a feel, and the new values are right.
static void testDelayTimes()
{
    constexpr double oldBeats[] = { 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.375, 0.5, 2.0 / 3.0, 0.75, 1.0 };
    for (int i = 0; i < (int) std::size (oldBeats); ++i)
    {
        int note = -1, feel = -1;
        splitOldDelayTime (i, note, feel);
        CHECK (std::abs (delayBeats (note, feel) - oldBeats[i]) < 1.0e-9);
    }
    CHECK (std::abs (delayBeats (3, 2) - 1.5) < 1.0e-9); // 1/4 dotted
    CHECK (std::abs (delayBeats (4, 0) - 2.0) < 1.0e-9); // 1/2
    CHECK (std::abs (delayBeats (4, 2) - 3.0) < 1.0e-9); // 1/2 dotted, the longest
}

static void testDelay()
{
    EffectParams p;
    p.delayNote = 1; // 1/16 at 120 BPM: 125 ms
    p.delayFeel = 0;
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

static void testReverb()
{
    EffectParams p;
    p.roomMix = 1.0f;
    for (float size : { 0.0f, 0.5f, 1.0f })
        for (float tone : { 0.0f, 1.0f })
        {
            p.roomSize = size;
            p.roomTone = tone;
            p.roomDecay = 1.2f;
            const int n = (int) (3.0 * fs);
            const auto out = run (p, impulse (n));
            CHECK (allFinite (out));
            CHECK (rms (out, 0, (int) (0.1 * fs)) > 1.0e-3); // it rings
            CHECK (rms (out, n - (int) (0.2 * fs), n) < 1.0e-5); // and falls silent within its decay time
        }

    // Decay sets how long it rings, separately from Size
    p.roomSize = 0.5f;
    p.roomTone = 0.5f;
    auto tailAt = [&p] (float decay)
    {
        p.roomDecay = decay;
        const auto out = run (p, impulse ((int) (2.0 * fs)));
        return rms (out, (int) (1.0 * fs), (int) (1.2 * fs));
    };
    CHECK (tailAt (4.0f) > 30.0 * tailAt (0.5f));

    // the longest setting still dies away, and nothing blows up at the extremes
    p.roomSize = 1.0f;
    p.roomTone = 1.0f;
    p.roomDecay = 6.0f;
    {
        const int n = (int) (12.0 * fs);
        const auto out = run (p, impulse (n));
        CHECK (allFinite (out));
        CHECK (rms (out, n - (int) (0.5 * fs), n) < 1.0e-5);
    }

    // Tone: a dark plate keeps less of the top end than a bright one
    p.roomSize = 0.5f;
    p.roomDecay = 1.2f;
    auto highs = [&p] (float tone)
    {
        p.roomTone = tone;
        const auto out = run (p, impulse ((int) (0.5 * fs)));
        double sum = 0.0;
        for (size_t i = 1; i < out.size(); ++i)
            sum += std::abs (out[i] - out[i - 1]); // a first difference stands in for a high-pass
        return sum;
    };
    CHECK (highs (0.0f) < 0.5 * highs (1.0f));

    // Duck: while a tone plays the wet signal is held back
    auto wetWhilePlaying = [&p] (float duck)
    {
        p.roomTone = 0.5f;
        p.roomDuck = duck;
        const auto out = run (p, sine (440.0, (int) fs, 0.4f));
        return rms (out, (int) (0.5 * fs), (int) fs);
    };
    CHECK (wetWhilePlaying (1.0f) < 0.4 * wetWhilePlaying (0.0f));
    p.roomDuck = 0.0f;

    // stereo: the two sides differ, so a centred source gets width
    Effects fx;
    fx.prepare (fs);
    fx.setParams (p, 120.0);
    std::vector<float> left = impulse (4800), right = impulse (4800);
    fx.process (left.data(), right.data(), 4800);
    double difference = 0.0;
    for (size_t i = 0; i < left.size(); ++i)
        difference += std::abs (left[i] - right[i]);
    CHECK (difference > 0.1);
}

static EffectParams driveParams (DriveType type, float amount, float mix, bool post)
{
    EffectParams p;
    p.driveType = type;
    p.driveAmount = amount;
    p.driveMix = mix;
    p.drivePost = post;
    return p;
}

static double goertzel (const std::vector<float>& v, double hz, int from)
{
    const double w = 2.0 * 3.14159265358979 * hz / fs;
    double re = 0.0, im = 0.0;
    for (size_t i = (size_t) from; i < v.size(); ++i)
    {
        re += v[i] * std::cos (w * (double) i);
        im += v[i] * std::sin (w * (double) i);
    }
    return 2.0 * std::sqrt (re * re + im * im) / (double) (v.size() - (size_t) from);
}

// At Mix 0 the signal is passed on bit for bit, whatever the other Drive settings are
static void testDriveTransparent()
{
    const auto in = sine (440.0, 9600);
    for (int t = 0; t < kNumDriveTypes; ++t)
        for (bool post : { false, true })
        {
            const auto out = run (driveParams ((DriveType) t, 1.0f, 0.0f, post), in);
            CHECK (out == in);
        }
}

// Even the most extreme settings and inputs give finite, bounded output
static void testDriveExtreme()
{
    std::vector<float> loud = sine (300.0, 9600, 8.0f);
    for (size_t i = 4800; i < loud.size(); ++i)
        loud[i] = i % 2000 < 1000 ? 100.0f : -100.0f; // a loud square and steps
    loud[100] = 1000.0f;
    for (int t = 0; t < kNumDriveTypes; ++t)
        for (bool post : { false, true })
            for (float mix : { 0.5f, 1.0f })
            {
                const auto out = run (driveParams ((DriveType) t, 1.0f, mix, post), loud);
                CHECK (allFinite (out));
                float peak = 0.0f;
                for (float x : out)
                    peak = std::max (peak, std::abs (x));
                CHECK (peak < 1100.0f); // the dry part of the 1000 is the most there is
                const auto quiet = run (driveParams ((DriveType) t, 1.0f, mix, post), sine (300.0, 9600, 0.0f));
                CHECK (allFinite (quiet) && rms (quiet, 0, 9600) == 0.0);
            }
}

// The compensation keeps the level of a sine near what went in, however hard it is driven
static void testDriveLevel()
{
    for (int t = 0; t < kNumDriveTypes; ++t)
        for (float amount : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
        {
            const auto p = driveParams ((DriveType) t, amount, 1.0f, false);
            const double in = rms (sine (1000.0, 14400, 0.3f), 4800, 14400);
            const double ratio = rms (run (p, sine (1000.0, 14400, 0.3f)), 4800, 14400) / in;
            std::printf ("level type %d amount %.2f: %.2f\n", t, amount, ratio);
            CHECK (ratio > 0.7 && ratio < 1.4); // within 3 dB
            // a quiet sound comes out louder and a loud one no more than so, but never absurdly
            const double quiet = rms (run (p, sine (1000.0, 14400, 0.03f)), 4800, 14400) / rms (sine (1000.0, 14400, 0.03f), 4800, 14400);
            const double loud = rms (run (p, sine (1000.0, 14400, 0.9f)), 4800, 14400) / rms (sine (1000.0, 14400, 0.9f), 4800, 14400);
            std::printf ("   quiet %.2f loud %.2f\n", quiet, loud);
            CHECK (quiet < 40.0 && quiet > 0.3);
            CHECK (loud < 3.0 && loud > 0.1);
        }
}

// The dry path is delayed to line up with the oversampling filters: a click through an
// undriven wet path arrives as a single peak, in the same place as the dry one
static void testDriveAlignment()
{
    auto click = impulse (400);
    click[0] = 0.0f;
    click[10] = 0.01f; // so small that Warm is linear
    const auto wet = run (driveParams (DriveType::Warm, 0.0f, 1.0f, false), click);
    size_t at = 0;
    for (size_t i = 0; i < wet.size(); ++i)
        if (std::abs (wet[i]) > std::abs (wet[at]))
            at = i;
    std::printf ("drive round trip peak at %zu (%.4f)\n", at - 10, wet[at]);
    CHECK (at > 10);
    // the half-band filters ring a little, but the peak stands clear of them
    for (size_t i = 0; i < wet.size(); ++i)
        if (i != at)
            CHECK (std::abs (wet[i]) < 0.7f * std::abs (wet[at]));
    const auto both = run (driveParams (DriveType::Warm, 0.0f, 0.5f, false), click); // dry full, wet full
    size_t atBoth = 0;
    for (size_t i = 0; i < both.size(); ++i)
        if (std::abs (both[i]) > std::abs (both[atBoth]))
            atBoth = i;
    CHECK (atBoth == at);
}

// Oversampling: the alias products of a hard clip are much weaker than without it
static void testDriveAliasing()
{
    const double f = 7000.0;
    const auto in = sine (f, 48000, 0.5f);
    auto naive = in;
    for (auto& x : naive)
        x = std::clamp (x * 100.0f, -0.85f, 1.0f);
    const auto driven = run (driveParams (DriveType::Hard, 1.0f, 1.0f, false), in);
    // the 5th harmonic at 35 kHz folds to 13 kHz and the 7th at 49 kHz to 1 kHz, neither a real harmonic
    double aliasNaive = 0.0, aliasDriven = 0.0;
    for (double hz : { 1000.0, 13000.0 })
    {
        aliasNaive += goertzel (naive, hz, 9600);
        aliasDriven += goertzel (driven, hz, 9600);
    }
    const double fundamentalNaive = goertzel (naive, f, 9600), fundamentalDriven = goertzel (driven, f, 9600);
    std::printf ("alias naive %.4f driven %.4f (fund %.3f %.3f)\n", aliasNaive / fundamentalNaive, aliasDriven / fundamentalDriven, fundamentalNaive, fundamentalDriven);
    CHECK (aliasDriven / fundamentalDriven < 0.25 * aliasNaive / fundamentalNaive);

    // the same for Fold
    auto foldNaive = in;
    for (auto& x : foldNaive)
        x = std::sin (1.5707963f * x * 100.0f);
    const auto folded = run (driveParams (DriveType::Fold, 1.0f, 1.0f, false), in);
    double aliasFoldNaive = 0.0, aliasFolded = 0.0;
    for (double hz : { 1000.0, 13000.0 })
    {
        aliasFoldNaive += goertzel (foldNaive, hz, 9600);
        aliasFolded += goertzel (folded, hz, 9600);
    }
    std::printf ("fold alias naive %.4f driven %.4f\n", aliasFoldNaive, aliasFolded);
    CHECK (aliasFolded < 0.5 * aliasFoldNaive);
}

// The three types sound different, and more drive adds more harmonics
static void testDriveTypes()
{
    const auto in = sine (440.0, 24000, 0.3f);
    auto harmonics = [&in] (DriveType type, float amount)
    {
        const auto out = run (driveParams (type, amount, 1.0f, false), in);
        return brightness (std::vector<float> (out.begin() + 4800, out.end()));
    };
    for (int t = 0; t < kNumDriveTypes; ++t)
        CHECK (harmonics ((DriveType) t, 1.0f) > 1.5 * harmonics ((DriveType) t, 0.1f));
    CHECK (std::abs (harmonics (DriveType::Warm, 1.0f) - harmonics (DriveType::Fold, 1.0f)) > 0.01);
}

// Pre and Post run the chain in different orders, a switch in the middle of a note neither
// clicks nor leaves a tail, and everything dies away when the input stops
static void testDriveOrder()
{
    EffectParams p = driveParams (DriveType::Hard, 0.8f, 0.8f, false);
    p.delayMix = 0.5f;
    p.delayNote = 1;
    p.delayFeel = 0;
    p.roomMix = 0.4f;
    p.roomDecay = 0.5f;
    p.filterType = FilterType::LowPass;
    p.cutoffHz = 1500.0f;
    const auto in = [] { auto v = sine (200.0, 14400, 0.4f); v.resize (96000, 0.0f); return v; }();
    const auto pre = run (p, in);
    p.drivePost = true;
    const auto post = run (p, in);
    double difference = 0.0;
    for (size_t i = 0; i < pre.size(); ++i)
        difference += std::abs (pre[i] - post[i]);
    CHECK (difference > 10.0);
    CHECK (allFinite (pre) && allFinite (post));

    // a switch every 5000 samples, in blocks, against a plain Pre run: no step bigger than
    // the quick parts of either would have, and all quiet at the end
    Effects fx;
    fx.prepare (fs);
    auto signal = in;
    float worstStep = 0.0f, last = 0.0f;
    for (size_t pos = 0; pos < signal.size(); pos += 256)
    {
        p.drivePost = (pos / 4096) % 2 == 1 && pos < 48000;
        fx.setParams (p, 120.0);
        fx.process (signal.data() + pos, nullptr, (int) std::min<size_t> (256, signal.size() - pos));
    }
    for (size_t i = 0; i < 48000; ++i)
    {
        worstStep = std::max (worstStep, std::abs (signal[i] - last));
        last = signal[i];
    }
    float worstPre = 0.0f;
    for (size_t i = 1; i < 48000; ++i)
        worstPre = std::max ({ worstPre, std::abs (pre[i] - pre[i - 1]), std::abs (post[i] - post[i - 1]) });
    std::printf ("switching: worst step %.4f, worst without %.4f\n", worstStep, worstPre);
    CHECK (allFinite (signal));
    CHECK (worstStep < 1.2f * worstPre + 0.05f);
    CHECK (rms (signal, 90000, 96000) < 1.0e-4);
    CHECK (rms (post, 90000, 96000) < 1.0e-4);
}

// Mix back to 0 hands the signal back untouched, and no click comes with it
static void testDriveMixOff()
{
    EffectParams p = driveParams (DriveType::Warm, 0.7f, 1.0f, true);
    Effects fx;
    fx.prepare (fs);
    auto signal = sine (500.0, 48000, 0.3f);
    const auto original = signal;
    for (size_t pos = 0; pos < signal.size(); pos += 256)
    {
        p.driveMix = pos < 16000 ? 1.0f : 0.0f;
        fx.setParams (p, 120.0);
        fx.process (signal.data() + pos, nullptr, 256);
    }
    float worst = 0.0f;
    for (size_t i = 1; i < signal.size(); ++i)
        worst = std::max (worst, std::abs (signal[i] - signal[i - 1]));
    CHECK (worst < 0.2f); // a 500 Hz sine at 0.3 steps 0.02 at most
    float tailDiff = 0.0f;
    for (size_t i = 40000; i < signal.size(); ++i)
        tailDiff = std::max (tailDiff, std::abs (signal[i] - original[i]));
    CHECK (tailDiff == 0.0f);
}

int main()
{
    testDefaultsPassThrough();
    testFilter();
    testCrusher();
    testDelayTimes();
    testDelay();
    testReverb();
    testDriveTransparent();
    testDriveExtreme();
    testDriveLevel();
    testDriveAlignment();
    testDriveAliasing();
    testDriveTypes();
    testDriveOrder();
    testDriveMixOff();
    return finish ("effects");
}

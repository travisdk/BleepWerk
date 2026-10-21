// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

// The Foley engine: the note (and pitch bend) sets the fundamental and Size shifts it, Damping sets how long
// it rings and every voice ends by itself, Brightness adds overtones, the sweep slides into
// the note, and every exciter and material gives finite output at a sensible level.
#include "FoleySynth.h"
#include "TestUtils.h"

#include <cstdio>
#include <vector>

using namespace bleepforge;

constexpr double fs = 48000.0;

static int ms (double milliseconds) { return (int) (milliseconds * 0.001 * fs); }

static std::vector<float> render (FoleySynth& synth, int numSamples)
{
    std::vector<float> out ((size_t) numSamples);
    for (int pos = 0; pos < numSamples; pos += 256)
        synth.render (out.data() + pos, std::min (256, numSamples - pos));
    return out;
}

static std::vector<float> strike (const FoleyParams& p, int note, double milliseconds, float velocity = 1.0f)
{
    FoleySynth synth;
    synth.prepare (fs);
    synth.setParams (p);
    synth.noteOn (note, velocity);
    return render (synth, ms (milliseconds));
}

static double frequency (const std::vector<float>& v, int from, int to)
{
    double first = -1.0, last = -1.0;
    int crossings = 0;
    for (int i = std::max (1, from); i < to; ++i)
        if (v[(size_t) i - 1] < 0.0f && v[(size_t) i] >= 0.0f)
        {
            const double at = i - 1 + v[(size_t) i - 1] / (v[(size_t) i - 1] - v[(size_t) i]);
            if (first < 0.0)
                first = at;
            last = at;
            ++crossings;
        }
    return crossings < 2 ? 0.0 : (crossings - 1) * fs / (last - first);
}

static double rms (const std::vector<float>& v, int from, int to)
{
    double sum = 0.0;
    for (int i = from; i < to; ++i)
        sum += (double) v[(size_t) i] * v[(size_t) i];
    return std::sqrt (sum / std::max (1, to - from));
}

// RMS of the first difference relative to the RMS: higher for brighter sound.
static double brightnessOf (const std::vector<float>& v, int from, int to)
{
    double diff = 0.0;
    for (int i = from + 1; i < to; ++i)
        diff += std::pow ((double) v[(size_t) i] - v[(size_t) i - 1], 2.0);
    return std::sqrt (diff / (to - from)) / std::max (1.0e-9, rms (v, from, to));
}

static float peak (const std::vector<float>& v)
{
    float m = 0.0f;
    for (float s : v)
        m = std::max (m, std::abs (s));
    return m;
}

// Wood, dark, ringing: after the overtones die out, the fundamental is left.
static FoleyParams pureTone()
{
    FoleyParams p;
    p.material = 0.0f;
    p.brightness = 0.0f;
    p.damping = 0.1f;
    return p;
}

// Pitch bend retunes the object: two semitones up from A4.
static void bendRetunesTheObject()
{
    FoleySynth synth;
    synth.prepare (fs);
    synth.setParams (pureTone());
    synth.setPitchBend (2.0f);
    synth.noteOn (69, 1.0f);
    const auto out = render (synth, ms (400));
    CHECK (std::abs (frequency (out, ms (150), ms (400)) - 493.88) < 2.0);
}

static void noteSetsTheFundamental()
{
    auto out = strike (pureTone(), 69, 400);
    CHECK (std::abs (frequency (out, ms (150), ms (400)) - 440.0) < 2.0);

    auto p = pureTone();
    p.size = 0.0f; // small: an octave up
    out = strike (p, 69, 400);
    CHECK (std::abs (frequency (out, ms (150), ms (400)) - 880.0) < 4.0);

    p.size = 1.0f; // big: an octave down
    out = strike (p, 69, 600);
    CHECK (std::abs (frequency (out, ms (200), ms (600)) - 220.0) < 1.0);
}

static void dampingSetsTheRingTime()
{
    auto p = pureTone();
    p.damping = 0.9f;
    const auto dead = strike (p, 60, 400);
    p.damping = 0.2f;
    const auto ringing = strike (p, 60, 400);
    CHECK (rms (ringing, ms (200), ms (300)) > 10.0 * rms (dead, ms (200), ms (300)));
}

static void voicesEndByThemselves()
{
    for (float material : { 0.0f, 0.5f, 1.0f })
        for (float damping : { 0.0f, 0.5f, 1.0f })
        {
            FoleyParams p;
            p.material = material;
            p.damping = damping;
            p.size = 1.0f; // the longest ring
            FoleySynth synth;
            synth.prepare (fs);
            synth.setParams (p);
            synth.noteOn (48, 1.0f);
            render (synth, ms (50));
            CHECK (synth.activeVoices() == 1);
            render (synth, ms (15000));
            CHECK (synth.activeVoices() == 0);
        }

    // a dead knock is over quickly
    FoleyParams knock;
    knock.damping = 1.0f;
    FoleySynth synth;
    synth.prepare (fs);
    synth.setParams (knock);
    synth.noteOn (60, 1.0f);
    render (synth, ms (300));
    CHECK (synth.activeVoices() == 0);
}

static void brightnessAddsOvertones()
{
    auto p = pureTone();
    p.damping = 0.4f;
    const auto dark = strike (p, 57, 200);
    p.brightness = 1.0f;
    const auto bright = strike (p, 57, 200);
    CHECK (brightnessOf (bright, 0, ms (50)) > 1.5 * brightnessOf (dark, 0, ms (50)));
}

static void sweepSlidesIntoTheNote()
{
    auto p = pureTone();
    p.sweepSemis = 12.0f;
    p.damping = 0.05f;
    const auto out = strike (p, 72, 500);
    const double early = frequency (out, 0, ms (20));
    const double late = frequency (out, ms (300), ms (500));
    CHECK (early < 0.8 * 523.25);
    CHECK (std::abs (late - 523.25) < 3.0);
}

static void everyExciterAndMaterialIsSane()
{
    float loudest = 0.0f, quietest = 1.0e9f;
    for (int e = 0; e < kNumFoleyExciters; ++e)
        for (float material : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
            for (float brightness : { 0.0f, 1.0f })
                for (int note : { 24, 60, 96, 127 })
                {
                    FoleyParams p;
                    p.exciter = (FoleyExciter) e;
                    p.exciterMs = e == (int) FoleyExciter::Crackle ? 60.0f : 3.0f;
                    p.material = material;
                    p.brightness = brightness;
                    p.damping = 0.4f;
                    const auto out = strike (p, note, 300);
                    bool finite = true;
                    double sum = 0.0;
                    for (float s : out)
                    {
                        finite = finite && std::isfinite (s);
                        sum += s;
                    }
                    CHECK (finite);
                    CHECK (std::abs (sum / (double) out.size()) < 0.01); // no DC
                    const float pk = peak (out);
                    loudest = std::max (loudest, pk);
                    if (note <= 96)
                        quietest = std::min (quietest, pk);
                }
    std::printf ("foley peaks: %.3f .. %.3f\n", quietest, loudest);
    CHECK (loudest < 1.5f);
    CHECK (quietest > 0.02f);
}

static void velocityScalesTheLevel()
{
    const auto soft = strike (pureTone(), 60, 100, 0.25f);
    const auto hard = strike (pureTone(), 60, 100, 1.0f);
    CHECK (std::abs (peak (hard) / peak (soft) - 4.0f) < 0.2f);
}

// A softer strike is darker, not only quieter.
static void softerIsDarker()
{
    for (auto exciter : { FoleyExciter::Mallet, FoleyExciter::Noise })
    {
        FoleyParams p;
        p.exciter = exciter;
        const double soft = brightness (strike (p, 60, 300, 0.3f)), hard = brightness (strike (p, 60, 300, 1.0f));
        std::printf ("%s brightness soft %.4f hard %.4f\n", kFoleyExciterNames[(int) exciter], soft, hard);
        CHECK (soft < hard * 0.85);
    }
}

// With Chaos off, a note struck again sounds the same, also with the noisy exciters (a looped
// bar mustn't change from pass to pass); any Chaos makes each hit different.
static void repeatedHitsMatchWithoutChaos()
{
    for (auto exciter : { FoleyExciter::Noise, FoleyExciter::Crackle })
        for (bool withChaos : { false, true })
        {
            FoleyParams p = pureTone();
            p.exciter = exciter;
            p.exciterMs = 5.0f;
            p.damping = 0.9f;
            FoleySynth synth;
            synth.prepare (fs);
            synth.setParams (p);
            ChaosDepths chaos;
            chaos.pan = 0.5f; // moves no sample of a mono render, only the noise
            synth.setChaos (withChaos ? chaos : ChaosDepths {});

            synth.noteOn (60, 1.0f);
            const auto first = render (synth, ms (300));
            CHECK (synth.activeVoices() == 0);
            synth.noteOn (67, 1.0f); // another note in between moves on any shared noise
            render (synth, ms (300));
            synth.noteOn (60, 1.0f);
            const auto again = render (synth, ms (300));
            CHECK ((first == again) != withChaos);
        }
}

int main()
{
    noteSetsTheFundamental();
    bendRetunesTheObject();
    dampingSetsTheRingTime();
    voicesEndByThemselves();
    brightnessAddsOvertones();
    sweepSlidesIntoTheNote();
    everyExciterAndMaterialIsSane();
    velocityScalesTheLevel();
    softerIsDarker();
    repeatedHitsMatchWithoutChaos();
    return finish ("foley");
}

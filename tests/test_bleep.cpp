// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

// The Bleep engine: pitch, envelopes ending by themselves, no DC or onset clicks, finite
// output for every wave, and voice allocation in Mono and Poly mode.
#include "BleepSynth.h"
#include "TestUtils.h"

#include <vector>

using namespace bleepforge;

constexpr double fs = 48000.0;

static std::vector<float> render (BleepSynth& synth, int numSamples)
{
    std::vector<float> out ((size_t) numSamples);
    for (int pos = 0; pos < numSamples; pos += 256) // in blocks, as the plug-in does
        synth.render (out.data() + pos, std::min (256, numSamples - pos));
    return out;
}

static int ms (double milliseconds) { return (int) (milliseconds * 0.001 * fs); }

// Frequency from the rising zero crossings in [from, to), interpolated to sub-sample precision.
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

static BleepParams steady (BleepWave wave)
{
    BleepParams p;
    p.wave = wave;
    p.pitchEnvSemis = 0.0f;
    p.decayMs = 5000.0f;
    return p;
}

static BleepSynth makeSynth (const BleepParams& p, BleepSynth::Mode mode = BleepSynth::Mode::Poly)
{
    BleepSynth synth;
    synth.prepare (fs);
    synth.setMode (mode);
    synth.setParams (p);
    return synth;
}

static void pitchIsRight()
{
    for (auto wave : { BleepWave::Sine, BleepWave::Triangle, BleepWave::Pulse })
    {
        auto synth = makeSynth (steady (wave));
        synth.noteOn (69, 1.0f);
        const auto out = render (synth, ms (300));
        CHECK (std::abs (frequency (out, ms (50), ms (300)) - 440.0) < 0.5);
    }

    auto p = steady (BleepWave::Sine);
    p.octave = -1;
    auto synth = makeSynth (p);
    synth.noteOn (69, 1.0f);
    const auto out = render (synth, ms (300));
    CHECK (std::abs (frequency (out, ms (50), ms (300)) - 220.0) < 0.5);
}

static void pitchEnvelopeFalls()
{
    auto p = steady (BleepWave::Sine);
    p.pitchEnvSemis = 12.0f;
    p.pitchEnvDecayMs = 60.0f;
    auto synth = makeSynth (p);
    synth.noteOn (81, 1.0f);
    const auto out = render (synth, ms (400));
    const double early = frequency (out, 0, ms (10));
    const double late = frequency (out, ms (200), ms (400));
    CHECK (early > 1.5 * 880.0);
    CHECK (std::abs (late - 880.0) < 1.0);
}

static void notesEndByThemselves()
{
    auto p = steady (BleepWave::Pulse);
    p.decayMs = 100.0f;
    auto synth = makeSynth (p);
    synth.noteOn (60, 1.0f); // never released
    render (synth, ms (50));
    CHECK (synth.activeVoices() == 1);
    render (synth, ms (100));
    CHECK (synth.activeVoices() == 0);
}

static void releaseCutsANote()
{
    auto p = steady (BleepWave::Sine);
    p.releaseMs = 20.0f;
    auto synth = makeSynth (p);
    synth.noteOn (60, 1.0f);
    render (synth, ms (50));
    synth.noteOff (60);
    render (synth, ms (30));
    CHECK (synth.activeVoices() == 0);
}

static void noDcAndNoOnsetClick()
{
    for (auto wave : { BleepWave::Sine, BleepWave::Triangle, BleepWave::Pulse, BleepWave::FM })
        for (float pw : { 0.5f, 0.2f, 0.8f })
        {
            auto p = steady (wave);
            p.pulseWidth = pw;
            p.attackMs = 0.0f; // the hardest case: full level on the first sample
            auto synth = makeSynth (p);
            synth.noteOn (57, 1.0f);
            const auto out = render (synth, ms (500));

            double sum = 0.0;
            for (int i = ms (100); i < ms (500); ++i)
                sum += out[(size_t) i];
            CHECK (std::abs (sum / ms (400)) < 0.01);

            // sine, triangle and FM start at zero; a pulse can't, it starts on its edge
            if (wave != BleepWave::Pulse)
                CHECK (std::abs (out[0]) < 0.05f);
        }
}

static void everyWaveIsFinite()
{
    for (int w = 0; w < kNumBleepWaves; ++w)
    {
        auto p = steady ((BleepWave) w);
        p.pitchEnvSemis = 48.0f;
        p.fmAmount = 1.0f;
        p.fmRatio = 8.0f;
        p.noise = 0.5f;
        p.octave = 3;
        auto synth = makeSynth (p);
        for (int note = 0; note < 128; note += 16)
            synth.noteOn (note, 1.0f);
        const auto out = render (synth, ms (200));
        bool finite = true;
        float peak = 0.0f;
        for (float s : out)
        {
            finite = finite && std::isfinite (s);
            peak = std::max (peak, std::abs (s));
        }
        CHECK (finite);
        CHECK (peak > 0.01f && peak < 8.0f);
    }
}

static void monoPlaysOneVoiceAndReturnsToHeldKey()
{
    auto synth = makeSynth (steady (BleepWave::Sine), BleepSynth::Mode::Mono);
    synth.noteOn (60, 1.0f);
    synth.noteOn (72, 1.0f);
    auto out = render (synth, ms (200));
    CHECK (synth.activeVoices() == 1);
    CHECK (std::abs (frequency (out, ms (20), ms (200)) - 523.25) < 1.0);

    synth.noteOff (72); // back to the still-held C4
    out = render (synth, ms (200));
    CHECK (synth.activeVoices() == 1);
    CHECK (std::abs (frequency (out, ms (20), ms (200)) - 261.63) < 1.0);

    synth.noteOff (60);
    render (synth, ms (100));
    CHECK (synth.activeVoices() == 0);
}

static void monoGlideSlides()
{
    auto synth = makeSynth (steady (BleepWave::Sine), BleepSynth::Mode::Mono);
    synth.setGlideMs (100.0f);
    synth.noteOn (60, 1.0f);
    render (synth, ms (50));
    synth.noteOn (72, 1.0f);
    const auto out = render (synth, ms (600));
    const double during = frequency (out, 0, ms (30));
    CHECK (during > 270.0 && during < 500.0);
    CHECK (std::abs (frequency (out, ms (400), ms (600)) - 523.25) < 1.0);
}

static void polyAllocatesAndSteals()
{
    auto synth = makeSynth (steady (BleepWave::Sine));
    synth.noteOn (60, 1.0f);
    synth.noteOn (64, 1.0f);
    synth.noteOn (67, 1.0f);
    render (synth, ms (10));
    CHECK (synth.activeVoices() == 3);

    synth.noteOn (60, 1.0f); // a retrigger reuses its voice
    render (synth, ms (10));
    CHECK (synth.activeVoices() == 3);

    for (int note = 70; note < 80; ++note)
        synth.noteOn (note, 1.0f);
    render (synth, ms (10));
    CHECK (synth.activeVoices() == BleepSynth::kMaxVoices);

    synth.allNotesOff();
    render (synth, ms (200));
    CHECK (synth.activeVoices() == 0);
}

static void modeSwitchReleasesEverything()
{
    auto synth = makeSynth (steady (BleepWave::Sine));
    synth.noteOn (60, 1.0f);
    synth.noteOn (64, 1.0f);
    render (synth, ms (10));
    synth.setMode (BleepSynth::Mode::Mono);
    render (synth, ms (200));
    CHECK (synth.activeVoices() == 0);
}

int main()
{
    pitchIsRight();
    pitchEnvelopeFalls();
    notesEndByThemselves();
    releaseCutsANote();
    noDcAndNoOnsetClick();
    everyWaveIsFinite();
    monoPlaysOneVoiceAndReturnsToHeldKey();
    monoGlideSlides();
    polyAllocatesAndSteals();
    modeSwitchReleasesEverything();
    return finish ("bleep");
}

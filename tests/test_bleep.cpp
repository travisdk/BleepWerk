// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

// The Bleep engine: pitch, envelopes ending by themselves, no DC or onset clicks, finite
// output for every wave, and voice allocation in Mono and Poly mode.
#include "BleepSynth.h"
#include "TestUtils.h"

#include <algorithm>
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
        auto synth = makeSynth (p);
        for (int note = 7; note < 128; note += 15) // up to G9, near the top of the range
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

static void chaosVariesEachNote()
{
    // without Chaos the same note is the same every time
    auto p = steady (BleepWave::Sine);
    auto synth = makeSynth (p);
    std::vector<double> pitches;
    ChaosDepths chaos;
    chaos.pitch = 1.0f;
    for (bool withChaos : { false, true })
    {
        synth.setChaos (withChaos ? chaos : ChaosDepths {});
        pitches.clear();
        for (int i = 0; i < 12; ++i)
        {
            synth.allNotesOff();
            render (synth, ms (300));
            synth.noteOn (69, 1.0f);
            const auto out = render (synth, ms (150));
            pitches.push_back (frequency (out, ms (20), ms (150)));
        }
        const auto [lo, hi] = std::minmax_element (pitches.begin(), pitches.end());
        if (! withChaos)
            CHECK (*hi - *lo < 0.5);
        else
        {
            CHECK (*hi - *lo > 20.0);                                  // audibly different notes
            CHECK (*lo > 440.0 * std::exp2 (-kChaosMaxPitchSemis / 12.0) - 1.0); // within the range
            CHECK (*hi < 440.0 * std::exp2 (kChaosMaxPitchSemis / 12.0) + 1.0);
        }
    }
}

static void chaosPansNotes()
{
    auto synth = makeSynth (steady (BleepWave::Sine));
    ChaosDepths chaos;
    chaos.pan = 1.0f;
    synth.setChaos (chaos);
    int leftHeavy = 0, rightHeavy = 0;
    for (int i = 0; i < 16; ++i)
    {
        synth.allNotesOff();
        std::vector<float> l ((size_t) ms (300)), r ((size_t) ms (300));
        synth.renderAdding (l.data(), r.data(), ms (300)); // let the last note end
        synth.noteOn (69, 1.0f);
        std::fill (l.begin(), l.end(), 0.0f);
        std::fill (r.begin(), r.end(), 0.0f);
        synth.renderAdding (l.data(), r.data(), ms (100));
        double el = 0.0, er = 0.0;
        for (int s = ms (10); s < ms (100); ++s)
        {
            el += l[(size_t) s] * l[(size_t) s];
            er += r[(size_t) s] * r[(size_t) s];
        }
        leftHeavy += el > 1.5 * er ? 1 : 0;
        rightHeavy += er > 1.5 * el ? 1 : 0;
    }
    CHECK (leftHeavy > 2 && rightHeavy > 2);

    // centred notes keep full level on both sides
    auto centred = makeSynth (steady (BleepWave::Sine));
    centred.noteOn (69, 1.0f);
    std::vector<float> l ((size_t) ms (50)), r ((size_t) ms (50)), m ((size_t) ms (50));
    centred.renderAdding (l.data(), r.data(), ms (50));
    auto mono = makeSynth (steady (BleepWave::Sine));
    mono.noteOn (69, 1.0f);
    mono.render (m.data(), ms (50));
    CHECK (std::equal (l.begin(), l.end(), m.begin()) && std::equal (r.begin(), r.end(), m.begin()));
}

static void chaosVariesDecay()
{
    auto p = steady (BleepWave::Sine);
    p.decayMs = 200.0f;
    auto synth = makeSynth (p);
    ChaosDepths chaos;
    chaos.decay = 1.0f;
    synth.setChaos (chaos);
    int shortOnes = 0, longOnes = 0;
    for (int i = 0; i < 16; ++i)
    {
        synth.noteOn (69, 1.0f);
        int samples = 0;
        while (synth.activeVoices() > 0 && samples < ms (2000))
        {
            render (synth, 64);
            samples += 64;
        }
        shortOnes += samples < ms (140) ? 1 : 0;
        longOnes += samples > ms (280) ? 1 : 0;
    }
    CHECK (shortOnes > 1 && longOnes > 1);
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
    chaosVariesEachNote();
    chaosPansNotes();
    chaosVariesDecay();
    return finish ("bleep");
}

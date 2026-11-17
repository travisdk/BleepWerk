// Copyright (C) 2026 Henrik Juhl. Part of BleepWerk, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

// The Strike engine: every exciter on every body gives finite output at a sensible level, it is
// silent with no notes, every voice ends by itself (note-offs or not), the body rings on the
// note, Ring sets how long, Length sets how long Blow and Squeal last, Sweep slides into the
// note, bend moves it, and Chaos and velocity behave.
#include "StrikeSynth.h"
#include "TestUtils.h"

#include <cstdio>
#include <vector>

using namespace bleepwerk;

constexpr double fs = 48000.0;

static int ms (double milliseconds) { return (int) (milliseconds * 0.001 * fs); }
static double noteHz (int note) { return 440.0 * std::exp2 ((note - 69) / 12.0); }

static std::vector<float> render (StrikeSynth& synth, int numSamples)
{
    std::vector<float> out ((size_t) numSamples);
    for (int pos = 0; pos < numSamples; pos += 256)
        synth.render (out.data() + pos, std::min (256, numSamples - pos));
    return out;
}

static std::vector<float> play (const StrikeParams& p, int note, double milliseconds, float velocity = 1.0f)
{
    StrikeSynth synth;
    synth.prepare (fs);
    synth.setParams (p);
    synth.noteOn (note, velocity);
    return render (synth, ms (milliseconds));
}

static StrikeParams params (StrikeExciter exciter, StrikeBody body, float ring = 0.5f, float tone = 0.5f, float motion = 0.0f,
                            float lengthMs = 300.0f)
{
    StrikeParams p;
    p.exciter = exciter;
    p.body = body;
    p.ring = ring;
    p.tone = tone;
    p.motion = motion;
    p.lengthMs = lengthMs;
    return p;
}

static double rms (const std::vector<float>& v, int from, int to)
{
    double sum = 0.0;
    for (int i = from; i < to; ++i)
        sum += (double) v[(size_t) i] * v[(size_t) i];
    return std::sqrt (sum / std::max (1, to - from));
}

static float peak (const std::vector<float>& v)
{
    float p = 0.0f;
    for (float x : v)
        p = std::max (p, std::abs (x));
    return p;
}

static bool finite (const std::vector<float>& v)
{
    for (float x : v)
        if (! std::isfinite (x))
            return false;
    return true;
}

// The frequency between lo and hi (Hz) with the most energy, in 1 Hz steps.
static double strongestHz (const std::vector<float>& v, int from, int to, double lo, double hi)
{
    double best = lo, bestPower = -1.0;
    for (double f = lo; f <= hi; f += 1.0)
    {
        const double c = 2.0 * std::cos (2.0 * 3.14159265358979 * f / fs);
        double s1 = 0.0, s2 = 0.0;
        for (int i = from; i < to; ++i)
        {
            const double s0 = v[(size_t) i] + c * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        const double power = s1 * s1 + s2 * s2 - c * s1 * s2;
        if (power > bestPower)
        {
            bestPower = power;
            best = f;
        }
    }
    return best;
}

static void everyCombinationIsSane()
{
    for (int e = 0; e < kNumStrikeExciters; ++e)
        for (int b = 0; b < kNumStrikeBodies; ++b)
            for (float ring : { 0.0f, 0.5f, 1.0f })
                for (float tone : { 0.0f, 1.0f })
                    for (float motion : { 0.0f, 1.0f })
                    {
                        auto p = params ((StrikeExciter) e, (StrikeBody) b, ring, tone, motion, 200.0f);
                        p.sweepSemis = motion > 0.5f ? 12.0f : -12.0f;
                        // Squeal sounds two octaves above the note, so the top note is beyond what can be played
                        for (int note : { 12, 60, 120 })
                        {
                            if (p.exciter == StrikeExciter::Friction && note > 90)
                                continue;
                            const auto out = play (p, note, 500);
                            CHECK (finite (out));
                            const float pk = peak (out);
                            if (! (pk > 1.0e-4f && pk < 1.0f))
                                std::printf ("level %.5f: exciter %d body %d ring %.1f tone %.0f motion %.0f note %d\n", pk, e, b, ring, tone, motion, note);
                            CHECK (pk > 1.0e-4f && pk < 1.0f);
                        }
                    }
}

static void silentWithNoNotes()
{
    StrikeSynth synth;
    synth.prepare (fs);
    for (int e = 0; e < kNumStrikeExciters; ++e)
    {
        synth.setParams (params ((StrikeExciter) e, StrikeBody::Wood));
        CHECK (peak (render (synth, ms (200))) == 0.0f);
    }
    CHECK (synth.activeVoices() == 0);
}

// With or without a note-off, every voice is gone a while after the sound should have ended.
static void voicesEndByThemselves()
{
    for (int e = 0; e < kNumStrikeExciters; ++e)
        for (int b = 0; b < kNumStrikeBodies; ++b)
            for (bool noteOff : { false, true })
            {
                StrikeSynth synth;
                synth.prepare (fs);
                synth.setParams (params ((StrikeExciter) e, (StrikeBody) b, 0.6f, 0.5f, 0.5f, 400.0f));
                synth.noteOn (48, 0.9f);
                synth.noteOn (55, 0.5f);
                render (synth, ms (50));
                if (noteOff)
                {
                    synth.noteOff (48);
                    synth.noteOff (55);
                }
                render (synth, ms (12000));
                if (synth.activeVoices() != 0)
                    std::printf ("still active: exciter %d body %d noteOff %d\n", e, b, (int) noteOff);
                CHECK (synth.activeVoices() == 0);
                CHECK (peak (render (synth, ms (100))) == 0.0f);
            }
}

// Every body rings on the note, whichever way it is struck.
static void bodyRingsOnTheNote()
{
    for (int b = 0; b < kNumStrikeBodies; ++b)
        for (int note : { 45, 57 })
        {
            const double hz = noteHz (note);
            const auto out = play (params (StrikeExciter::Knock, (StrikeBody) b, 1.0f, 0.3f), note, 600);
            const double found = strongestHz (out, ms (20), ms (400), hz * 0.85, hz * 1.15);
            std::printf ("body %d note %d: %.1f Hz (note %.1f)\n", b, note, found, hz);
            CHECK (std::abs (found / hz - 1.0) < 0.01);
        }
}

// Ring sets how long it rings, and each body has its own scale.
static void ringSetsTheRingTime()
{
    auto tail = [] (StrikeBody body, float ring)
    {
        const auto out = play (params (StrikeExciter::Knock, body, ring), 60, 1500);
        return rms (out, ms (300), ms (400));
    };
    CHECK (tail (StrikeBody::Wood, 0.9f) > 5.0 * tail (StrikeBody::Wood, 0.3f));
    CHECK (strikeRingSeconds (kWoodBody, 0.0f) < strikeRingSeconds (kWoodBody, 1.0f));
}

// A skin is a drum, not a plate: it rings only a little longer than a bar, its high modes die much
// faster than its low ones, and the low modes are the loudest.
static void aSkinSoundsLikeADrum()
{
    for (float ring : { 0.2f, 0.5f, 0.8f })
    {
        const float ratio = strikeRingSeconds (kMembraneBody, ring) / strikeRingSeconds (kWoodBody, ring);
        CHECK (ratio >= 1.0f && ratio <= 1.6f);
    }
    // each mode's ring time falls with its ratio (as updateCoefs sets it); the top one is a short
    // colour: under a tenth of the fundamental's ring, and every mode rings shorter than the one below it
    for (size_t k = 1; k < kMembraneBody.ratios.size(); ++k)
        CHECK (std::pow (kMembraneBody.ratios[k], -kMembraneBody.highModeDamping) < std::pow (kMembraneBody.ratios[k - 1], -kMembraneBody.highModeDamping));
    CHECK (std::pow (kMembraneBody.ratios.back(), -kMembraneBody.highModeDamping) < 0.1f);
    CHECK (kMembraneBody.extraTilt > kWoodBody.extraTilt);

    // heard: a hit's tail is dominated by the fundamental, so the hit decays to a low note, not a ring of
    // high partials; compare the level of the first 20 ms with a late stretch
    const auto out = play (params (StrikeExciter::Knock, StrikeBody::Membrane, 0.5f, 0.5f), 48, 1500);
    CHECK (rms (out, 0, ms (20)) > 3.0 * rms (out, ms (300), ms (400)));
}

// Length sets how long Blow and Squeal last, fade included, and note-offs don't stop them.
static void lengthSetsHowLongItLasts()
{
    for (auto exciter : { StrikeExciter::Steam, StrikeExciter::Friction })
    {
        const auto shortOne = play (params (exciter, StrikeBody::Wood, 0.5f, 0.5f, 0.0f, 150.0f), 48, 3000);
        const auto longOne = play (params (exciter, StrikeBody::Wood, 0.5f, 0.5f, 0.0f, 1500.0f), 48, 3000);
        CHECK (rms (shortOne, ms (600), ms (800)) < 0.02 * rms (shortOne, ms (20), ms (60)));
        CHECK (rms (longOne, ms (600), ms (800)) > 0.1 * rms (longOne, ms (200), ms (400)));
        CHECK (rms (longOne, ms (2800), ms (3000)) < 0.01 * rms (longOne, ms (200), ms (400)));
    }

    StrikeSynth synth;
    synth.prepare (fs);
    synth.setParams (params (StrikeExciter::Steam, StrikeBody::Wood, 0.5f, 0.5f, 0.0f, 1000.0f));
    synth.noteOn (48, 1.0f);
    render (synth, ms (50));
    synth.noteOff (48);
    CHECK (rms (render (synth, ms (300)), 0, ms (300)) > 1.0e-3);
}

// Brush: Length is the burst, not the sound; a longer burst is not louder.
static void burstLengthDoesNotChangeTheLevelMuch()
{
    const auto shortOne = play (params (StrikeExciter::Noise, StrikeBody::Wood, 0.5f, 0.5f, 0.0f, 2.0f), 48, 800);
    const auto longOne = play (params (StrikeExciter::Noise, StrikeBody::Wood, 0.5f, 0.5f, 0.0f, 40.0f), 48, 800);
    const double ratio = rms (longOne, 0, ms (400)) / rms (shortOne, 0, ms (400));
    CHECK (ratio > 0.2 && ratio < 5.0);
}

// Presets and projects saved before Crackle and Metal were removed keep their sound: the old
// choice indices map to the ones that replaced them (Crackle to Brush, Metal to Bar).
static void oldChoicesMigrate()
{
    CHECK (migrateStrikeExciter (0) == (int) StrikeExciter::Knock);
    CHECK (migrateStrikeExciter (1) == (int) StrikeExciter::Noise);
    CHECK (migrateStrikeExciter (2) == (int) StrikeExciter::Noise);
    CHECK (migrateStrikeExciter (3) == (int) StrikeExciter::Steam);
    CHECK (migrateStrikeExciter (4) == (int) StrikeExciter::Friction);
    CHECK (migrateStrikeBody (0) == (int) StrikeBody::Wood);
    CHECK (migrateStrikeBody (1) == (int) StrikeBody::Wood);
    CHECK (migrateStrikeBody (2) == (int) StrikeBody::Membrane);
}

// Tone: bright strikes have more high end, dark ones less.
static void toneAddsBrightness()
{
    for (int e = 0; e < kNumStrikeExciters; ++e)
    {
        const auto dark = play (params ((StrikeExciter) e, StrikeBody::Wood, 0.5f, 0.0f), 48, 400);
        const auto bright = play (params ((StrikeExciter) e, StrikeBody::Wood, 0.5f, 1.0f), 48, 400);
        if (! (brightness (bright) > brightness (dark)))
            std::printf ("exciter %d: dark %.4f bright %.4f\n", e, brightness (dark), brightness (bright));
        CHECK (brightness (bright) > brightness (dark));
    }
}

// Sweep: the body starts away from the note and slides in; negative starts above.
static void sweepSlidesIntoTheNote()
{
    const double hz = noteHz (57);
    auto p = params (StrikeExciter::Knock, StrikeBody::Wood, 1.0f, 0.3f);
    p.sweepSemis = 12.0f;
    const auto up = play (p, 57, 700);
    CHECK (strongestHz (up, 0, ms (8), hz * 0.4, hz * 1.2) < hz * 0.8); // starts an octave below...
    CHECK (std::abs (strongestHz (up, ms (300), ms (600), hz * 0.85, hz * 1.15) / hz - 1.0) < 0.01); // ...and settles on the note
    p.sweepSemis = -12.0f;
    const auto down = play (p, 57, 700);
    CHECK (strongestHz (down, 0, ms (8), hz * 0.8, hz * 2.4) > hz * 1.3);
    CHECK (std::abs (strongestHz (down, ms (300), ms (600), hz * 0.85, hz * 1.15) / hz - 1.0) < 0.01);
}

static void bendMovesTheNote()
{
    const double hz = noteHz (57);
    StrikeSynth synth;
    synth.prepare (fs);
    synth.setParams (params (StrikeExciter::Knock, StrikeBody::Wood, 1.0f, 0.3f));
    synth.setPitchBend (12.0f);
    synth.noteOn (57, 1.0f);
    const auto out = render (synth, ms (700));
    CHECK (std::abs (strongestHz (out, ms (50), ms (400), hz * 1.7, hz * 2.3) / (2.0 * hz) - 1.0) < 0.01);
}

static void velocityScalesTheLevel()
{
    for (int e = 0; e < kNumStrikeExciters; ++e)
    {
        const auto p = params ((StrikeExciter) e, StrikeBody::Wood);
        const auto loud = play (p, 55, 400, 1.0f);
        const auto soft = play (p, 55, 400, 0.3f);
        CHECK (peak (loud) > peak (soft));
        CHECK (peak (soft) > 0.0f);
    }
}

// Chaos on the decay makes repeated hits differ; without it they match exactly.
static void chaosVariesTheHits()
{
    for (bool chaos : { false, true })
    {
        StrikeSynth synth;
        synth.prepare (fs);
        synth.setParams (params (StrikeExciter::Noise, StrikeBody::Wood, 0.6f, 0.5f, 0.0f, 3.0f));
        ChaosDepths depths;
        if (chaos)
            depths.pitch = depths.decay = depths.tone = 0.8f;
        synth.setChaos (depths);
        std::vector<std::vector<float>> hits;
        for (int i = 0; i < 2; ++i)
        {
            synth.noteOn (48, 0.8f);
            hits.push_back (render (synth, ms (600)));
            synth.noteOff (48);
            render (synth, ms (6000));
        }
        double difference = 0.0;
        for (size_t i = 0; i < hits[0].size(); ++i)
            difference = std::max (difference, (double) std::abs (hits[0][i] - hits[1][i]));
        CHECK (chaos ? difference > 0.01 : difference == 0.0);
    }
}

// Mono: the last key wins and the voice follows it; poly voices stay at eight.
static void monoAndPolyBehave()
{
    StrikeSynth synth;
    synth.prepare (fs);
    synth.setParams (params (StrikeExciter::Steam, StrikeBody::Wood, 0.5f, 0.5f, 0.0f, 3000.0f));
    synth.setMode (StrikeSynth::Mode::Mono);
    synth.noteOn (48, 1.0f);
    synth.noteOn (55, 1.0f);
    render (synth, ms (100));
    CHECK (synth.activeVoices() == 1);
    synth.setMode (StrikeSynth::Mode::Poly);
    for (int n = 40; n < 60; ++n)
        synth.noteOn (n, 1.0f);
    CHECK (finite (render (synth, ms (200))));
    CHECK (synth.activeVoices() <= StrikeSynth::kMaxVoices);
    synth.allNotesOff();
    render (synth, ms (8000));
    CHECK (synth.activeVoices() == 0);
}

// A retrigger of a ringing note does not click: the output stays continuous.
static void retriggerDoesNotClickLoudly()
{
    StrikeSynth synth;
    synth.prepare (fs);
    synth.setParams (params (StrikeExciter::Knock, StrikeBody::Membrane, 0.8f, 0.5f));
    synth.noteOn (48, 0.8f);
    const auto first = render (synth, ms (100));
    synth.noteOn (48, 0.8f);
    const auto second = render (synth, ms (100));
    CHECK (finite (second));
    CHECK (peak (second) < 4.0f * peak (first));
}

int main()
{
    everyCombinationIsSane();
    silentWithNoNotes();
    voicesEndByThemselves();
    bodyRingsOnTheNote();
    ringSetsTheRingTime();
    aSkinSoundsLikeADrum();
    lengthSetsHowLongItLasts();
    burstLengthDoesNotChangeTheLevelMuch();
    toneAddsBrightness();
    sweepSlidesIntoTheNote();
    bendMovesTheNote();
    velocityScalesTheLevel();
    chaosVariesTheHits();
    monoAndPolyBehave();
    retriggerDoesNotClickLoudly();
    oldChoicesMigrate();
    return finish ("strike");
}

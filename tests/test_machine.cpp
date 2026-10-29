// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

// The Machine engine: every type is tuned to the note and follows pitch bend, with or without the resonator, Length
// sets how long a sound lasts and every voice ends by itself (note-offs or not), Motion moves
// the sound and 0 keeps it still, Chaos makes every hit ring a little differently, and every
// type and setting gives finite output at a sensible level.
#include "MachineSynth.h"
#include "TestUtils.h"

#include <cstdio>
#include <vector>

using namespace bleepforge;

constexpr double fs = 48000.0;

static int ms (double milliseconds) { return (int) (milliseconds * 0.001 * fs); }
static double noteHz (int note) { return 440.0 * std::exp2 ((note - 69) / 12.0); }

static std::vector<float> render (MachineSynth& synth, int numSamples)
{
    std::vector<float> out ((size_t) numSamples);
    for (int pos = 0; pos < numSamples; pos += 256)
        synth.render (out.data() + pos, std::min (256, numSamples - pos));
    return out;
}

static std::vector<float> play (const MachineParams& p, int note, double milliseconds, float velocity = 1.0f)
{
    MachineSynth synth;
    synth.prepare (fs);
    synth.setParams (p);
    synth.noteOn (note, velocity);
    return render (synth, ms (milliseconds));
}

static MachineParams params (MachineType type, float tone, float focus, float motion = 0.0f, float lengthMs = 1000.0f,
                             float metal = 0.0f)
{
    MachineParams p;
    p.metal = metal;
    p.type = type;
    p.tone = tone;
    p.focus = focus;
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

// The lag (in samples) between minLag and maxLag where the signal best matches itself.
static int bestLag (const std::vector<float>& v, int from, int to, int minLag, int maxLag)
{
    int best = minLag;
    double bestScore = -1.0e30;
    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        double score = 0.0;
        for (int i = from; i < to; ++i)
            score += (double) v[(size_t) i] * v[(size_t) i - (size_t) lag];
        if (score > bestScore)
        {
            bestScore = score;
            best = lag;
        }
    }
    return best;
}

static void everyTypeFollowsTheNote()
{
    for (int note : { 45, 57 })
    {
        const double hz = noteHz (note);

        // a whistling hiss at Tone 0 sits on the note (found by its period: the faint
        // broadband rest of the noise would throw off a count of zero crossings)
        const double period = fs / hz;
        const auto hiss = play (params (MachineType::Hiss, 0.0f, 1.0f), note, 600);
        const double hissHz = fs / bestLag (hiss, ms (100), ms (400), (int) (period * 0.7), (int) (period * 1.3));
        CHECK (std::abs (hissHz / hz - 1.0) < 0.03);

        // a ringing grind repeats once per period of the note
        const auto grind = play (params (MachineType::Grind, 1.0f, 1.0f), note, 600);
        const int lag = bestLag (grind, ms (100), ms (300), (int) (period * 0.7), (int) (period * 1.3));
        CHECK (std::abs (lag / period - 1.0) < 0.03);
        std::printf ("note %d: hiss %.1f grind %.1f Hz (note %.1f)\n", note, hissHz, fs / lag, hz);

        // the ringing resonator's lowest, loudest mode is the note
        const auto hit = play (params (MachineType::Hit, 0.3f, 1.0f, 0.0f, 1000.0f, 1.0f), note, 600);
        const double hitHz = strongestHz (hit, ms (20), ms (400), hz * 0.85, hz * 1.15);
        CHECK (std::abs (hitHz / hz - 1.0) < 0.01);

        // a still screech, once it has slid up, sounds two octaves above the note
        const auto screech = play (params (MachineType::Screech, 0.5f, 1.0f, 0.0f, 1000.0f, 0.5f), note, 600);
        const double screechHz = strongestHz (screech, ms (200), ms (450), hz * 4.0 * 0.85, hz * 4.0 * 1.15);
        CHECK (std::abs (screechHz / (hz * 4.0) - 1.0) < 0.01);
    }
}

static void lengthSetsHowLongItLasts()
{
    // a hit dies away by itself before its Length; the rest hold until their fade
    for (auto type : { MachineType::Hit, MachineType::Hiss, MachineType::Grind, MachineType::Screech })
        for (float lengthMs : { 100.0f, 1500.0f })
        {
            const auto out = play (params (type, 0.5f, 0.5f, 0.0f, lengthMs, 0.5f), 48, lengthMs + 300.0);
            const double during = rms (out, ms (lengthMs * 0.2), ms (lengthMs * 0.45));
            const double after = rms (out, ms (lengthMs + 50.0), ms (lengthMs + 300.0));
            CHECK (type == MachineType::Hit || during > 0.02);
            CHECK (after < 1.0e-4);
        }
}

static void voicesEndByThemselves()
{
    // a note-off doesn't cut the sound, and with or without one every voice ends
    MachineSynth synth;
    synth.prepare (fs);
    synth.setParams (params (MachineType::Hiss, 0.5f, 0.5f, 0.3f, 800.0f));
    synth.noteOn (48, 1.0f);
    render (synth, ms (50));
    synth.noteOff (48);
    const auto held = render (synth, ms (200));
    CHECK (rms (held, 0, ms (200)) > 0.05);
    for (int note = 40; note < 52; ++note)
        synth.noteOn (note, 0.7f);
    render (synth, ms (1000));
    CHECK (synth.activeVoices() == 0);
}

static void motionMovesTheSound()
{
    // the level of 40 ms windows over a second: still at Motion 0, moving above it
    auto spread = [] (const std::vector<float>& v)
    {
        double lo = 1.0e9, hi = 0.0;
        for (int start = ms (100); start + ms (40) <= ms (1100); start += ms (40))
        {
            const double level = rms (v, start, start + ms (40));
            lo = std::min (lo, level);
            hi = std::max (hi, level);
        }
        return hi / std::max (1.0e-9, lo);
    };
    const auto still = play (params (MachineType::Hiss, 0.5f, 0.0f, 0.0f, 3000.0f), 45, 1200);
    const auto moving = play (params (MachineType::Hiss, 0.5f, 0.0f, 0.3f, 3000.0f), 45, 1200);
    std::printf ("hiss level spread: still %.2f, moving %.2f\n", spread (still), spread (moving));
    CHECK (spread (still) < 1.3);
    CHECK (spread (moving) > 2.0);

    // a moving grind slides its pitch: the period found early and later differ
    const auto grind = play (params (MachineType::Grind, 1.0f, 1.0f, 0.2f, 3000.0f), 45, 1500);
    const double period = fs / noteHz (45);
    const int start = bestLag (grind, ms (60), ms (110), (int) (period * 0.6), (int) (period * 1.1));
    const int later = bestLag (grind, ms (1100), ms (1150), (int) (period * 0.6), (int) (period * 1.1));
    CHECK (later < start - 3);

    // a moving screech chatters: its level stutters where a still one holds
    const auto stillScreech = play (params (MachineType::Screech, 0.5f, 1.0f, 0.0f, 3000.0f), 57, 1200);
    const auto chattering = play (params (MachineType::Screech, 0.5f, 1.0f, 0.4f, 3000.0f), 57, 1200);
    std::printf ("screech level spread: still %.2f, moving %.2f\n", spread (stillScreech), spread (chattering));
    CHECK (spread (stillScreech) < 1.3);
    CHECK (spread (chattering) > 1.6);
}

static void everySettingIsSane()
{
    float loudest = 0.0f, quietest = 1.0e9f;
    for (auto type : { MachineType::Hit, MachineType::Hiss, MachineType::Grind, MachineType::Screech })
        for (float tone : { 0.0f, 0.5f, 1.0f })
            for (float focus : { 0.0f, 0.5f, 1.0f })
                for (float motion : { 0.0f, 1.0f })
                    for (float metal : { 0.0f, 0.5f, 1.0f })
                    for (int note : { 24, 48, 72, 96 })
                    {
                        const auto out = play (params (type, tone, focus, motion, 300.0f, metal), note, 400);
                        bool finite = true;
                        for (float x : out)
                            finite = finite && std::isfinite (x);
                        CHECK (finite);
                        const float pk = peak (out);
                        loudest = std::max (loudest, pk);
                        quietest = std::min (quietest, pk);
                        if (pk > 1.5f || pk < 0.02f)
                            std::printf ("FAIL %s tone %.1f focus %.1f motion %.1f metal %.1f note %d: peak %.3f\n",
                                         kMachineTypeNames[(int) type], tone, focus, motion, metal, note, pk);
                    }
    std::printf ("machine peaks: %.3f .. %.3f\n", quietest, loudest);
    CHECK (loudest < 1.5f);
    CHECK (quietest > 0.02f);
}

// Pitch bend moves the comb and its resonator with it.
static void bendMovesTheNote()
{
    MachineSynth synth;
    synth.prepare (fs);
    synth.setParams (params (MachineType::Grind, 1.0f, 1.0f));
    synth.setPitchBend (-3.0f);
    synth.noteOn (57, 1.0f);
    const auto out = render (synth, ms (600));
    const double period = fs / noteHz (54);
    const int lag = bestLag (out, ms (100), ms (300), (int) (period * 0.7), (int) (period * 1.3));
    CHECK (std::abs (lag / period - 1.0) < 0.03);
}

// The saturation rounds off the loudest hits, so four times the velocity is a little less
// than four times as loud.
static void velocityScalesTheLevel()
{
    const auto soft = play (params (MachineType::Hiss, 0.5f, 0.0f), 48, 300, 0.25f);
    const auto hard = play (params (MachineType::Hiss, 0.5f, 0.0f), 48, 300, 1.0f);
    const double ratio = rms (hard, 0, ms (300)) / rms (soft, 0, ms (300));
    std::printf ("velocity ratio %.3f\n", ratio);
    CHECK (ratio > 3.0 && ratio < 4.05);
}

// With Chaos on pitch, the resonator's modes move a little on every note, so the second mode
// rings at a slightly different pitch on each hit; without Chaos it stays put.
static void chaosVariesTheResonator()
{
    auto spreadOfSecondMode = [] (float chaosPitch)
    {
        MachineSynth synth;
        synth.prepare (fs);
        auto p = params (MachineType::Hit, 0.5f, 1.0f, 0.0f, 1000.0f, 1.0f);
        synth.setParams (p);
        ChaosDepths chaos;
        chaos.pitch = chaosPitch;
        synth.setChaos (chaos);
        // chaos pitch also moves the whole note; measuring the second mode against the first
        // leaves only the spread between the modes
        double lo = 1.0e9, hi = 0.0;
        for (int hit = 0; hit < 6; ++hit)
        {
            synth.noteOn (72, 1.0f);
            const auto out = render (synth, ms (1100));
            const double base = noteHz (72);
            const double first = strongestHz (out, ms (10), ms (700), base * 0.85, base * 1.15);
            const double second = strongestHz (out, ms (10), ms (700), first * 1.40, first * 1.62);
            lo = std::min (lo, second / first);
            hi = std::max (hi, second / first);
        }
        return hi / lo - 1.0;
    };
    const double without = spreadOfSecondMode (0.0f), with = spreadOfSecondMode (1.0f);
    std::printf ("second mode spread: chaos off %.4f, on %.4f\n", without, with);
    CHECK (without < 0.004);
    CHECK (with > 0.006);
}

// A softer note is darker, not only quieter.
static void softerIsDarker()
{
    for (auto type : { MachineType::Hit, MachineType::Hiss, MachineType::Grind, MachineType::Screech })
    {
        const auto p = params (type, 0.5f, 0.5f, 0.0f, 400.0f, 0.5f);
        const double soft = brightness (play (p, 48, 400, 0.3f)), hard = brightness (play (p, 48, 400, 1.0f));
        std::printf ("%s brightness soft %.4f hard %.4f\n", kMachineTypeNames[(int) type], soft, hard);
        CHECK (soft < hard * 0.85);
    }
}

int main()
{
    everyTypeFollowsTheNote();
    bendMovesTheNote();
    lengthSetsHowLongItLasts();
    voicesEndByThemselves();
    motionMovesTheSound();
    everySettingIsSane();
    velocityScalesTheLevel();
    chaosVariesTheResonator();
    softerIsDarker();
    return finish ("machine");
}

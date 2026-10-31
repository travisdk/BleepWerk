// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "DspBasics.h"
#include "VoicePool.h"

#include <array>
#include <iterator>

// The Strike engine: something hits something. Two choices and five knobs.
//
// What hits (the exciter), into a bank of eight tuned two-pole bandpass resonators:
//   Knock     a short pulse with a little noise bite; Tone is how hard: dark is a soft mallet,
//             bright a hard click. Motion adds small knocks while the body rings (a rattle)
//   Noise     a decaying burst of noise, Length long
//   Crackle   sparse clicks for Length
//   Steam     steady noise through a bandpass that sits on the note (Tone lifts it up to four
//             octaves above), opening with a puff; Motion makes it flutter
//   Friction  metal rubbing on metal: a bright tone two octaves up that slides into the note,
//             with friction hiss around it; Motion makes it waver, chatter and jump
// What is hit (the body): Wood (a free bar), Metal (a hard part, between a bar and a plate) or
// Membrane (a drum skin, the zeros of the Bessel functions of a circular membrane).
// Knobs:
//   Length  Noise, Crackle: how long the burst lasts. Steam, Friction: how long the whole sound
//           lasts, fade included (it ends by itself; note-offs don't stop it). Not for Knock
//   Ring    how long the body rings, from a dead knock to seconds. For Steam and Friction it is
//           also how much of the body's colour is mixed into the source
//   Tone    dark to bright: the strike's hardness and how loud the high modes are
//   Motion  see the exciters; Noise and Crackle have none
//   Sweep   the body starts this far below the note (above, when negative) and slides to it:
//           a drip, a drum's fall
// The note sets the fundamental. Chaos moves the body's modes a little on every note. JUCE-free.
namespace bleepforge
{
enum class StrikeExciter { Knock, Noise, Crackle, Steam, Friction };
inline constexpr const char* kStrikeExciterNames[] = { "Knock", "Noise", "Crackle", "Steam", "Friction" };
inline constexpr int kNumStrikeExciters = (int) std::size (kStrikeExciterNames);

enum class StrikeBody { Wood, Metal, Membrane };
inline constexpr const char* kStrikeBodyNames[] = { "Wood", "Metal", "Membrane" };
inline constexpr int kNumStrikeBodies = (int) std::size (kStrikeBodyNames);

struct StrikeParams
{
    StrikeExciter exciter = StrikeExciter::Knock;
    StrikeBody body = StrikeBody::Wood;
    float lengthMs = 300.0f;
    float ring = 0.5f;
    float tone = 0.5f;
    float motion = 0.0f; // 0 still, then 0.1 Hz .. 16 Hz
    float sweepSemis = 0.0f;
};

inline constexpr int kNumStrikeModes = 8;

// Mode frequencies relative to the fundamental, how long the body rings relative to the Ring
// setting, and how much faster the higher modes die out (ring time ~ ratio^-x).
struct BodyModel
{
    std::array<float, kNumStrikeModes> ratios;
    float decayScale;
    float highModeDamping;
};

// A free wooden bar (beam modes), a hard metal part (between a bar and a plate, so it reads as
// metal rather than a bell) and a circular membrane (the Bessel zeros j01, j11, j21, j02, j31,
// j12, j41, j22 divided by j01).
inline constexpr BodyModel kWoodBody { { 1.0f, 2.756f, 5.404f, 8.933f, 13.34f, 18.64f, 24.81f, 31.97f }, 0.35f, 1.3f };
inline constexpr BodyModel kMetalBody { { 1.0f, 1.51f, 2.11f, 2.73f, 3.42f, 4.31f, 5.37f, 6.62f }, 0.5f, 0.7f };
inline constexpr BodyModel kMembraneBody { { 1.0f, 1.594f, 2.136f, 2.296f, 2.653f, 2.918f, 3.156f, 3.500f }, 2.0f, 0.25f };

inline const BodyModel& bodyModel (StrikeBody body)
{
    return body == StrikeBody::Metal ? kMetalBody : body == StrikeBody::Membrane ? kMembraneBody : kWoodBody;
}

// How long the body's lowest mode takes to die away (-60 dB): 20 ms .. 3 s at Ring 0..1, scaled
// by the body.
inline float strikeRingSeconds (const BodyModel& body, float ring)
{
    return 0.02f * std::pow (150.0f, std::clamp (ring, 0.0f, 1.0f)) * body.decayScale;
}

// Motion's rate in Hz; 0 means no movement.
inline float strikeMotionHz (float motion)
{
    return motion <= 0.0f ? 0.0f : 0.1f * std::pow (160.0f, std::clamp (motion, 0.0f, 1.0f));
}

// Steam and Friction are steady sources that Length shapes; the others are one-off strikes.
inline bool strikeIsSteady (StrikeExciter e) { return e == StrikeExciter::Steam || e == StrikeExciter::Friction; }

class StrikeVoice
{
public:
    static constexpr float kLevel = 0.5f;
    static constexpr int kCoefInterval = 16; // samples between filter updates
    static constexpr float kCracklesPerSecond = 1500.0f;
    static constexpr float kMaxModeSpread = 0.015f; // mode detuning at full Chaos pitch depth
    static constexpr float kPuffSeconds = 0.03f;    // Steam's opening puff dies away over this
    static constexpr float kFocus = 0.5f;           // how narrow Steam's band and how clean Friction's tone are
    static constexpr float kScreechSemis = 24.0f;   // Friction sounds this far above the note
    static constexpr float kScreechSlideSemis = -1.0f; // it starts this far below and slides up
    static constexpr float kScreechSlideSeconds = 0.15f;
    static constexpr float kScreechWaver = 0.025f;  // pitch wobble at full Motion, as a fraction

    // Friction's partials (odd and even, so it is bright rather than hollow), at full Tone;
    // the 4th, 6th and 7th make it rasp rather than whistle on low notes
    static constexpr std::array<float, 7> kScreechPartials { 1.0f, 0.5f, 0.35f, 0.32f, 0.15f, 0.24f, 0.2f };
    static constexpr std::array<float, 7> kScreechPartialPhases { 0.0f, 0.3f, 0.0f, 1.1f, 0.0f, 0.0f, 0.5f };
    // the vibration modes a screech jumps between: the note, a fourth and a minor seventh up
    static constexpr std::array<float, 3> kScreechHopRatios { 1.0f, 1.3348f, 1.7818f };

    void prepare (float sampleRate, uint32_t noiseSeed)
    {
        fs = sampleRate;
        bendCoef = bendCoefFor (fs);
        noise.seed (noiseSeed);
        biteHighpassCoef = 1.0f - std::exp (-kTwoPi * 2000.0f / fs);
        waverCoef = 1.0f - std::exp (-kTwoPi * 9.0f / fs);
        jitterCoef = 1.0f - std::exp (-kTwoPi * 25.0f / fs);
        hopCoef = 1.0f - std::exp (-kTwoPi * 60.0f / fs);
        levelDecay = std::exp (-1.0f / (0.02f * fs));
        reset();
    }

    void reset()
    {
        for (auto& m : modes)
            m.y1 = m.y2 = 0.0f;
        x1 = x2 = 0.0f;
        active = false;
        initPending = false;
        steady = false;
        stage = Stage::Idle;
        exciterLeft = 0;
        level = 0.0f;
        env = 0.0f;
        ic1 = ic2 = sc1 = sc2 = 0.0f;
        hissShift = 0.0f;
        dryPower = wetPower = 0.0f;
        wetGain = 1.0f;
        lfoPhase = 0.0f;
        waver = jitter = SlowNoise {};
        note = -1;
    }

    bool isActive() const { return active; }
    // struck and now ringing out, or a steady source in its fade
    bool isReleasing() const { return ! initPending && (steady ? stage == Stage::Fade : exciterLeft <= 0); }
    int getNote() const { return note; }
    float getLevel() const { return steady ? env : level; }

    // Striking a body that still rings keeps its vibration, so a retrigger doesn't click.
    void start (int midiNote, float velocity, bool glide, const Variation& v = {})
    {
        variation = v;
        noise.seed (v.noiseSeed);
        if (! active)
            glide = false;
        note = midiNote;
        targetPitch = (float) midiNote;
        if (! glide)
            currentPitch = targetPitch;
        strength = velocity;
        sweepEnv = 1.0f;
        elapsed = 0;
        lfoPhase = 0.0f; // every hit moves the same way, so a pattern stays hypnotic
        lowpass = biteLowpass = biteHighpass = 0.0f;
        screechPhase = chatterPhase = 0.0f;
        hopMode = 0;
        hopRatio = 1.0f;
        hopCountdown = (int) (0.1f * fs);
        for (auto& m : modes)
            m.spread = noise.next() * kMaxModeSpread * v.modeSpread;
        active = true;
        initPending = true; // what depends on the parameters is set on the first render
        needsCoefs = true;
    }

    void glideTo (int midiNote)
    {
        note = midiNote;
        targetPitch = (float) midiNote;
    }

    // Length or the ring decides when it ends; note-offs don't stop it.
    void release() {}

    // Pitch bend in semitones: a sounding voice slides there, a silent one starts there.
    void setBend (float semis)
    {
        bendTarget = semis;
        if (! isActive())
            bend = semis;
    }

    void render (float* out, int numSamples, const StrikeParams& p, float glideCoef)
    {
        if (! active)
            return;
        if (initPending)
            begin (p);

        // Steady sources: a fade in, a plateau, then a fade to silence that ends at Length
        const bool friction = p.exciter == StrikeExciter::Friction;
        const float lengthSamples = std::max (1.0f, std::clamp (p.lengthMs, 5.0f, 6000.0f) * 0.001f * fs * variation.decayScale);
        const float attackSamples = friction ? std::clamp (0.15f * lengthSamples, 0.01f * fs, 0.08f * fs)
                                             : std::clamp (0.1f * lengthSamples, 0.001f * fs, 0.02f * fs);
        const int holdUntil = (int) (0.5f * lengthSamples);
        const float fadeCoef = std::exp (-9.21f / std::max (1.0f, 0.5f * lengthSamples)); // -80 dB at Length

        const float tone = toneOf (p);
        const float mix = std::clamp (p.ring, 0.0f, 1.0f);
        const float lfoInc = strikeMotionHz (p.motion) / fs;
        const bool moving = lfoInc > 0.0f;
        const float followCoef = 1.0f - std::exp (-1.0f / (0.03f * fs));
        // the strike's tone: a one-pole lowpass from 600 Hz to 16 kHz
        const float lpCoef = 1.0f - std::exp (-kTwoPi * 600.0f * std::exp2 (tone * 4.74f) / fs);
        const float sweepCoef = std::exp (-1.0f / (0.04f * fs)); // 40 ms slide

        for (int i = 0; i < numSamples; ++i)
        {
            if (steady)
            {
                switch (stage)
                {
                    case Stage::Attack:
                        env = std::min (1.0f, env + 1.0f / attackSamples);
                        if (env >= 1.0f)
                            stage = Stage::Hold;
                        break;
                    case Stage::Hold:
                        if (elapsed >= holdUntil)
                            stage = Stage::Fade;
                        break;
                    case Stage::Fade:
                        env *= fadeCoef;
                        break;
                    case Stage::Idle:
                        break;
                }
            }
            ++elapsed;

            currentPitch = targetPitch + (currentPitch - targetPitch) * glideCoef;
            bend += bendCoef * (bendTarget - bend);
            sweepEnv *= sweepCoef;

            // 0 at the start of a note and back at every cycle: moves away from rest and back
            const float lfo = moving ? 0.5f - 0.5f * std::cos (kTwoPi * lfoPhase) : 0.0f;
            lfoPhase = wrapPhase (lfoPhase + lfoInc);

            // Steam: a puff as the valve opens, brighter and louder for the first tens of ms;
            // Motion lowers the band as the pressure drops, and back
            const float puff = strength * std::exp (-(float) elapsed / (kPuffSeconds * fs));
            hissShift = 12.0f * puff - 9.0f * lfo;

            if (needsCoefs || i % kCoefInterval == 0)
                updateCoefs (p, tone);

            if (steady)
            {
                const float dry = friction ? screech (tone, p.motion) : hiss (lfo, puff);
                float wet = resonate (dry);
                // a steady source only rings the modes it overlaps, so the ring is brought to
                // the source's own level, following it over about 30 ms
                dryPower += followCoef * (dry * dry - dryPower);
                wetPower += followCoef * (wet * wet - wetPower);
                const float target = std::clamp (std::sqrt (dryPower / std::max (1.0e-12f, wetPower)), 0.1f, 200.0f);
                wetGain += followCoef * (target - wetGain);
                wet *= wetGain;
                const float s = dry + mix * (wet - dry);
                // light saturation; a harder hit drives it more
                out[i] += std::tanh (1.5f * s * strength) * (1.0f / 1.5f) * env * kLevel;

                if (stage == Stage::Fade && env < Envelope::kSilence)
                {
                    reset();
                    return;
                }
                continue;
            }

            // zeros at DC and Nyquist (x - x[n-2]) make each resonator a bandpass, so the
            // one-sided pulses leave no DC behind
            lowpass += lpCoef * (excite (p, tone) - lowpass);
            const float s = resonate (lowpass) * kLevel;
            out[i] += std::tanh (1.5f * s) * (1.0f / 1.5f);

            // the voice is done once struck and quiet
            level = std::max (level * levelDecay, std::abs (s));
            if (exciterLeft <= 0 && level < 1.0e-4f)
            {
                reset();
                return;
            }
        }
    }

private:
    enum class Stage { Idle, Attack, Hold, Fade };

    // A softer note is darker, not only quieter: it strikes with less tone, a longer touch,
    // and rings a little shorter. Full velocity is as set.
    static constexpr float kSoftDarkening = 0.35f, kSoftTouch = 1.0f, kSoftRing = 0.2f;
    float softness() const { return 1.0f - std::clamp (strength, 0.0f, 1.0f); }

    // the patch's tone moved by this note's Chaos tone and how soft it is
    float toneOf (const StrikeParams& p) const
    {
        return std::clamp (p.tone + 0.5f * variation.tone - kSoftDarkening * softness(), 0.0f, 1.0f);
    }

    // Noise smoothed by two one-poles, so it changes about as often as their cutoff: for
    // wavering and jitter. Scaled to roughly unit spread whatever the rate.
    struct SlowNoise
    {
        float lp1 = 0.0f, lp2 = 0.0f;
        float next (float white, float coef)
        {
            lp1 += coef * (white - lp1);
            lp2 += coef * (lp1 - lp2);
            return lp2 * 2.0f / std::sqrt (coef);
        }
    };

    struct Mode
    {
        float a1 = 0.0f, a2 = 0.0f, gain = 0.0f, spread = 0.0f;
        float y1 = 0.0f, y2 = 0.0f;
    };

    // What depends on the parameters, at the first render of a note.
    void begin (const StrikeParams& p)
    {
        initPending = false;
        steady = strikeIsSteady (p.exciter);
        exciterPos = 0;
        if (steady)
        {
            stage = Stage::Attack; // fades back in from where it is, so a retrigger doesn't click
            exciterLeft = 0;
            dryPower = wetPower = 0.0f;
            wetGain = 1.0f;
            return;
        }
        stage = Stage::Idle;
        // a softer strike is a longer, gentler touch (up to twice as long at velocity 0)
        const float touch = p.exciter == StrikeExciter::Crackle ? 1.0f : 1.0f + kSoftTouch * softness();
        if (p.exciter == StrikeExciter::Knock)
        {
            // Tone sets the pulse: 3 ms (dark, a soft mallet) .. 0.15 ms (bright, a click)
            const float ms = 0.15f * std::pow (20.0f, 1.0f - toneOf (p));
            pulseLength = std::max (1, (int) (ms * touch * 0.001f * fs));
            // a pulse longer than half the note's period would barely sound the note
            pulseLength = std::min (pulseLength, std::max (1, (int) (0.4f * fs / (440.0f * std::exp2 ((targetPitch - 69.0f) / 12.0f)))));
            biteLength = std::max (1, (int) (0.003f * fs));
            exciterLength = exciterLeft = std::max (pulseLength, biteLength);
        }
        else
        {
            exciterLength = exciterLeft = std::max (1, (int) (std::clamp (p.lengthMs, 0.05f, 6000.0f) * touch * 0.001f * fs));
        }
    }

    float pitchHz (float semis) const
    {
        return 440.0f * std::exp2 ((currentPitch + bend + variation.pitchSemis + semis - sweepOffset() - 69.0f) / 12.0f);
    }
    float sweepOffset() const { return sweepSemis * sweepEnv; }

    // The body's coefficients and gains, and Steam's bandpass and Friction's partials. A
    // one-off strike gets each mode at its share of the unit pulse; a steady source gets unity
    // gain at each peak, and render() matches the ring to the source's level.
    void updateCoefs (const StrikeParams& p, float tone)
    {
        needsCoefs = false;
        sweepSemis = p.sweepSemis;
        const bool friction = p.exciter == StrikeExciter::Friction;
        // Friction's body follows the note it sounds, not its wavering, so the squeal swells
        // and fades as it passes the modes
        const float f0 = pitchHz (friction ? kScreechSemis : 0.0f);
        const auto& body = bodyModel (p.body);
        const float baseT60 = strikeRingSeconds (body, p.ring) * variation.decayScale * (1.0f - kSoftRing * softness());
        rattleUntil = (int) std::min (baseT60 * fs, 1.0e9f);
        const float tilt = 1.6f - 1.3f * tone; // high modes quieter when dark

        float total = 0.0f;
        std::array<float, kNumStrikeModes> amps {};
        for (size_t k = 0; k < modes.size(); ++k)
            total += amps[k] = std::pow ((float) k + 1.0f, -tilt);

        for (size_t k = 0; k < modes.size(); ++k)
        {
            auto& m = modes[k];
            const float f = f0 * body.ratios[k] * (1.0f + m.spread);
            if (f >= 0.45f * fs)
            {
                m.gain = 0.0f; // above what this sample rate can play: let it fade, add nothing
                m.a1 = m.a2 = 0.0f;
                continue;
            }
            const float t60 = std::max (0.005f, baseT60 * std::pow (body.ratios[k], -body.highModeDamping));
            const float r = std::exp (-6.91f / (t60 * fs));
            m.a1 = 2.0f * r * std::cos (kTwoPi * f / fs);
            m.a2 = r * r;
            m.gain = (steady ? 1.0f - r : 1.0f) * amps[k] / total;
        }

        if (p.exciter == StrikeExciter::Steam)
        {
            // no higher than 16 kHz: above it a hiss is felt more than heard
            const float centre = std::clamp (pitchHz (48.0f * tone + hissShift), 20.0f, std::min (16000.0f, 0.45f * fs));
            const float q = 0.7f * std::pow (60.0f, kFocus);
            svfG = std::tan (kPi * centre / fs);
            svfK = 1.0f / q;
            // a narrower band lets less noise through; this keeps the level about the same
            svfGain = std::min (120.0f, 0.7f * std::sqrt (fs / (kPi * centre / q)));
        }

        if (friction)
        {
            // Tone rolls the upper partials off when dark; the sum is kept at the same level
            float sumSquares = 0.0f;
            for (size_t k = 0; k < screechAmps.size(); ++k)
            {
                screechAmps[k] = kScreechPartials[k] * std::pow ((float) k + 1.0f, -2.0f * (1.0f - tone) * (1.0f - tone));
                sumSquares += screechAmps[k] * screechAmps[k];
            }
            for (auto& a : screechAmps)
                a /= std::sqrt (sumSquares);
        }
    }

    // Two-pole bandpass modes with zeros at DC and Nyquist (x - x[n-2]).
    float resonate (float x)
    {
        const float in = x - x2;
        x2 = x1;
        x1 = x;
        float s = 0.0f;
        for (auto& m : modes)
        {
            const float y = m.a1 * m.y1 - m.a2 * m.y2 + m.gain * in;
            m.y2 = m.y1;
            m.y1 = y;
            s += y;
        }
        return s;
    }

    // One sample of a one-off strike; each delivers about the same strength to the fundamental.
    float excite (const StrikeParams& p, float tone)
    {
        float x = 0.0f;
        if (exciterLeft > 0)
        {
            const int pos = exciterPos++;
            --exciterLeft;
            switch (p.exciter)
            {
                case StrikeExciter::Knock:
                    // half-sine pulse with unit area: longer is a softer mallet with fewer
                    // overtones, and a few ms of decaying noise, highpassed at 2 kHz and
                    // lowpassed by Tone, adds the bite of a hard hit
                    if (pos < pulseLength)
                        x += strength * std::sin (kPi * (float) pos / (float) pulseLength) * kPi / (2.0f * (float) pulseLength);
                    if (pos < biteLength)
                    {
                        const float lpCoef = 1.0f - std::exp (-kTwoPi * 600.0f * std::exp2 (tone * 4.74f) / fs);
                        biteLowpass += lpCoef * (noise.next() - biteLowpass);
                        biteHighpass += biteHighpassCoef * (biteLowpass - biteHighpass);
                        x += strength * 0.3f * (biteLowpass - biteHighpass) * (1.0f - (float) pos / (float) biteLength);
                    }
                    break;
                case StrikeExciter::Noise:
                    // decaying noise burst with unit energy
                    x = strength * noise.next() * (1.0f - (float) pos / (float) exciterLength) * std::sqrt (3.0f / (float) exciterLength);
                    break;
                case StrikeExciter::Crackle:
                    // sparse impulses, about 1.5 per millisecond, scaled so a longer crackle
                    // isn't louder
                    if (noise.next() > 1.0f - 2.0f * kCracklesPerSecond / fs)
                        x = strength * (0.4f + 0.6f * std::abs (noise.next()))
                          / std::sqrt (std::max (1.0f, kCracklesPerSecond * (float) exciterLength / fs));
                    break;
                case StrikeExciter::Steam:
                case StrikeExciter::Friction:
                    break; // steady sources are not one-off strikes (see render())
            }
        }
        else if (p.exciter == StrikeExciter::Knock && p.motion > 0.0f && elapsed < rattleUntil && 0.5f + 0.5f * noise.next() < 25.0f * p.motion * p.motion / fs)
        {
            // Motion: knocks for as long as the body rings (its ring time, so a rattle can't
            // go on for ever), up to about 25 a second, each a different strength, like a
            // part that rattles
            x = strength * 0.5f * (0.3f + 0.35f * (1.0f + noise.next()));
        }
        return x;
    }

    // White noise through a bandpass. Tone lifts the centre from the note up to four octaves
    // above it, since steam hisses high. Every note opens with a puff, up to twice as loud and
    // an octave higher. Motion makes the hiss flutter: the level dips deeply and the band drops.
    float hiss (float lfo, float puff)
    {
        const float x = noise.next();
        const float a1 = 1.0f / (1.0f + svfG * (svfG + svfK)), a2 = svfG * a1, a3 = svfG * a2;
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return v1 * svfK * svfGain * (1.0f - 0.85f * lfo) * (1.0f + puff);
    }

    // Friction: metal sticks and slips against metal. A bright tone two octaves above the note
    // slides up into it over 150 ms (the part straining), with friction hiss around it, driven
    // so it rasps instead of whistling: harder as Tone goes up, cleaner when dark. Motion makes
    // the pitch waver and the level chatter, from a slow stutter to 30 times a second; above
    // half way the screech jumps to another mode now and then (a fourth or a seventh up) and
    // back, like a brake squeal. Motion 0 holds it still.
    float screech (float tone, float motion)
    {
        const float t = (float) elapsed / fs;
        const float x = std::min (1.0f, t / kScreechSlideSeconds);
        const float slide = kScreechSlideSemis * (1.0f - x * x * (3.0f - 2.0f * x));

        // a jump away from the note goes back to it at the next one
        if (motion <= 0.5f)
            hopMode = 0;
        else if (--hopCountdown <= 0)
        {
            hopMode = hopMode != 0 ? 0 : (noise.next() < 0.0f ? 1 : 2);
            const float rate = std::max (0.05f, 2.0f * (motion - 0.5f));
            hopCountdown = (int) ((0.08f + 0.11f * (1.0f + noise.next())) * fs / rate);
        }
        hopRatio += hopCoef * (kScreechHopRatios[(size_t) hopMode] - hopRatio); // a jump slides over a few ms

        const float f = pitchHz (kScreechSemis + slide) * hopRatio * (1.0f + kScreechWaver * motion * waver.next (noise.next(), waverCoef));
        screechPhase = wrapPhase (screechPhase + std::min (0.45f, f / fs));
        const float w = kTwoPi * screechPhase;
        float partials = 0.0f;
        for (size_t k = 0; k < screechAmps.size(); ++k)
            if ((float) (k + 1) * f < 0.45f * fs)
                partials += screechAmps[k] * std::sin ((float) (k + 1) * w + kScreechPartialPhases[k]);

        // the friction hiss: noise through a bandpass (Q 6) from the tone (dark) up to two
        // octaves above it (bright), kept at about the same level whatever the pitch
        if (elapsed % kCoefInterval == 1)
        {
            const float centre = std::clamp (f * (1.0f + 3.0f * tone), 20.0f, 0.45f * fs);
            screechG = std::tan (kPi * centre / fs);
            screechHissGain = 0.7f * std::sqrt (fs / (kPi * centre / 6.0f));
        }
        const float k = 1.0f / 6.0f;
        const float a1 = 1.0f / (1.0f + screechG * (screechG + k)), a2 = screechG * a1, a3 = screechG * a2;
        const float v3 = noise.next() - sc2;
        const float v1 = a1 * sc1 + a2 * v3;
        const float v2 = sc2 + a2 * sc1 + a3 * v3;
        sc1 = 2.0f * v1 - sc1;
        sc2 = 2.0f * v2 - sc2;
        const float hissy = v1 * k * screechHissGain;

        // stick-slip chatter: the level stutters, unevenly; it takes about half the level away
        // at full Motion, which is given back
        const float wobble = jitter.next (noise.next(), jitterCoef);
        chatterPhase = wrapPhase (chatterPhase + (5.0f + 25.0f * motion) * (1.0f + 0.2f * wobble) / fs);
        const float depth = std::min (1.0f, 3.0f * motion);
        const float stick = std::pow (0.5f + 0.5f * std::sin (kTwoPi * chatterPhase), 3.0f);
        const float chatter = (1.0f - 0.85f * depth * (1.0f - stick)) / (1.0f - 0.58f * depth);
        const float swell = std::max (0.0f, 1.0f + 0.25f * motion * wobble);

        // the chatter comes after the drive, which would otherwise flatten it
        const float rough = 0.7f * (1.0f - kFocus);
        const float drive = 1.0f + 4.0f * tone;
        return 0.8f * std::tanh (drive * ((1.0f - rough) * partials + rough * hissy)) / std::tanh (drive) * chatter * swell;
    }

    float fs = 48000.0f;
    WhiteNoise noise;
    std::array<Mode, kNumStrikeModes> modes {}; // the body
    bool active = false, initPending = false, steady = false, needsCoefs = true;
    Stage stage = Stage::Idle;
    int elapsed = 0, rattleUntil = 0, exciterPos = 0, exciterLeft = 0, exciterLength = 1, pulseLength = 1, biteLength = 1;
    float strength = 1.0f, env = 0.0f, level = 0.0f, levelDecay = 0.999f;
    float lowpass = 0.0f, x1 = 0.0f, x2 = 0.0f;
    float sweepEnv = 0.0f, sweepSemis = 0.0f, lfoPhase = 0.0f;
    float currentPitch = 60.0f, targetPitch = 60.0f;
    float bend = 0.0f, bendTarget = 0.0f, bendCoef = 0.1f;
    float dryPower = 0.0f, wetPower = 0.0f, wetGain = 1.0f; // the ring matched to a steady source
    float biteLowpass = 0.0f, biteHighpass = 0.0f, biteHighpassCoef = 0.2f; // Knock
    float ic1 = 0.0f, ic2 = 0.0f, svfG = 0.1f, svfK = 1.0f, svfGain = 1.0f; // Steam
    float hissShift = 0.0f; // semitones the puff and Motion move Steam's band
    std::array<float, kScreechPartials.size()> screechAmps {}; // Friction
    float screechPhase = 0.0f, chatterPhase = 0.0f, hopRatio = 1.0f;
    int hopMode = 0, hopCountdown = 0;
    float sc1 = 0.0f, sc2 = 0.0f, screechG = 0.1f, screechHissGain = 1.0f;
    float waverCoef = 0.01f, jitterCoef = 0.01f, hopCoef = 0.01f;
    SlowNoise waver, jitter;
    Variation variation;
    int note = -1;
};

using StrikeSynth = VoicePool<StrikeVoice, StrikeParams>;
} // namespace bleepforge

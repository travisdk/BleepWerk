// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "DspBasics.h"
#include "VoicePool.h"

#include <array>
#include <iterator>

// The Foley engine: modal synthesis. A short exciter (a noise burst, a mallet-like pulse or
// sparse crackles) strikes a bank of tuned two-pole bandpass resonators, one per vibration
// mode of an object. Four controls shape the object:
//   Material   wood -> metal -> glass: where the overtones sit and how fast they die out
//   Size       bigger is lower and rings longer
//   Damping    how long the object rings
//   Brightness how hard it is struck: the exciter's tone and how loud the high modes are
// A pitch sweep (the modes slide into the note) gives drips. The note sets the fundamental.
// JUCE-free.
namespace bleepforge
{
enum class FoleyExciter { Noise, Mallet, Crackle };
inline constexpr const char* kFoleyExciterNames[] = { "Noise", "Mallet", "Crackle" };
inline constexpr int kNumFoleyExciters = (int) std::size (kFoleyExciterNames);

struct FoleyParams
{
    FoleyExciter exciter = FoleyExciter::Mallet;
    float exciterMs = 0.3f;    // burst, pulse or crackle length
    float material = 0.0f;     // 0 wood, 0.5 metal, 1 glass
    float size = 0.5f;         // 0 small .. 1 big; 0.5 plays the note as it is
    float damping = 0.5f;      // 0 rings for seconds .. 1 a dead knock
    float brightness = 0.5f;
    float sweepSemis = 0.0f;   // the modes start this far below the note and slide up to it
};

inline constexpr int kNumModes = 6;

// Mode frequencies relative to the fundamental, how long the object rings relative to the
// Damping setting, and how much faster the higher modes die out (decay time ~ ratio^-x).
struct MaterialModel
{
    std::array<float, kNumModes> ratios;
    float decayScale;
    float highModeDamping;
};

// A free wooden bar (beam modes), a metal plate (circular plate modes), a glass (thin shell).
inline constexpr MaterialModel kWood { { 1.0f, 2.756f, 5.404f, 8.933f, 13.34f, 18.64f }, 0.35f, 1.3f };
inline constexpr MaterialModel kMetal { { 1.0f, 1.594f, 2.136f, 2.296f, 2.653f, 2.918f }, 2.0f, 0.25f };
inline constexpr MaterialModel kGlass { { 1.0f, 2.32f, 4.25f, 6.63f, 9.38f, 12.4f }, 1.2f, 0.6f };

// Material 0..1 blends wood into metal into glass; ratios and decay blend on a log scale.
inline MaterialModel blendMaterial (float material)
{
    const float m = std::clamp (material, 0.0f, 1.0f);
    const auto& a = m < 0.5f ? kWood : kMetal;
    const auto& b = m < 0.5f ? kMetal : kGlass;
    const float t = m < 0.5f ? m * 2.0f : (m - 0.5f) * 2.0f;
    auto logBlend = [t] (float x, float y) { return std::exp (std::log (x) + t * (std::log (y) - std::log (x))); };

    MaterialModel out {};
    for (size_t k = 0; k < (size_t) kNumModes; ++k)
        out.ratios[k] = logBlend (a.ratios[k], b.ratios[k]);
    out.decayScale = logBlend (a.decayScale, b.decayScale);
    out.highModeDamping = a.highModeDamping + t * (b.highModeDamping - a.highModeDamping);
    return out;
}

class FoleyVoice
{
public:
    static constexpr float kLevel = 0.5f;
    static constexpr int kCoefInterval = 16; // samples between coefficient updates while sliding
    static constexpr float kCracklesPerSecond = 1500.0f;

    void prepare (float sampleRate, uint32_t noiseSeed)
    {
        fs = sampleRate;
        bendCoef = bendCoefFor (fs);
        noise.seed (noiseSeed);
        reset();
    }

    void reset()
    {
        for (auto& m : modes)
            m.y1 = m.y2 = 0.0f;
        x1 = x2 = 0.0f;
        active = false;
        exciterLeft = 0;
        level = 0.0f;
        note = -1;
    }

    bool isActive() const { return active; }
    bool isReleasing() const { return exciterLeft <= 0; } // struck, now ringing out
    int getNote() const { return note; }
    float getLevel() const { return level; }

    // Striking an object that still rings keeps its vibration, so a retrigger doesn't click.
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
        exciterPos = 0;
        exciterLeft = -1; // set from the parameters on the first render
        lowpass = 0.0f;
        active = true;
        needsCoefs = true;
    }

    void glideTo (int midiNote)
    {
        note = midiNote;
        targetPitch = (float) midiNote;
    }

    // A struck object rings out by itself; note-offs don't stop it.
    void release() {}

    // Pitch bend in semitones: a sounding voice slides there, a silent one starts there.
    void setBend (float semis)
    {
        bendTarget = semis;
        if (! isActive())
            bend = semis;
    }

    void render (float* out, int numSamples, const FoleyParams& p, float glideCoef)
    {
        if (! active)
            return;

        if (exciterLeft < 0)
        {
            // a softer strike is a longer, gentler touch (up to twice as long at velocity 0)
            const float touch = p.exciter == FoleyExciter::Crackle ? 1.0f : 1.0f + kSoftTouch * softness();
            int length = std::max (1, (int) (p.exciterMs * touch * 0.001f * fs));
            // a mallet pulse longer than half the note's period would barely sound the note
            if (p.exciter == FoleyExciter::Mallet)
                length = std::min (length, std::max (1, (int) (0.4f * fs / (440.0f * std::exp2 ((targetPitch - 69.0f) / 12.0f)))));
            exciterLength = exciterLeft = length;
        }

        // the exciter's tone: a one-pole lowpass from 600 Hz to 16 kHz
        const float cutoff = 600.0f * std::exp2 (brightnessOf (p) * 4.74f);
        const float lpCoef = 1.0f - std::exp (-kTwoPi * cutoff / fs);
        const float sweepCoef = std::exp (-1.0f / (0.04f * fs)); // 40 ms slide

        for (int i = 0; i < numSamples; ++i)
        {
            const bool sliding = std::abs (currentPitch - targetPitch) > 1.0e-3f || std::abs (bend - bendTarget) > 1.0e-3f
                              || (std::abs (p.sweepSemis) > 1.0e-3f && sweepEnv > 1.0e-3f);
            if (needsCoefs || (sliding && i % kCoefInterval == 0))
                updateCoefs (p);
            currentPitch = targetPitch + (currentPitch - targetPitch) * glideCoef;
            bend += bendCoef * (bendTarget - bend);
            sweepEnv *= sweepCoef;

            // zeros at DC and Nyquist (x - x[n-2]) make each resonator a bandpass, so the
            // one-sided mallet and crackle pulses leave no DC behind
            lowpass += lpCoef * (excite (p) - lowpass);
            const float x = lowpass - x2;
            x2 = x1;
            x1 = lowpass;
            float s = 0.0f;
            for (auto& m : modes)
            {
                const float y = m.a1 * m.y1 - m.a2 * m.y2 + m.gain * x;
                m.y2 = m.y1;
                m.y1 = y;
                s += y;
            }
            s *= kLevel;
            out[i] += s;

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
    // A softer note is darker, not only quieter: it strikes with less brightness, a longer
    // touch, and rings a little shorter. Full velocity is as set.
    static constexpr float kSoftDarkening = 0.35f, kSoftTouch = 1.0f, kSoftRing = 0.2f;
    float softness() const { return 1.0f - std::clamp (strength, 0.0f, 1.0f); }

    // the patch's brightness moved by this note's Chaos tone and how soft it is
    float brightnessOf (const FoleyParams& p) const
    {
        return std::clamp (p.brightness + 0.5f * variation.tone - kSoftDarkening * softness(), 0.0f, 1.0f);
    }

    struct Mode
    {
        float a1 = 0.0f, a2 = 0.0f, gain = 0.0f;
        float y1 = 0.0f, y2 = 0.0f;
    };

    // One exciter sample; each exciter delivers about the same strength to the fundamental.
    float excite (const FoleyParams& p)
    {
        if (exciterLeft <= 0)
            return 0.0f;
        const float t = (float) exciterPos / (float) exciterLength;
        ++exciterPos;
        --exciterLeft;

        switch (p.exciter)
        {
            case FoleyExciter::Noise:
                // decaying noise burst with unit energy
                return strength * noise.next() * (1.0f - t) * std::sqrt (3.0f / (float) exciterLength);
            case FoleyExciter::Mallet:
                // half-sine pulse with unit area: longer is a softer mallet with fewer overtones
                return strength * std::sin (kPi * t) * kPi / (2.0f * (float) exciterLength);
            case FoleyExciter::Crackle:
                // sparse impulses, about 1.5 per millisecond, scaled so a longer crackle
                // isn't louder
                if (noise.next() > 1.0f - 2.0f * kCracklesPerSecond / fs)
                    return strength * (0.4f + 0.6f * std::abs (noise.next()))
                         / std::sqrt (std::max (1.0f, kCracklesPerSecond * (float) exciterLength / fs));
                return 0.0f;
        }
        return 0.0f;
    }

    void updateCoefs (const FoleyParams& p)
    {
        needsCoefs = false;
        const auto material = blendMaterial (p.material);
        const float size = std::clamp (p.size, 0.0f, 1.0f);
        const float pitch = currentPitch + bend + variation.pitchSemis + 12.0f * (1.0f - 2.0f * size) - p.sweepSemis * sweepEnv;
        const float f0 = 440.0f * std::exp2 ((pitch - 69.0f) / 12.0f);

        // ring time of the fundamental: 3 s .. 20 ms, longer for big objects and metal
        const float baseT60 = std::exp (std::log (3.0f) + std::clamp (p.damping, 0.0f, 1.0f) * (std::log (0.02f) - std::log (3.0f)))
                            * material.decayScale * std::exp2 (2.0f * size - 1.0f) * variation.decayScale
                            * (1.0f - kSoftRing * softness());
        const float tilt = 1.6f - 1.3f * brightnessOf (p); // high modes quieter when dark

        float total = 0.0f;
        std::array<float, kNumModes> amps {};
        for (size_t k = 0; k < (size_t) kNumModes; ++k)
            total += amps[k] = std::pow ((float) k + 1.0f, -tilt);

        for (size_t k = 0; k < (size_t) kNumModes; ++k)
        {
            auto& m = modes[k];
            const float f = f0 * material.ratios[k];
            if (f >= 0.45f * fs)
            {
                m.gain = 0.0f; // above what this sample rate can play: let it fade, add nothing
                m.a1 = m.a2 = 0.0f;
                continue;
            }
            const float w = kTwoPi * f / fs;
            const float t60 = std::max (0.005f, baseT60 * std::pow (material.ratios[k], -material.highModeDamping));
            const float r = std::exp (-6.91f / (t60 * fs));
            m.a1 = 2.0f * r * std::cos (w);
            m.a2 = r * r;
            m.gain = amps[k] / total; // a bandpass resonator rings at about twice its gain per unit impulse
        }
        levelDecay = std::exp (-1.0f / (0.02f * fs));
    }

    float fs = 48000.0f;
    std::array<Mode, kNumModes> modes {};
    WhiteNoise noise;
    bool active = false, needsCoefs = true;
    int exciterPos = 0, exciterLeft = 0, exciterLength = 1;
    float strength = 1.0f, lowpass = 0.0f, x1 = 0.0f, x2 = 0.0f, sweepEnv = 0.0f, level = 0.0f, levelDecay = 0.999f;
    float currentPitch = 60.0f, targetPitch = 60.0f;
    float bend = 0.0f, bendTarget = 0.0f, bendCoef = 0.1f;
    Variation variation;
    int note = -1;
};

using FoleySynth = VoicePool<FoleyVoice, FoleyParams>;
} // namespace bleepforge

// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "DspBasics.h"
#include "VoicePool.h"

#include <array>
#include <iterator>
#include <vector>

// The Machine engine: industrial sounds that last, tuned to the note so they play melodically
// in the grid. Three types:
//   Hiss   steam: noise through a bandpass that sits on the note (or octaves above it)
//   Hum    a motor or transformer: a deep tone, from a pure sine to a buzz, with a rasp
//   Grind  noise through a comb filter tuned to the note, which slides when it moves
// Four controls, the same for every type:
//   Length how long the sound lasts, fade included (a voice ends by itself, note-offs don't
//          stop it, so a short step can start a hum that lasts for bars)
//   Tone   dark to bright
//   Motion how fast it moves: 0 is still, then a slow wave up to a fast rattle. It pulses the
//          level (Hum), sweeps the comb (Grind) or makes the hiss flutter (Hiss)
//   Focus  from wide, airy noise to a clear tone
// JUCE-free.
namespace bleepforge
{
enum class MachineType { Hiss, Hum, Grind };
inline constexpr const char* kMachineTypeNames[] = { "Hiss", "Hum", "Grind" };
inline constexpr int kNumMachineTypes = (int) std::size (kMachineTypeNames);

struct MachineParams
{
    MachineType type = MachineType::Hum;
    float lengthMs = 400.0f;
    float tone = 0.5f;
    float motion = 0.0f; // 0 still, then 0.1 Hz .. 16 Hz
    float focus = 0.5f;
};

// Motion's rate in Hz; 0 means no movement.
inline float machineMotionHz (float motion)
{
    return motion <= 0.0f ? 0.0f : 0.1f * std::pow (160.0f, std::clamp (motion, 0.0f, 1.0f));
}

class MachineVoice
{
public:
    static constexpr float kLevel = 0.5f;
    static constexpr int kCoefInterval = 16; // samples between filter updates
    static constexpr float kLowestHz = 20.0f; // the comb's longest delay

    // The comb's delay line is sized here, off the audio thread.
    void prepare (float sampleRate, uint32_t noiseSeed)
    {
        fs = sampleRate;
        noise.seed (noiseSeed);
        comb.assign ((size_t) (fs / kLowestHz) + 4, 0.0f);
        reset();
    }

    void reset()
    {
        std::fill (comb.begin(), comb.end(), 0.0f);
        stage = Stage::Idle;
        env = 0.0f;
        ic1 = ic2 = combLowpass = noiseLowpass = 0.0f;
        phase = lfoPhase = 0.0f;
        note = -1;
    }

    bool isActive() const { return stage != Stage::Idle; }
    bool isReleasing() const { return stage == Stage::Fade; }
    int getNote() const { return note; }
    float getLevel() const { return env; }

    // A voice that still sounds fades back in from where it is and keeps its phase and comb,
    // so a retrigger doesn't click.
    void start (int midiNote, float velocity, bool glide, const Variation& v = {})
    {
        variation = v;
        if (stage == Stage::Idle)
        {
            glide = false;
            phase = 0.0f;
        }
        note = midiNote;
        targetPitch = (float) midiNote;
        if (! glide)
            currentPitch = targetPitch;
        gain = velocity;
        lfoPhase = 0.0f; // every hit moves the same way, so a pattern stays hypnotic
        stage = Stage::Attack;
        elapsed = 0;
        needsCoefs = true;
    }

    void glideTo (int midiNote)
    {
        note = midiNote;
        targetPitch = (float) midiNote;
    }

    // Length decides when it ends; note-offs don't stop it.
    void release() {}

    void render (float* out, int numSamples, const MachineParams& p, float glideCoef)
    {
        if (stage == Stage::Idle)
            return;

        // Length: a short fade in, a plateau, then a fade to silence that ends at Length
        const float lengthSamples = std::max (1.0f, std::clamp (p.lengthMs, 5.0f, 6000.0f) * 0.001f * fs * variation.decayScale);
        const float attackSamples = std::clamp (0.1f * lengthSamples, 0.001f * fs, 0.02f * fs);
        const int holdUntil = (int) (0.5f * lengthSamples);
        const float fadeCoef = std::exp (-9.21f / std::max (1.0f, 0.5f * lengthSamples)); // -80 dB at Length

        const float tone = std::clamp (p.tone + 0.5f * variation.tone, 0.0f, 1.0f);
        const float focus = std::clamp (p.focus, 0.0f, 1.0f);
        const float lfoInc = machineMotionHz (p.motion) / fs;
        const bool moving = lfoInc > 0.0f;

        for (int i = 0; i < numSamples; ++i)
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
            ++elapsed;

            currentPitch = targetPitch + (currentPitch - targetPitch) * glideCoef;
            // 0 at the start of a note and back at every cycle: moves away from rest and back
            const float lfo = moving ? 0.5f - 0.5f * std::cos (kTwoPi * lfoPhase) : 0.0f;
            lfoPhase = wrapPhase (lfoPhase + lfoInc);

            float s = 0.0f;
            switch (p.type)
            {
                case MachineType::Hiss: s = hiss (tone, focus, lfo, i); break;
                case MachineType::Hum: s = hum (tone, focus, lfo); break;
                case MachineType::Grind: s = grind (tone, focus, lfo); break;
            }
            out[i] += s * gain * env * kLevel;

            if (stage == Stage::Fade && env < Envelope::kSilence)
            {
                stage = Stage::Idle;
                env = 0.0f;
                note = -1;
                return;
            }
        }
    }

private:
    enum class Stage { Idle, Attack, Hold, Fade };

    float pitchHz (float semis) const
    {
        return 440.0f * std::exp2 ((currentPitch + variation.pitchSemis + semis - 69.0f) / 12.0f);
    }

    // White noise through a bandpass. Tone lifts the centre from the note up to four octaves
    // above it, since steam hisses high; Focus narrows it from about an octave wide to a
    // whistle. Motion makes the hiss flutter: the centre wobbles and the level dips.
    float hiss (float tone, float focus, float lfo, int i)
    {
        if (needsCoefs || i % kCoefInterval == 0)
        {
            needsCoefs = false;
            const float centre = std::clamp (pitchHz (48.0f * tone - 6.0f * lfo), 20.0f, 0.45f * fs);
            const float q = 0.7f * std::pow (60.0f, focus);
            svfG = std::tan (kPi * centre / fs);
            svfK = 1.0f / q;
            // a narrower band lets less noise through; this keeps the level about the same
            const float bandwidth = centre / q;
            svfGain = std::min (120.0f, 0.7f * std::sqrt (fs / (kPi * bandwidth)));
        }
        const float x = noise.next();
        const float a1 = 1.0f / (1.0f + svfG * (svfG + svfK)), a2 = svfG * a1, a3 = svfG * a2;
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return v1 * svfK * svfGain * (1.0f - 0.6f * lfo);
    }

    // A sine on the note, driven into a buzz by Tone (less on high notes, where the overtones
    // would alias). Below full Focus a rasp joins in: noise chopped by the tone's own cycle.
    // Motion pulses the level, like a motor that labours.
    float hum (float tone, float focus, float lfo)
    {
        const float f = pitchHz (0.0f);
        const float inc = std::min (0.45f, f / fs);
        const float sine = std::sin (kTwoPi * phase);
        phase = wrapPhase (phase + inc);

        const float drive = 1.0f + 8.0f * tone * std::min (1.0f, 150.0f / f);
        float s = std::tanh (drive * sine) / std::tanh (drive);

        const float lpCoef = 1.0f - std::exp (-kTwoPi * std::min (0.45f * fs, 4.0f * f) / fs);
        noiseLowpass += lpCoef * (noise.next() - noiseLowpass);
        // a one-pole lowpass passes a/(2-a) of white noise's power; this keeps the rasp as loud
        // on low notes as on high ones
        const float rasp = noiseLowpass * (1.0f + sine) * std::sqrt ((2.0f - lpCoef) / lpCoef);
        s += (1.0f - focus) * (rasp - s);
        return 0.55f * s * (1.0f - 0.8f * lfo);
    }

    // Noise into a feedback comb whose delay is one period of the note; Focus is the feedback,
    // from a faint colour to a ringing tone, and Tone the lowpass in the loop. Motion slides
    // the comb up to a fifth up and back, the sound of a grinder biting.
    float grind (float tone, float focus, float lfo)
    {
        const float f = std::max (kLowestHz, pitchHz (7.0f * lfo));
        const float delay = std::min ((float) comb.size() - 2.0f, fs / f);
        const float feedback = 0.3f + 0.68f * focus;
        const float lpCoef = 1.0f - std::exp (-kTwoPi * 600.0f * std::exp2 (tone * 4.74f) / fs);

        // linear interpolation between the two samples around the delay
        float readPos = (float) combWrite - delay;
        if (readPos < 0.0f)
            readPos += (float) comb.size();
        const auto i0 = (size_t) readPos;
        const auto i1 = (i0 + 1) % comb.size();
        const float frac = readPos - (float) i0;
        const float delayed = comb[i0] + frac * (comb[i1] - comb[i0]);

        combLowpass += lpCoef * (delayed - combLowpass);
        const float x = noise.next() * 0.5f;
        const float y = x + feedback * combLowpass;
        comb[combWrite] = y;
        combWrite = (combWrite + 1) % comb.size();
        // the lowpass in the loop takes most of the energy the feedback adds, so the level
        // stays about the same without correcting for it
        return y * 1.2f;
    }

    float fs = 48000.0f;
    WhiteNoise noise;
    Stage stage = Stage::Idle;
    bool needsCoefs = true;
    int elapsed = 0;
    float env = 0.0f, gain = 1.0f;
    float currentPitch = 60.0f, targetPitch = 60.0f, phase = 0.0f, lfoPhase = 0.0f;
    float ic1 = 0.0f, ic2 = 0.0f, svfG = 0.1f, svfK = 1.0f, svfGain = 1.0f; // Hiss
    float noiseLowpass = 0.0f;                                               // Hum
    std::vector<float> comb;                                                 // Grind
    size_t combWrite = 0;
    float combLowpass = 0.0f;
    Variation variation;
    int note = -1;
};

using MachineSynth = VoicePool<MachineVoice, MachineParams>;
} // namespace bleepforge

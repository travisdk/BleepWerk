// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "DspBasics.h"
#include "VoicePool.h"

#include <array>
#include <iterator>
#include <vector>

// The Machine engine: industrial sounds, tuned to the note so they play melodically. Every type is a source into a metal resonator (8 sharp bandpasses on inharmonic
// ratios that follow the note), then a light saturation:
//   Hit    a click and a burst of noise striking the resonator, with a deep rumble and a short
//          hiss layered on: a heavy industrial hit
//   Hiss   steam: noise through a bandpass that sits on the note (or octaves above it)
//   Hum    a motor or transformer: a deep tone, from a pure sine to a buzz, with a rasp
//   Grind  noise through a comb filter tuned to the note, which slides when it moves
// Five controls, the same for every type:
//   Length how long the sound lasts, fade included (a voice ends by itself, note-offs don't
//          stop it, so a short step can start a hum that lasts for bars)
//   Tone   dark to bright
//   Motion how fast it moves: 0 is still, then a slow wave up to a fast rattle. It pulses the
//          level (Hum), sweeps the comb (Grind), makes the hiss flutter (Hiss) or rattles the
//          struck part with small knocks (Hit)
//   Focus  from wide, airy noise to a clear tone; on Hit, how much rumble and hiss there is
//   Metal  how much the resonator rings: 0 the dry source, 1 ringing metal for about a second
// Chaos moves the resonator's modes a little on every note, so no two hits ring the same.
// JUCE-free.
namespace bleepforge
{
enum class MachineType { Hit, Hiss, Hum, Grind };
inline constexpr const char* kMachineTypeNames[] = { "Hit", "Hiss", "Hum", "Grind" };
inline constexpr int kNumMachineTypes = (int) std::size (kMachineTypeNames);

struct MachineParams
{
    MachineType type = MachineType::Hit;
    float lengthMs = 400.0f;
    float tone = 0.5f;
    float motion = 0.0f; // 0 still, then 0.1 Hz .. 16 Hz
    float focus = 0.5f;
    float metal = 0.5f;
};

// Motion's rate in Hz; 0 means no movement.
inline float machineMotionHz (float motion)
{
    return motion <= 0.0f ? 0.0f : 0.1f * std::pow (160.0f, std::clamp (motion, 0.0f, 1.0f));
}

// The resonator's modes relative to the note: between a free bar and a plate, so it reads as
// a hard metal part rather than a bell.
inline constexpr std::array<float, 8> kMachineModeRatios { 1.0f, 1.51f, 2.11f, 2.73f, 3.42f, 4.31f, 5.37f, 6.62f };

// How long the resonator's lowest mode rings at a Metal setting: 80 ms .. 1.2 s.
inline float machineRingSeconds (float metal)
{
    return 0.08f * std::pow (15.0f, std::clamp (metal, 0.0f, 1.0f));
}

class MachineVoice
{
public:
    static constexpr float kLevel = 0.5f;
    static constexpr int kCoefInterval = 16; // samples between filter updates
    static constexpr float kLowestHz = 20.0f; // the comb's longest delay
    static constexpr float kMaxModeSpread = 0.015f; // mode detuning at full Chaos pitch depth
    static constexpr float kHitRing = 1.0f;         // how loud a struck mode rings

    // The comb's delay line is sized here, off the audio thread.
    void prepare (float sampleRate, uint32_t noiseSeed)
    {
        fs = sampleRate;
        bendCoef = bendCoefFor (fs);
        noise.seed (noiseSeed);
        comb.assign ((size_t) (fs / kLowestHz) + 4, 0.0f);
        // Hit's hiss layer: a fixed bandpass around 7 kHz, about two octaves wide
        const float g = std::tan (kPi * std::min (7000.0f, 0.4f * fs) / fs);
        airK = 1.0f / 1.2f;
        airA1 = 1.0f / (1.0f + g * (g + airK));
        airA2 = g * airA1;
        airA3 = g * airA2;
        reset();
    }

    void reset()
    {
        std::fill (comb.begin(), comb.end(), 0.0f);
        for (auto& m : modes)
            m.y1 = m.y2 = 0.0f;
        x1 = x2 = 0.0f;
        stage = Stage::Idle;
        env = 0.0f;
        ic1 = ic2 = combLowpass = noiseLowpass = 0.0f;
        air1 = air2 = rumble1 = rumble2 = 0.0f;
        dryPower = wetPower = 0.0f;
        wetGain = 1.0f;
        phase = lfoPhase = subPhase = 0.0f;
        note = -1;
    }

    bool isActive() const { return stage != Stage::Idle; }
    bool isReleasing() const { return stage == Stage::Fade; }
    int getNote() const { return note; }
    float getLevel() const { return env; }

    // A voice that still sounds fades back in from where it is and keeps its phase, comb and
    // ringing modes, so a retrigger doesn't click.
    void start (int midiNote, float velocity, bool glide, const Variation& v = {})
    {
        variation = v;
        noise.seed (v.noiseSeed);
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
        struck = 0;
        hitLowpass = 0.0f;
        for (auto& m : modes)
            m.spread = noise.next() * kMaxModeSpread * v.modeSpread;
        needsCoefs = true;
    }

    void glideTo (int midiNote)
    {
        note = midiNote;
        targetPitch = (float) midiNote;
    }

    // Length decides when it ends; note-offs don't stop it.
    void release() {}

    // Pitch bend in semitones: a sounding voice slides there, a silent one starts there.
    void setBend (float semis)
    {
        bendTarget = semis;
        if (! isActive())
            bend = semis;
    }

    void render (float* out, int numSamples, const MachineParams& p, float glideCoef)
    {
        if (stage == Stage::Idle)
            return;

        // Length: a fade in (none for a hit, which must click), a plateau, then a fade to
        // silence that ends at Length
        const bool hit = p.type == MachineType::Hit;
        const float lengthSamples = std::max (1.0f, std::clamp (p.lengthMs, 5.0f, 6000.0f) * 0.001f * fs * variation.decayScale);
        const float attackSamples = hit ? 1.0f : std::clamp (0.1f * lengthSamples, 0.001f * fs, 0.02f * fs);
        const int holdUntil = (int) (0.5f * lengthSamples);
        const float fadeCoef = std::exp (-9.21f / std::max (1.0f, 0.5f * lengthSamples)); // -80 dB at Length

        const float tone = std::clamp (p.tone + 0.5f * variation.tone, 0.0f, 1.0f);
        const float focus = std::clamp (p.focus, 0.0f, 1.0f);
        const float metal = std::clamp (p.metal, 0.0f, 1.0f);
        const float lfoInc = machineMotionHz (p.motion) / fs;
        const bool moving = lfoInc > 0.0f;
        const float followCoef = 1.0f - std::exp (-1.0f / (0.03f * fs));

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
            bend += bendCoef * (bendTarget - bend);
            if (needsCoefs || i % kCoefInterval == 0)
                updateCoefs (p.type, tone, focus, metal);

            // 0 at the start of a note and back at every cycle: moves away from rest and back
            const float lfo = moving ? 0.5f - 0.5f * std::cos (kTwoPi * lfoPhase) : 0.0f;
            lfoPhase = wrapPhase (lfoPhase + lfoInc);

            float dry = 0.0f, layers = 0.0f;
            switch (p.type)
            {
                case MachineType::Hit:
                    dry = strike (tone, p.motion);
                    layers = (1.0f - focus) * hitLayers();
                    break;
                case MachineType::Hiss: dry = hiss (lfo); break;
                case MachineType::Hum: dry = hum (tone, focus, lfo); break;
                case MachineType::Grind: dry = grind (tone, focus, lfo); break;
            }

            float wet = resonate (dry);
            if (! hit)
            {
                // a steady source only rings the modes it overlaps, so the ring is brought to
                // the source's own level, following it over about 30 ms
                dryPower += followCoef * (dry * dry - dryPower);
                wetPower += followCoef * (wet * wet - wetPower);
                const float target = std::clamp (std::sqrt (dryPower / std::max (1.0e-12f, wetPower)), 0.1f, 200.0f);
                wetGain += followCoef * (target - wetGain);
                wet *= wetGain;
            }
            const float s = dry + metal * (wet - dry) + layers;
            // light saturation; a harder hit drives it more
            out[i] += std::tanh (1.5f * s * gain) * (1.0f / 1.5f) * env * kLevel;

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

    struct Mode
    {
        float a1 = 0.0f, a2 = 0.0f, gain = 0.0f, spread = 0.0f;
        float y1 = 0.0f, y2 = 0.0f;
    };

    float pitchHz (float semis) const
    {
        return 440.0f * std::exp2 ((currentPitch + bend + variation.pitchSemis + semis - 69.0f) / 12.0f);
    }

    // The resonator's coefficients and gains, and Hiss's bandpass. A click puts little energy
    // into a long ring, so for Hit a mode's ring is made up by 1/(1-r); a steady source gets
    // unity gain at each peak, and render() matches the ring to the source's level.
    void updateCoefs (MachineType type, float tone, float focus, float metal)
    {
        needsCoefs = false;
        const float f0 = pitchHz (0.0f);
        const float ring = machineRingSeconds (metal) * variation.decayScale;
        const float tilt = 1.6f - 1.3f * tone; // high modes quieter when dark

        float total = 0.0f;
        std::array<float, kMachineModeRatios.size()> amps {};
        for (size_t k = 0; k < modes.size(); ++k)
            total += amps[k] = std::pow ((float) k + 1.0f, -tilt);

        for (size_t k = 0; k < modes.size(); ++k)
        {
            auto& m = modes[k];
            const float f = f0 * kMachineModeRatios[k] * (1.0f + m.spread);
            if (f >= 0.45f * fs)
            {
                m.gain = 0.0f; // above what this sample rate can play: let it fade, add nothing
                m.a1 = m.a2 = 0.0f;
                continue;
            }
            const float t60 = std::max (0.005f, ring * std::pow (kMachineModeRatios[k], -0.7f));
            const float r = std::exp (-6.91f / (t60 * fs));
            m.a1 = 2.0f * r * std::cos (kTwoPi * f / fs);
            m.a2 = r * r;
            // a struck low mode rings quieter than a high one (the zeros at DC take more of
            // it), so Hit's modes are evened out across the keyboard
            const float evenOut = std::pow (261.6f / std::max (20.0f, f0), 0.55f);
            m.gain = (type == MachineType::Hit ? kHitRing * evenOut : 1.0f - r) * amps[k] / total;
        }

        if (type == MachineType::Hiss)
        {
            // no higher than 16 kHz: above it a hiss is felt more than heard
            const float centre = std::clamp (pitchHz (48.0f * tone), 20.0f, std::min (16000.0f, 0.45f * fs));
            const float q = 0.7f * std::pow (60.0f, focus);
            svfG = std::tan (kPi * centre / fs);
            svfK = 1.0f / q;
            // a narrower band lets less noise through; this keeps the level about the same
            svfGain = std::min (120.0f, 0.7f * std::sqrt (fs / (kPi * centre / q)));
        }
    }

    // Two-pole bandpass modes with zeros at DC and Nyquist (x - x[n-2]), so a one-sided click
    // leaves no DC behind.
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

    // Hit's strike: one click and 3 ms of decaying noise, lowpassed by Tone (600 Hz .. 16 kHz).
    // Motion adds knocks while the note lasts, up to about 25 a second, each a different
    // strength, like a part that rattles.
    float strike (float tone, float motion)
    {
        const int burst = std::max (1, (int) (0.003f * fs));
        float x = 0.0f;
        if (struck < burst)
        {
            const float lpCoef = 1.0f - std::exp (-kTwoPi * 600.0f * std::exp2 (tone * 4.74f) / fs);
            hitLowpass += lpCoef * (noise.next() - hitLowpass);
            x = (struck == 0 ? 1.0f : 0.0f) + 0.6f * hitLowpass * (1.0f - (float) struck / (float) burst);
            ++struck;
        }
        else if (motion > 0.0f && stage != Stage::Fade && 0.5f + 0.5f * noise.next() < 25.0f * motion * motion / fs)
        {
            x = 0.3f + 0.35f * (1.0f + noise.next());
        }
        return x;
    }

    // Hit's layers: a deep rumble (noise lowpassed at 120 Hz and a sine two octaves below the
    // note, fading over about 350 ms) and a short hiss around 7 kHz (gone after about 60 ms).
    float hitLayers()
    {
        const float t = (float) elapsed / fs;
        const float lp = 1.0f - std::exp (-kTwoPi * 120.0f / fs);
        rumble1 += lp * (noise.next() - rumble1);
        rumble2 += lp * (rumble1 - rumble2);
        subPhase = wrapPhase (subPhase + std::min (0.45f, pitchHz (-24.0f) / fs));
        const float rumble = std::min (1.0f, t / 0.015f) * std::exp (-t / (0.12f * variation.decayScale))
                           * (3.0f * rumble2 + 0.35f * std::sin (kTwoPi * subPhase));

        const float v3 = noise.next() - air2;
        const float v1 = airA1 * air1 + airA2 * v3;
        const float v2 = air2 + airA2 * air1 + airA3 * v3;
        air1 = 2.0f * v1 - air1;
        air2 = 2.0f * v2 - air2;
        const float air = std::exp (-t / 0.012f) * v1 * airK;
        return 0.8f * rumble + 0.35f * air;
    }

    // White noise through a bandpass. Tone lifts the centre from the note up to four octaves
    // above it, since steam hisses high; Focus narrows it from about an octave wide to a
    // whistle. Motion makes the hiss flutter: the level dips.
    float hiss (float lfo)
    {
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
    float bend = 0.0f, bendTarget = 0.0f, bendCoef = 0.1f;
    std::array<Mode, kMachineModeRatios.size()> modes {};                    // the resonator
    float x1 = 0.0f, x2 = 0.0f;
    float dryPower = 0.0f, wetPower = 0.0f, wetGain = 1.0f; // the ring matched to the source
    int struck = 0;                                                         // Hit
    float hitLowpass = 0.0f, subPhase = 0.0f, rumble1 = 0.0f, rumble2 = 0.0f;
    float air1 = 0.0f, air2 = 0.0f, airK = 1.0f, airA1 = 0.0f, airA2 = 0.0f, airA3 = 0.0f;
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

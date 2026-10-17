// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "DspBasics.h"

#include <array>
#include <iterator>

// The Bleep engine: one oscillator (sine, triangle, pulse with variable width, or 2-operator
// FM) with a fast pitch envelope, a short amp envelope and a noise blend. BleepSynth plays
// up to kMaxVoices of them in Poly mode, or one with glide in Mono mode. JUCE-free.
namespace bleepforge
{
enum class BleepWave { Sine, Triangle, Pulse, FM };
inline constexpr const char* kBleepWaveNames[] = { "Sine", "Triangle", "Pulse", "FM" };
inline constexpr int kNumBleepWaves = (int) std::size (kBleepWaveNames);

inline constexpr float kMaxFmIndex = 8.0f; // modulation index at FM amount 100 %

struct BleepParams
{
    BleepWave wave = BleepWave::Sine;
    float pulseWidth = 0.5f;      // duty cycle 0.05..0.95, 0.5 = square
    float fmRatio = 2.0f;         // modulator frequency / carrier frequency
    float fmAmount = 0.3f;        // 0..1, index 0..kMaxFmIndex
    float pitchEnvSemis = 12.0f;  // pitch offset at the start of a note, decays to 0
    float pitchEnvDecayMs = 25.0f;
    float attackMs = 0.5f;
    float decayMs = 180.0f;
    float releaseMs = 30.0f;      // after note-off, if the decay hasn't ended the note yet
    float noise = 0.0f;           // 0 oscillator only .. 1 noise only
};

class BleepVoice
{
public:
    static constexpr float kLevel = 0.5f; // headroom for several voices at full velocity

    void prepare (float sampleRate, uint32_t noiseSeed)
    {
        fs = sampleRate;
        noise.seed (noiseSeed);
        reset();
    }

    void reset()
    {
        amp.reset();
        carrierPhase = modulatorPhase = 0.0f;
        note = -1;
    }

    bool isActive() const { return amp.isActive(); }
    bool isReleasing() const { return amp.isReleasing(); }
    int getNote() const { return note; }
    float getLevel() const { return amp.value; }

    // A voice that is still sounding keeps its phase and attacks from its current level, so
    // retriggering or stealing it doesn't click. With glide it slides from its current pitch.
    void start (int midiNote, float velocity, bool glide)
    {
        if (! amp.isActive())
        {
            carrierPhase = modulatorPhase = 0.0f;
            glide = false;
        }
        note = midiNote;
        targetPitch = (float) midiNote;
        if (! glide)
            currentPitch = targetPitch;
        gain = velocity;
        pitchEnv = 1.0f;
        amp.noteOn();
    }

    // Mono mode: back to a key that is still held, sliding there without a new attack.
    void glideTo (int midiNote)
    {
        note = midiNote;
        targetPitch = (float) midiNote;
    }

    void release() { amp.noteOff(); }

    // Adds the voice to out. glideCoef is the per-sample one-pole coefficient (0 = no glide).
    void render (float* out, int numSamples, const BleepParams& p, float glideCoef)
    {
        amp.set (fs, p.attackMs, p.decayMs, 0.0f, p.releaseMs);
        const float pitchCoef = std::exp (-4.6f / std::max (1.0f, p.pitchEnvDecayMs * 0.001f * fs)); // -40 dB
        const float fmIndex = p.fmAmount * kMaxFmIndex;
        const float pw = std::clamp (p.pulseWidth, 0.05f, 0.95f);

        for (int i = 0; i < numSamples && amp.isActive(); ++i)
        {
            currentPitch = targetPitch + (currentPitch - targetPitch) * glideCoef;
            const float pitch = currentPitch + p.pitchEnvSemis * pitchEnv;
            pitchEnv *= pitchCoef;
            const float inc = std::min (0.45f, 440.0f * std::exp2 ((pitch - 69.0f) / 12.0f) / fs);

            float s = oscillator (inc, p, pw, fmIndex);
            if (p.noise > 0.0f)
                s += p.noise * (noise.next() - s);
            out[i] += s * gain * amp.next() * kLevel;
        }
    }

private:
    float oscillator (float inc, const BleepParams& p, float pw, float fmIndex)
    {
        const float t = carrierPhase;
        float out = 0.0f;

        switch (p.wave)
        {
            case BleepWave::Sine:
                out = std::sin (kTwoPi * t);
                break;

            case BleepWave::Triangle:
            {
                // starts at 0 rising, like the sine, so a fresh note doesn't click
                const float u = wrapPhase (t + 0.25f);
                out = 1.0f - 4.0f * std::abs (u - 0.5f);
                out += 4.0f * inc * (polyBlamp (u, inc) - polyBlamp (wrapPhase (u + 0.5f), inc));
                break;
            }

            case BleepWave::Pulse:
                out = t < pw ? 1.0f : -1.0f;
                out += 2.0f * stepResidualAtWrap (t, inc);                    // rises by 2 at phase 0
                out -= 2.0f * stepResidualAtWrap (wrapPhase (t + 1.0f - pw), inc); // falls by 2 at pw
                out -= 2.0f * pw - 1.0f;                                      // remove the DC of the duty cycle
                break;

            case BleepWave::FM:
                // phase modulation of a sine carrier by a sine modulator
                out = std::sin (kTwoPi * t + fmIndex * std::sin (kTwoPi * modulatorPhase));
                modulatorPhase = wrapPhase (modulatorPhase + inc * p.fmRatio);
                break;
        }

        carrierPhase = wrapPhase (t + inc);
        return out;
    }

    float fs = 48000.0f;
    Envelope amp;
    WhiteNoise noise;
    float carrierPhase = 0.0f, modulatorPhase = 0.0f;
    float currentPitch = 60.0f, targetPitch = 60.0f, pitchEnv = 0.0f, gain = 1.0f;
    int note = -1;
};

class BleepSynth
{
public:
    static constexpr int kMaxVoices = 8;
    enum class Mode { Mono, Poly };

    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        for (size_t v = 0; v < voices.size(); ++v)
            voices[v].prepare (fs, 0x1234567u + 7919u * (uint32_t) v);
        numHeld = 0;
        setGlideMs (glideMs);
    }

    void setParams (const BleepParams& p) { params = p; }

    // Switching mode releases whatever plays, so no Poly voice is left hanging in Mono.
    void setMode (Mode m)
    {
        if (m == mode)
            return;
        allNotesOff();
        mode = m;
    }
    Mode getMode() const { return mode; }

    // Glide applies in Mono mode only. The pitch is within 1 % of the new note after ms.
    void setGlideMs (float ms)
    {
        glideMs = ms;
        glideCoef = ms <= 0.0f ? 0.0f : std::exp (-4.6f / (ms * 0.001f * fs));
    }

    void noteOn (int note, float velocity)
    {
        if (mode == Mode::Mono)
        {
            removeHeld (note);
            if (numHeld < (int) held.size())
                held[(size_t) numHeld++] = note;
            voices[0].start (note, velocity, glideCoef > 0.0f);
            return;
        }

        auto& voice = voiceForNote (note);
        voice.start (note, velocity, false);
        startOrder[(size_t) (&voice - voices.data())] = ++startCounter;
    }

    void noteOff (int note)
    {
        if (mode == Mode::Mono)
        {
            const bool wasPlaying = numHeld > 0 && held[(size_t) numHeld - 1] == note;
            removeHeld (note);
            if (! wasPlaying)
                return;
            if (numHeld > 0)
                voices[0].glideTo (held[(size_t) numHeld - 1]);
            else
                voices[0].release();
            return;
        }

        for (auto& voice : voices)
            if (voice.isActive() && ! voice.isReleasing() && voice.getNote() == note)
                voice.release();
    }

    // Lets every voice fade out with its release (MIDI all-notes-off, mode changes).
    void allNotesOff()
    {
        numHeld = 0;
        for (auto& voice : voices)
            voice.release();
    }

    // Writes (not adds) numSamples of mono output.
    void render (float* out, int numSamples)
    {
        std::fill (out, out + numSamples, 0.0f);
        const float coef = mode == Mode::Mono ? glideCoef : 0.0f;
        for (auto& voice : voices)
            if (voice.isActive())
                voice.render (out, numSamples, params, coef);
    }

    int activeVoices() const
    {
        int n = 0;
        for (const auto& voice : voices)
            n += voice.isActive() ? 1 : 0;
        return n;
    }

private:
    // The voice already playing this note, else a free one, else the quietest releasing one,
    // else the oldest.
    BleepVoice& voiceForNote (int note)
    {
        for (auto& voice : voices)
            if (voice.isActive() && voice.getNote() == note)
                return voice;
        for (auto& voice : voices)
            if (! voice.isActive())
                return voice;

        BleepVoice* best = nullptr;
        for (auto& voice : voices)
            if (voice.isReleasing() && (best == nullptr || voice.getLevel() < best->getLevel()))
                best = &voice;
        if (best != nullptr)
            return *best;

        size_t oldest = 0;
        for (size_t v = 1; v < voices.size(); ++v)
            if (startOrder[v] < startOrder[oldest])
                oldest = v;
        return voices[oldest];
    }

    void removeHeld (int note)
    {
        const auto end = held.begin() + numHeld;
        numHeld = (int) (std::remove (held.begin(), end, note) - held.begin());
    }

    float fs = 48000.0f;
    BleepParams params;
    Mode mode = Mode::Poly;
    float glideMs = 0.0f, glideCoef = 0.0f;
    std::array<BleepVoice, kMaxVoices> voices;
    std::array<uint64_t, kMaxVoices> startOrder {};
    uint64_t startCounter = 0;
    std::array<int, 16> held {}; // Mono: keys held, last pressed at the end
    int numHeld = 0;
};
} // namespace bleepforge

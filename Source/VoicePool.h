// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// Plays notes on a set of voices of one engine: up to kMaxVoices at once in Poly mode
// (stealing the quietest releasing voice, else the oldest), or one with glide in Mono mode
// (the last key held wins). Chaos gives every note its own small variation of pitch, decay,
// tone and stereo position. JUCE-free.
//
// A Voice provides: prepare (float sampleRate, uint32_t noiseSeed), reset(), isActive(),
// isReleasing(), getNote(), getLevel(), start (int note, float velocity, bool glide,
// const Variation&), glideTo (int note), release(), and render (float* out, int numSamples,
// const Params&, float glideCoef), which adds to out.
namespace bleepforge
{
// How one note differs from the patch, drawn by Chaos.
struct Variation
{
    float pitchSemis = 0.0f;
    float decayScale = 1.0f;
    float tone = 0.0f; // -1 darker .. 1 brighter
};

// How far Chaos may move each note, 0..1 per target (the amount already applied).
struct ChaosDepths
{
    float pitch = 0.0f, decay = 0.0f, tone = 0.0f, pan = 0.0f;
};

inline constexpr float kChaosMaxPitchSemis = 2.0f;  // at full depth; depth^2, so fine near 0
inline constexpr float kChaosMaxDecayOctaves = 1.5f; // decay up to 2^1.5 longer or shorter

template <typename Voice, typename Params>
class VoicePool
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

    void setParams (const Params& p) { params = p; }
    void setChaos (const ChaosDepths& depths) { chaos = depths; }

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
            startVoice (0, note, velocity, glideCoef > 0.0f);
            return;
        }

        const auto index = (size_t) (&voiceForNote (note) - voices.data());
        startVoice (index, note, velocity, false);
        startOrder[index] = ++startCounter;
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

    // Writes (not adds) numSamples of mono output, without panning.
    void render (float* out, int numSamples)
    {
        std::fill (out, out + numSamples, 0.0f);
        renderAdding (out, nullptr, numSamples);
    }

    // Adds the voices to left and right, each at its own position; with right null, adds
    // them to left unpanned. A note's position glides over one chunk, so moving a ringing
    // voice doesn't click.
    void renderAdding (float* left, float* right, int numSamples)
    {
        const float coef = mode == Mode::Mono ? glideCoef : 0.0f;
        for (int pos = 0; pos < numSamples; pos += kChunk)
        {
            const int length = std::min (kChunk, numSamples - pos);
            for (size_t v = 0; v < voices.size(); ++v)
            {
                if (! voices[v].isActive())
                    continue;
                std::fill (scratch.begin(), scratch.begin() + length, 0.0f);
                voices[v].render (scratch.data(), length, params, coef);

                if (right == nullptr)
                {
                    for (int i = 0; i < length; ++i)
                        left[pos + i] += scratch[(size_t) i];
                    continue;
                }
                const float from = panNow[v], to = panTarget[v];
                for (int i = 0; i < length; ++i)
                {
                    const float pan = from + (to - from) * (float) (i + 1) / (float) length;
                    left[pos + i] += scratch[(size_t) i] * std::min (1.0f, 1.0f - pan); // the centre keeps full level
                    right[pos + i] += scratch[(size_t) i] * std::min (1.0f, 1.0f + pan);
                }
                panNow[v] = to;
            }
        }
    }

    int activeVoices() const
    {
        int n = 0;
        for (const auto& voice : voices)
            n += voice.isActive() ? 1 : 0;
        return n;
    }

private:
    static constexpr int kChunk = 128;

    void startVoice (size_t index, int note, float velocity, bool glide)
    {
        Variation variation;
        variation.pitchSemis = signedRandom() * kChaosMaxPitchSemis * chaos.pitch * chaos.pitch;
        variation.decayScale = std::exp2 (signedRandom() * kChaosMaxDecayOctaves * chaos.decay);
        variation.tone = signedRandom() * chaos.tone;
        const float pan = signedRandom() * chaos.pan;

        if (! voices[index].isActive())
            panNow[index] = pan; // a silent voice can jump; a ringing one glides
        panTarget[index] = pan;
        voices[index].start (note, velocity, glide, variation);
    }

    // xorshift32 in -1..1
    float signedRandom()
    {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        return (float) (rng >> 8) * (2.0f / 16777216.0f) - 1.0f;
    }

    // The voice already playing this note, else a free one, else the quietest releasing one,
    // else the oldest.
    Voice& voiceForNote (int note)
    {
        for (auto& voice : voices)
            if (voice.isActive() && voice.getNote() == note)
                return voice;
        for (auto& voice : voices)
            if (! voice.isActive())
                return voice;

        Voice* best = nullptr;
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
    Params params;
    ChaosDepths chaos;
    uint32_t rng = 0x6c8e9cf5u;
    std::array<float, kMaxVoices> panNow {}, panTarget {};
    std::array<float, kChunk> scratch {};
    Mode mode = Mode::Poly;
    float glideMs = 0.0f, glideCoef = 0.0f;
    std::array<Voice, kMaxVoices> voices;
    std::array<uint64_t, kMaxVoices> startOrder {};
    uint64_t startCounter = 0;
    std::array<int, 16> held {}; // Mono: keys held, last pressed at the end
    int numHeld = 0;
};
} // namespace bleepforge

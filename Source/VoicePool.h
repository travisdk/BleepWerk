// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// Plays notes on a set of voices of one engine: up to kMaxVoices at once in Poly mode
// (stealing the quietest releasing voice, else the oldest), or one with glide in Mono mode
// (the last key held wins). JUCE-free.
//
// A Voice provides: prepare (float sampleRate, uint32_t noiseSeed), reset(), isActive(),
// isReleasing(), getNote(), getLevel(), start (int note, float velocity, bool glide),
// glideTo (int note), release(), and render (float* out, int numSamples, const Params&,
// float glideCoef), which adds to out.
namespace bleepforge
{
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
        renderAdding (out, numSamples);
    }

    void renderAdding (float* out, int numSamples)
    {
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
    Mode mode = Mode::Poly;
    float glideMs = 0.0f, glideCoef = 0.0f;
    std::array<Voice, kMaxVoices> voices;
    std::array<uint64_t, kMaxVoices> startOrder {};
    uint64_t startCounter = 0;
    std::array<int, 16> held {}; // Mono: keys held, last pressed at the end
    int numHeld = 0;
};
} // namespace bleepforge

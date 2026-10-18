// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "DspBasics.h"

#include <array>
#include <iterator>
#include <vector>

// The effects after the engines, in a fixed order: filter, bitcrush, tempo-synced delay and a
// small room. Each one is transparent at its default (filter Off, Bits and Rate Off, mixes at
// 0), so a patch that doesn't use them sounds as before. Buffers are sized in prepare(); the
// audio thread never allocates. JUCE-free.
namespace bleepforge
{
enum class FilterType { Off, LowPass, BandPass, HighPass };
inline constexpr const char* kFilterTypeNames[] = { "Off", "LP", "BP", "HP" };
inline constexpr int kNumFilterTypes = (int) std::size (kFilterTypeNames);

// Delay times in beats, straight, triplet and dotted, short enough for a percussive line.
inline constexpr const char* kDelayTimeNames[] = { "1/32", "1/16T", "1/16", "1/8T", "1/16D", "1/8", "1/4T", "1/8D", "1/4" };
inline constexpr double kDelayTimeBeats[] = { 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.375, 0.5, 2.0 / 3.0, 0.75, 1.0 };
inline constexpr int kNumDelayTimes = (int) std::size (kDelayTimeNames);
inline constexpr int kDefaultDelayTime = 4; // 1/16 dotted, the classic against a straight 1/16 line

inline constexpr float kCrushBitsOff = 16.0f;    // at the top of the Bits range the crusher is out
inline constexpr float kCrushRateOff = 48000.0f; // and at the top of the Rate range there is no hold

struct EffectParams
{
    FilterType filterType = FilterType::Off;
    float cutoffHz = 2000.0f;
    float resonance = 0.2f;      // 0..1
    float crushBits = kCrushBitsOff; // 1..16, fractional values blend in smoothly
    float crushRateHz = kCrushRateOff;
    int delayTime = kDefaultDelayTime; // index into kDelayTimeBeats
    float delayFeedback = 0.35f; // 0..0.95
    float delayMix = 0.0f;       // 0..1
    float roomSize = 0.4f;       // 0..1
    float roomMix = 0.0f;        // 0..1
};

// Dry stays at full level up to the middle and the wet signal reaches full level there, so
// the middle of a Mix knob is "both at once" rather than both at half.
inline void mixGains (float mix, float& dry, float& wet)
{
    dry = std::min (1.0f, 2.0f * (1.0f - mix));
    wet = std::min (1.0f, 2.0f * mix);
}

// One-pole smoothing towards a target, per sample, so knob moves and automation don't click.
struct Smoothed
{
    float value = 0.0f, target = 0.0f, coef = 0.0f;

    void prepare (float fs, float ms = 20.0f) { coef = std::exp (-1.0f / std::max (1.0f, ms * 0.001f * fs)); }
    void jump (float v) { value = target = v; }
    float next() { return value = target + (value - target) * coef; }
};

// State-variable filter in the trapezoidal (topology-preserving) form: stable under fast
// cutoff changes and with resonance that doesn't blow up at high cutoffs.
class Filter
{
public:
    void prepare (float sampleRate)
    {
        fs = sampleRate;
        g.prepare (fs);
        k.prepare (fs);
        reset();
    }

    void reset()
    {
        for (auto& s : state)
            s = { 0.0f, 0.0f };
        primed = false;
    }

    void setParams (FilterType newType, float cutoffHz, float resonance)
    {
        if (newType != type)
            reset(); // a different response from the same state would thump
        type = newType;
        const float hz = std::clamp (cutoffHz, 20.0f, 0.45f * fs);
        g.target = std::tan (kPi * hz / fs);
        // damping 2 (no peak) down to 0.07 (Q about 14)
        k.target = 2.0f - 1.93f * std::clamp (resonance, 0.0f, 1.0f);
        if (! primed)
        {
            g.jump (g.target);
            k.jump (k.target);
            primed = true;
        }
    }

    void process (float* left, float* right, int numSamples)
    {
        if (type == FilterType::Off)
            return;
        for (int i = 0; i < numSamples; ++i)
        {
            const float gi = g.next(), ki = k.next();
            const float a1 = 1.0f / (1.0f + gi * (gi + ki)), a2 = gi * a1, a3 = gi * a2;
            left[i] = tick (state[0], left[i], a1, a2, a3, ki);
            if (right != nullptr)
                right[i] = tick (state[1], right[i], a1, a2, a3, ki);
        }
    }

private:
    struct State { float ic1 = 0.0f, ic2 = 0.0f; };

    float tick (State& s, float x, float a1, float a2, float a3, float ki) const
    {
        const float v3 = x - s.ic2;
        const float v1 = a1 * s.ic1 + a2 * v3;
        const float v2 = s.ic2 + a2 * s.ic1 + a3 * v3;
        s.ic1 = 2.0f * v1 - s.ic1;
        s.ic2 = 2.0f * v2 - s.ic2;
        switch (type)
        {
            case FilterType::LowPass: return v2;
            case FilterType::BandPass: return v1 * ki; // unity gain at the centre for any resonance
            case FilterType::HighPass: return x - ki * v1 - v2;
            case FilterType::Off: break;
        }
        return x;
    }

    float fs = 48000.0f;
    FilterType type = FilterType::Off;
    Smoothed g, k;
    bool primed = false;
    std::array<State, 2> state {};
};

// Bit depth reduction and sample-and-hold at a lower rate. Fractional bit depths quantize to
// a fractional number of levels, so sweeping Bits is a smooth change instead of steps.
class Crusher
{
public:
    void prepare (float sampleRate)
    {
        fs = sampleRate;
        reset();
    }

    void reset()
    {
        holdPhase = 1.0f;
        held = { 0.0f, 0.0f };
    }

    void setParams (float bits, float rateHz)
    {
        bitsOn = bits < kCrushBitsOff - 0.01f;
        holdOn = rateHz < std::min (kCrushRateOff, fs) - 1.0f;
        levels = std::pow (2.0f, std::clamp (bits, 1.0f, kCrushBitsOff) - 1.0f);
        holdStep = std::clamp (rateHz, 50.0f, fs) / fs;
        if (! holdOn)
            holdPhase = 1.0f; // the next time it is turned on it picks up a fresh sample
    }

    void process (float* left, float* right, int numSamples)
    {
        if (! bitsOn && ! holdOn)
            return;
        for (int i = 0; i < numSamples; ++i)
        {
            if (holdOn)
            {
                if (holdPhase >= 1.0f)
                {
                    holdPhase -= std::floor (holdPhase);
                    held[0] = left[i];
                    held[1] = right != nullptr ? right[i] : 0.0f;
                }
                holdPhase += holdStep;
                left[i] = held[0];
                if (right != nullptr)
                    right[i] = held[1];
            }
            if (bitsOn)
            {
                left[i] = quantize (left[i]);
                if (right != nullptr)
                    right[i] = quantize (right[i]);
            }
        }
    }

private:
    float quantize (float x) const { return std::round (x * levels) / levels; }

    float fs = 48000.0f;
    bool bitsOn = false, holdOn = false;
    float levels = 32768.0f, holdStep = 1.0f, holdPhase = 1.0f;
    std::array<float, 2> held {};
};

// A stereo delay synced to the host tempo. The feedback path is darkened a little, so the
// repeats fade into the background the way tape or analogue echoes do.
class Delay
{
public:
    static constexpr double kMaxSeconds = 4.0; // 1/4 at 15 BPM

    void prepare (float sampleRate)
    {
        fs = sampleRate;
        const size_t size = (size_t) (kMaxSeconds * fs) + 4;
        for (auto& line : lines)
            line.assign (size, 0.0f);
        delaySamples.prepare (fs, 60.0f);
        feedback.prepare (fs);
        mix.prepare (fs);
        dampCoef = 1.0f - std::exp (-kTwoPi * 4500.0f / fs);
        reset();
    }

    void reset()
    {
        for (auto& line : lines)
            std::fill (line.begin(), line.end(), 0.0f);
        damp = { 0.0f, 0.0f };
        writePos = 0;
        primed = false;
    }

    void setParams (int timeIndex, double bpm, float newFeedback, float newMix)
    {
        const double beats = kDelayTimeBeats[std::clamp (timeIndex, 0, kNumDelayTimes - 1)];
        const double seconds = beats * 60.0 / std::max (1.0, bpm);
        delaySamples.target = (float) std::clamp (seconds * fs, 1.0, (double) lines[0].size() - 3.0);
        feedback.target = std::clamp (newFeedback, 0.0f, 0.95f);
        mix.target = std::clamp (newMix, 0.0f, 1.0f);
        if (! primed)
        {
            delaySamples.jump (delaySamples.target);
            feedback.jump (feedback.target);
            mix.jump (mix.target);
            primed = true;
        }
    }

    void process (float* left, float* right, int numSamples)
    {
        if (lines[0].empty())
            return;
        const int size = (int) lines[0].size();
        for (int i = 0; i < numSamples; ++i)
        {
            const float d = delaySamples.next(), fb = feedback.next();
            float dry, wet;
            mixGains (mix.next(), dry, wet);

            // a fractional read, so a tempo change glides instead of clicking
            float readPos = (float) writePos - d;
            if (readPos < 0.0f)
                readPos += (float) size;
            const int r0 = (int) readPos;
            const int r1 = r0 + 1 < size ? r0 + 1 : 0;
            const float frac = readPos - (float) r0;

            float* io[2] = { left, right };
            for (int c = 0; c < (right != nullptr ? 2 : 1); ++c)
            {
                auto& line = lines[(size_t) c];
                const float echo = line[(size_t) r0] + (line[(size_t) r1] - line[(size_t) r0]) * frac;
                damp[(size_t) c] += dampCoef * (echo - damp[(size_t) c]);
                const float in = io[c][i];
                line[(size_t) writePos] = in + fb * damp[(size_t) c];
                io[c][i] = dry * in + wet * echo;
            }
            if (++writePos >= size)
                writePos = 0;
        }
    }

private:
    float fs = 48000.0f;
    std::array<std::vector<float>, 2> lines;
    Smoothed delaySamples, feedback, mix;
    float dampCoef = 0.5f;
    std::array<float, 2> damp {};
    int writePos = 0;
    bool primed = false;
};

// A small room: an 8-line feedback delay network mixed by a Hadamard matrix. Size scales the
// line lengths and the decay time together; the loop is damped, so the tail darkens as it
// fades, like a real room.
class Room
{
public:
    static constexpr int kLines = 8;

    void prepare (float sampleRate)
    {
        fs = sampleRate;
        for (size_t l = 0; l < kLines; ++l)
            lines[l].assign ((size_t) (kBaseMs[l] * 0.001f * kMaxScale * fs) + 4, 0.0f);
        scale.prepare (fs, 80.0f);
        mix.prepare (fs);
        dampCoef = 1.0f - std::exp (-kTwoPi * 6000.0f / fs);
        reset();
    }

    void reset()
    {
        for (auto& line : lines)
            std::fill (line.begin(), line.end(), 0.0f);
        damp.fill (0.0f);
        writePos.fill (0);
        primed = false;
    }

    void setParams (float size, float newMix)
    {
        size = std::clamp (size, 0.0f, 1.0f);
        scale.target = kMinScale + (kMaxScale - kMinScale) * size;
        // gain per pass so every line falls 60 dB in the decay time; per block, as pow is slow
        const float decaySeconds = 0.2f + 1.3f * size;
        for (size_t l = 0; l < kLines; ++l)
            gains[l] = std::pow (10.0f, -3.0f * kBaseMs[l] * 0.001f * scale.target / decaySeconds);
        mix.target = std::clamp (newMix, 0.0f, 1.0f);
        if (! primed)
        {
            scale.jump (scale.target);
            mix.jump (mix.target);
            primed = true;
        }
    }

    void process (float* left, float* right, int numSamples)
    {
        if (lines[0].empty())
            return;
        for (int i = 0; i < numSamples; ++i)
        {
            const float s = scale.next();
            float dry, wet;
            mixGains (mix.next(), dry, wet);
            const float inL = left[i], inR = right != nullptr ? right[i] : inL;

            std::array<float, kLines> x;
            for (size_t l = 0; l < kLines; ++l)
            {
                const auto& line = lines[l];
                const int size = (int) line.size();
                const float length = kBaseMs[l] * 0.001f * s * fs;
                float readPos = (float) writePos[l] - length;
                if (readPos < 0.0f)
                    readPos += (float) size;
                const int r0 = (int) readPos;
                const int r1 = r0 + 1 < size ? r0 + 1 : 0;
                const float out = line[(size_t) r0] + (line[(size_t) r1] - line[(size_t) r0]) * ((float) readPos - (float) r0);

                damp[l] += dampCoef * (out - damp[l]);
                x[l] = damp[l] * gains[l];
            }

            float outL = 0.0f, outR = 0.0f;
            for (size_t l = 0; l < kLines; ++l)
                (l % 2 == 0 ? outL : outR) += x[l];

            hadamard (x);
            for (size_t l = 0; l < kLines; ++l)
            {
                // left feeds the even lines, right the odd ones, with alternating signs
                const float in = (l % 2 == 0 ? inL : inR) * ((l / 2) % 2 == 0 ? 1.0f : -1.0f);
                auto& line = lines[l];
                line[(size_t) writePos[l]] = x[l] + in * kInputGain;
                if (++writePos[l] >= (int) line.size())
                    writePos[l] = 0;
            }

            left[i] = dry * inL + wet * outL * kOutputGain;
            if (right != nullptr)
                right[i] = dry * inR + wet * outR * kOutputGain;
        }
    }

private:
    // mutually prime-ish lengths so the echoes don't line up into a pitch
    static constexpr std::array<float, kLines> kBaseMs { 7.3f, 9.1f, 11.3f, 12.7f, 14.9f, 17.3f, 19.7f, 23.1f };
    static constexpr float kMinScale = 0.6f, kMaxScale = 2.2f;
    static constexpr float kInputGain = 0.5f, kOutputGain = 0.5f;

    // Fast Walsh-Hadamard transform, normalized so it is energy preserving.
    static void hadamard (std::array<float, kLines>& x)
    {
        for (size_t h = 1; h < kLines; h *= 2)
            for (size_t i = 0; i < kLines; i += h * 2)
                for (size_t j = i; j < i + h; ++j)
                {
                    const float a = x[j], b = x[j + h];
                    x[j] = a + b;
                    x[j + h] = a - b;
                }
        const float norm = 1.0f / std::sqrt ((float) kLines);
        for (auto& v : x)
            v *= norm;
    }

    float fs = 48000.0f;
    std::array<std::vector<float>, kLines> lines;
    std::array<float, kLines> damp {}, gains {};
    std::array<int, kLines> writePos {};
    Smoothed scale, mix;
    float dampCoef = 0.5f;
    bool primed = false;
};

// The chain in the order the brief sets.
class Effects
{
public:
    void prepare (double sampleRate)
    {
        const float fs = (float) sampleRate;
        filter.prepare (fs);
        crusher.prepare (fs);
        delay.prepare (fs);
        room.prepare (fs);
    }

    void reset()
    {
        filter.reset();
        crusher.reset();
        delay.reset();
        room.reset();
    }

    void setParams (const EffectParams& p, double bpm)
    {
        filter.setParams (p.filterType, p.cutoffHz, p.resonance);
        crusher.setParams (p.crushBits, p.crushRateHz);
        delay.setParams (p.delayTime, bpm, p.delayFeedback, p.delayMix);
        room.setParams (p.roomSize, p.roomMix);
    }

    // right may be null for a mono output
    void process (float* left, float* right, int numSamples)
    {
        filter.process (left, right, numSamples);
        crusher.process (left, right, numSamples);
        delay.process (left, right, numSamples);
        room.process (left, right, numSamples);
    }

private:
    Filter filter;
    Crusher crusher;
    Delay delay;
    Room room;
};
} // namespace bleepforge

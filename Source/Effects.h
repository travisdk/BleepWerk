// Copyright (C) 2026 Henrik Juhl. Part of BleepWerk, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "DspBasics.h"

#include <array>
#include <iterator>
#include <vector>

// The effects after the engines, in a fixed order: filter, bitcrush, tempo-synced delay and a
// plate reverb, with a distortion (Drive) that sits either first or last. Each one is
// transparent at its default (filter Off, Bits and Rate Off, mixes at 0), so a patch that
// doesn't use them sounds as before. Buffers are sized in prepare(); the audio thread never
// allocates. JUCE-free.
namespace bleepwerk
{
enum class FilterType { Off, LowPass, BandPass, HighPass };
inline constexpr const char* kFilterTypeNames[] = { "Off", "LP", "BP", "HP" };
inline constexpr int kNumFilterTypes = (int) std::size (kFilterTypeNames);

// The delay time is a note value, each twice the one before, and a feel that makes it a
// triplet or dotted. Two controls rather than one list, so the knob takes even steps and the
// feel can be seen at a glance.
inline constexpr const char* kDelayNoteNames[] = { "1/32", "1/16", "1/8", "1/4", "1/2" };
inline constexpr double kDelayNoteBeats[] = { 0.125, 0.25, 0.5, 1.0, 2.0 };
inline constexpr int kNumDelayNotes = (int) std::size (kDelayNoteNames);
inline constexpr const char* kDelayFeelNames[] = { "Straight", "Triplet", "Dotted" };
inline constexpr double kDelayFeelScale[] = { 1.0, 2.0 / 3.0, 1.5 };
inline constexpr int kNumDelayFeels = (int) std::size (kDelayFeelNames);
inline constexpr int kDefaultDelayNote = 1, kDefaultDelayFeel = 2; // 1/16 dotted, the classic against a straight 1/16 line

inline double delayBeats (int note, int feel)
{
    return kDelayNoteBeats[std::clamp (note, 0, kNumDelayNotes - 1)] * kDelayFeelScale[std::clamp (feel, 0, kNumDelayFeels - 1)];
}

// Before 0.11 the time was one list, "delayTime": 1/32, 1/16T, 1/16, 1/8T, 1/16D, 1/8, 1/4T,
// 1/8D, 1/4. Older projects and presets are read through this, so they keep their length.
inline void splitOldDelayTime (int oldIndex, int& note, int& feel)
{
    constexpr int notes[] = { 0, 1, 1, 2, 1, 2, 3, 2, 3 };
    constexpr int feels[] = { 0, 1, 0, 1, 2, 0, 1, 2, 0 };
    const int i = std::clamp (oldIndex, 0, (int) std::size (notes) - 1);
    note = notes[i];
    feel = feels[i];
}

// Warm is a soft tape-like saturation, Hard clips like a fuzz pedal and Fold is a wavefolder,
// whose overtones are the inharmonic, metallic kind that suits Strike.
enum class DriveType { Warm, Hard, Fold };
inline constexpr const char* kDriveTypeNames[] = { "Warm", "Hard", "Fold" };
inline constexpr int kNumDriveTypes = (int) std::size (kDriveTypeNames);
inline constexpr const char* kDrivePlaceNames[] = { "Pre", "Post" };

inline constexpr float kCrushBitsOff = 16.0f;    // at the top of the Bits range the crusher is out
inline constexpr float kCrushRateOff = 48000.0f; // and at the top of the Rate range there is no hold

struct EffectParams
{
    FilterType filterType = FilterType::Off;
    float cutoffHz = 2000.0f;
    float resonance = 0.2f;      // 0..1
    float crushBits = kCrushBitsOff; // 1..16, fractional values blend in smoothly
    float crushRateHz = kCrushRateOff;
    int delayNote = kDefaultDelayNote; // index into kDelayNoteBeats
    int delayFeel = kDefaultDelayFeel; // index into kDelayFeelScale
    float delayFeedback = 0.35f; // 0..0.95
    float delayMix = 0.0f;       // 0..1
    float roomSize = 0.4f;       // 0..1
    float roomDecay = 1.2f;      // seconds
    float roomTone = 0.5f;       // 0..1, dark to bright
    float roomMix = 0.0f;        // 0..1
    float roomDuck = 0.0f;       // 0..1
    DriveType driveType = DriveType::Warm;
    bool drivePost = true;       // false: before the filter, true: after the reverb
    float driveAmount = 0.5f;    // 0..1, nearly clean to wrecked
    float driveMix = 0.0f;       // 0..1
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
    static constexpr double kMaxSeconds = 6.0; // 1/2 dotted at 30 BPM

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

    void setParams (int note, int feel, double bpm, float newFeedback, float newMix)
    {
        const double seconds = delayBeats (note, feel) * 60.0 / std::max (1.0, bpm);
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

// A delay line read at a fractional delay, for the plate's lines and allpasses. The buffer is
// a power of two so wrapping is a mask. Before this sample's push, read (d) gives the input d
// samples ago (d >= 1).
class DelayLine
{
public:
    void prepare (int maxDelay)
    {
        size_t size = 1;
        while (size < (size_t) maxDelay + 2)
            size *= 2;
        buffer.assign (size, 0.0f);
        mask = (int) size - 1;
        pos = 0;
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        pos = 0;
    }

    bool empty() const { return buffer.empty(); }

    float read (float delay) const
    {
        const int whole = (int) delay;
        const float frac = delay - (float) whole;
        const float a = buffer[(size_t) ((pos - whole + 1) & mask)];
        const float b = buffer[(size_t) ((pos - whole) & mask)];
        return a + frac * (b - a);
    }

    void push (float x)
    {
        pos = (pos + 1) & mask;
        buffer[(size_t) pos] = x;
    }

    // a Schroeder allpass around this line: H = (z^-N - g) / (1 - g z^-N)
    float allpass (float x, float delay, float g)
    {
        const float z = read (delay);
        const float v = x + g * z;
        push (v);
        return z - g * v;
    }

private:
    std::vector<float> buffer;
    int mask = 0, pos = 0;
};

// The reverb: Jon Dattorro's plate ("Effect Design, Part 1: Reverberator and Other Filters",
// J. Audio Eng. Soc. 45(9), 1997), written from the paper; see docs/LICENSE-REVIEW.md. The
// input is low-cut, pre-delayed, band-limited and smeared by four allpasses, then circulates
// in a tank of two cross-fed halves, each a modulated allpass, a delay, damping, a second
// allpass and a delay. The slow modulation in the tank keeps the tail from ringing at fixed
// pitches, which on tonal percussion sounds metallic. Size scales every delay, Decay sets the
// loop gain for that time, Tone the damping and the input bandwidth, and Duck turns the wet
// signal down while the input plays so the tail blooms in the gaps.
class Plate
{
public:
    void prepare (float sampleRate)
    {
        fs = sampleRate;
        rateScale = fs / kPaperRate;
        const float most = kMaxSize * rateScale;
        for (size_t k = 0; k < diffusers.size(); ++k)
            diffusers[k].prepare ((int) (kDiffuserLengths[k] * most) + 2);
        for (size_t k = 0; k < 2; ++k)
        {
            modAllpass[k].prepare ((int) ((kModLengths[k] + kExcursion) * most) + 2);
            firstDelay[k].prepare ((int) (kFirstDelays[k] * most) + 2);
            secondAllpass[k].prepare ((int) (kSecondLengths[k] * most) + 2);
            secondDelay[k].prepare ((int) (kSecondDelays[k] * most) + 2);
        }
        preDelay.prepare ((int) (kPreDelayMs * 0.001f * fs) + 2);

        size.prepare (fs, 80.0f);
        mix.prepare (fs);
        lowCutCoef = 1.0f - std::exp (-kTwoPi * kLowCutHz / fs);
        lfoStep = kTwoPi * kLfoHz / fs;
        duckAttack = 1.0f - std::exp (-1.0f / (0.003f * fs));
        duckRelease = 1.0f - std::exp (-1.0f / (0.25f * fs));
        reset();
    }

    void reset()
    {
        for (auto& d : diffusers)
            d.reset();
        for (size_t k = 0; k < 2; ++k)
        {
            modAllpass[k].reset();
            firstDelay[k].reset();
            secondAllpass[k].reset();
            secondDelay[k].reset();
        }
        preDelay.reset();
        lowCut.fill (0.0f);
        bandwidthState = 0.0f;
        damp.fill (0.0f);
        tankOut.fill (0.0f);
        lfoPhase = 0.0f;
        duckEnv = 0.0f;
        primed = false;
    }

    // size, tone and duck 0..1, decay the time in seconds for the tail to fall 60 dB
    void setParams (float newSize, float decaySeconds, float tone, float newMix, float duck)
    {
        size.target = kMinSize + (kMaxSize - kMinSize) * std::clamp (newSize, 0.0f, 1.0f);
        // the gain per tank segment (the paper's "decay") so the loop falls 60 dB in the decay
        // time; per block, as pow is slow
        const float segmentSeconds = kSegmentLength * size.target / kPaperRate;
        decayGain = std::pow (10.0f, -3.0f * segmentSeconds / std::max (0.05f, decaySeconds));
        // the paper ties decay diffusion 2 to the decay, kept between 0.25 and 0.5
        diffusion2 = std::clamp (decayGain + 0.15f, 0.25f, 0.5f);

        tone = std::clamp (tone, 0.0f, 1.0f);
        dampCoef = 1.0f - std::exp (-kTwoPi * std::min (0.45f * fs, 1500.0f * std::exp2 (3.4f * tone)) / fs);
        bandwidthCoef = 1.0f - std::exp (-kTwoPi * std::min (0.45f * fs, 4000.0f * std::exp2 (2.3f * tone)) / fs);
        duckDepth = 0.75f * std::clamp (duck, 0.0f, 1.0f); // at most -12 dB
        mix.target = std::clamp (newMix, 0.0f, 1.0f);
        if (! primed)
        {
            size.jump (size.target);
            mix.jump (mix.target);
            primed = true;
        }
    }

    void process (float* left, float* right, int numSamples)
    {
        if (preDelay.empty())
            return;
        const float preDelaySamples = kPreDelayMs * 0.001f * fs;
        for (int i = 0; i < numSamples; ++i)
        {
            const float scale = size.next() * rateScale;
            float dry, wet;
            mixGains (mix.next(), dry, wet);
            const float inL = left[i], inR = right != nullptr ? right[i] : inL;

            // the duck follows the input level, quick to grab and slow to let go
            const float level = std::max (std::abs (inL), std::abs (inR));
            duckEnv += (level > duckEnv ? duckAttack : duckRelease) * (level - duckEnv);
            const float duckGain = 1.0f - duckDepth * std::min (1.0f, duckEnv * (1.0f / kDuckLevel));

            // input: low cut (two one-pole high-passes), pre-delay, bandwidth, diffusion
            float x = 0.5f * (inL + inR);
            for (auto& state : lowCut)
            {
                state += lowCutCoef * (x - state);
                x -= state;
            }
            const float delayed = preDelay.read (preDelaySamples);
            preDelay.push (x);
            bandwidthState += bandwidthCoef * (delayed - bandwidthState);
            x = bandwidthState;
            for (size_t k = 0; k < diffusers.size(); ++k)
                x = diffusers[k].allpass (x, kDiffuserLengths[k] * scale, k < 2 ? kInputDiffusion1 : kInputDiffusion2);

            // the tank: each half takes the input plus the other half's output
            lfoPhase += lfoStep;
            if (lfoPhase > kTwoPi)
                lfoPhase -= kTwoPi;
            const float lfo[2] = { std::sin (lfoPhase), std::cos (lfoPhase) };
            const float fed[2] = { x + tankOut[1], x + tankOut[0] };
            for (size_t k = 0; k < 2; ++k)
            {
                // the paper's decay diffusion 1 has the opposite sign to the other allpasses
                float t = modAllpass[k].allpass (fed[k], (kModLengths[k] + kExcursion * lfo[k]) * scale, -kDecayDiffusion1);
                const float d = firstDelay[k].read (kFirstDelays[k] * scale);
                firstDelay[k].push (t);
                damp[k] += dampCoef * (d - damp[k]);
                t = secondAllpass[k].allpass (damp[k] * decayGain, kSecondLengths[k] * scale, diffusion2);
                tankOut[k] = secondDelay[k].read (kSecondDelays[k] * scale) * decayGain;
                secondDelay[k].push (t);
            }

            // output taps (the paper's Table 2): each side reads mostly from the other half
            auto tap = [scale] (const DelayLine& line, float at) { return line.read (std::max (1.0f, at * scale)); };
            const float outL = tap (firstDelay[1], 266) + tap (firstDelay[1], 2974) - tap (secondAllpass[1], 1913)
                             + tap (secondDelay[1], 1996) - tap (firstDelay[0], 1990) - tap (secondAllpass[0], 187)
                             - tap (secondDelay[0], 1066);
            const float outR = tap (firstDelay[0], 353) + tap (firstDelay[0], 3627) - tap (secondAllpass[0], 1228)
                             + tap (secondDelay[0], 2673) - tap (firstDelay[1], 2111) - tap (secondAllpass[1], 335)
                             - tap (secondDelay[1], 121);

            const float wetGain = wet * duckGain * kOutputGain;
            left[i] = dry * inL + wetGain * outL;
            if (right != nullptr)
                right[i] = dry * inR + wetGain * outR;
        }
    }

private:
    // the paper's delay lengths are in samples at its 29761 Hz; they scale to our rate and Size
    static constexpr float kPaperRate = 29761.0f;
    static constexpr std::array<float, 4> kDiffuserLengths { 142, 107, 379, 277 };
    static constexpr std::array<float, 2> kModLengths { 672, 908 };
    static constexpr std::array<float, 2> kFirstDelays { 4453, 4217 };
    static constexpr std::array<float, 2> kSecondLengths { 1800, 2656 };
    static constexpr std::array<float, 2> kSecondDelays { 3720, 3163 };
    static constexpr float kExcursion = 12.0f; // the paper allows up to 16
    static constexpr float kSegmentLength = 5400.0f; // about the samples between two decay gains
    static constexpr float kInputDiffusion1 = 0.75f, kInputDiffusion2 = 0.625f, kDecayDiffusion1 = 0.7f;
    static constexpr float kMinSize = 0.35f, kMaxSize = 1.3f;
    static constexpr float kPreDelayMs = 15.0f, kLowCutHz = 200.0f, kLfoHz = 0.8f;
    static constexpr float kDuckLevel = 0.3f; // input level that ducks fully
    static constexpr float kOutputGain = 0.45f; // the paper uses 0.6; this matches the old room's level

    float fs = 48000.0f, rateScale = 1.0f;
    std::array<DelayLine, 4> diffusers;
    std::array<DelayLine, 2> modAllpass, firstDelay, secondAllpass, secondDelay;
    DelayLine preDelay;
    std::array<float, 2> lowCut {}, damp {}, tankOut {};
    float bandwidthState = 0.0f;
    float lowCutCoef = 0.0f, bandwidthCoef = 1.0f, dampCoef = 1.0f, decayGain = 0.5f, diffusion2 = 0.5f;
    float lfoPhase = 0.0f, lfoStep = 0.0f;
    float duckEnv = 0.0f, duckAttack = 0.0f, duckRelease = 0.0f, duckDepth = 0.0f;
    Smoothed size, mix;
    bool primed = false;
};

// One half-band FIR for a factor-two change of sample rate, so two of them in a row give four
// times. A half-band filter has every second tap zero apart from the centre, which halves the
// work. Linear phase, so the only cost is a fixed delay, which Drive makes up for in its dry
// path. The taps are a Blackman-windowed sinc computed in prepare().
class HalfBand
{
public:
    void prepare (int length) // length is 4m + 3
    {
        half = (length - 1) / 2;
        taps.assign ((size_t) half + 1, 0.0f);
        for (int d = 1; d <= half; d += 2)
        {
            const double t = (double) d * 0.5;
            const double window = 0.42 + 0.5 * std::cos (kPi * (double) d / (double) (half + 1))
                                + 0.08 * std::cos (2.0 * kPi * (double) d / (double) (half + 1));
            taps[(size_t) d] = (float) (std::sin (kPi * t) / (kPi * t) * 0.5 * window);
        }
        taps[0] = 0.5f;
        history.assign ((size_t) (2 * half + 1), 0.0f);
        reset();
    }

    void reset()
    {
        std::fill (history.begin(), history.end(), 0.0f);
        pos = 0;
    }

    int delay() const { return half; } // in samples at the faster rate, up and down alike

    // one sample in at the faster rate; the filtered one out (the caller keeps every second)
    float push (float x)
    {
        const int size = (int) history.size();
        if (--pos < 0)
            pos = size - 1;
        history[(size_t) pos] = x;
        auto at = [this, size] (int ago) { return history[(size_t) ((pos + ago) % size)]; };
        float sum = taps[0] * at (half);
        for (int d = 1; d <= half; d += 2)
            sum += taps[(size_t) d] * (at (half - d) + at (half + d));
        return sum;
    }

private:
    std::vector<float> taps, history;
    int half = 0, pos = 0;
};

// A stereo distortion with four times oversampling, so the sharp corners of Hard and the
// folds of Fold don't alias into notes that aren't there. Drive sets the gain into the
// shaper, and a level compensation worked out in prepare() (the shaper's output level for a
// sine, against its input) brings it back down as the drive goes up, so a high setting is
// denser rather than just louder. Mix is a dry/wet, with the dry path delayed to match the
// oversampling filters. Moving between the first and last place in the chain fades the wet
// signal out where it was and back in at the new place, so there is no click, and at Mix 0
// the signal isn't touched at all.
class Drive
{
public:
    void prepare (float sampleRate)
    {
        fs = sampleRate;
        for (auto& ch : channels)
        {
            ch.up1.prepare (kLongFilter);
            ch.up2.prepare (kShortFilter);
            ch.down2.prepare (kShortFilter);
            ch.down1.prepare (kLongFilter);
            ch.dry.assign (kDryRing, 0.0f);
        }
        // the round trip in samples at the base rate: the first pair of filters runs at twice
        // that rate and the second at four times, and the kept sample is two quarter-samples in
        dryDelay = std::max (1, (int) std::lround ((4.0 * channels[0].up1.delay() + 2.0 * channels[0].up2.delay() - 2.0) / 4.0));
        amount.prepare (fs);
        mix.prepare (fs);
        place.prepare (fs, 4.0f);
        bypass.prepare (fs, 5.0f);
        dcCoef = 1.0f - std::exp (-kTwoPi * 15.0f / fs);
        buildCompensation();
        reset();
    }

    void reset()
    {
        for (auto& ch : channels)
        {
            ch.up1.reset();
            ch.up2.reset();
            ch.down2.reset();
            ch.down1.reset();
            std::fill (ch.dry.begin(), ch.dry.end(), 0.0f);
            ch.dc = 0.0f;
        }
        writePos = 0;
        primed = false;
    }

    void setParams (DriveType newType, bool post, float newAmount, float newMix)
    {
        type = newType;
        wantPost = post;
        amount.target = std::clamp (newAmount, 0.0f, 1.0f);
        mix.target = std::clamp (newMix, 0.0f, 1.0f);
        if (! primed)
        {
            amount.jump (amount.target);
            mix.jump (mix.target);
            place.jump (1.0f);
            bypass.jump (mix.target <= 0.0f ? 1.0f : 0.0f);
            placedPost = wantPost;
            primed = true;
        }
        if (isIdle())
            placedPost = wantPost; // nothing is heard, so move without a fade
    }

    // where in the chain it sits at the moment; follows the setting once the wet signal has faded
    bool isPost() const { return placedPost; }

    void process (float* left, float* right, int numSamples)
    {
        if (channels[0].dry.empty())
            return;
        float* io[2] = { left, right };
        const int numChannels = right != nullptr ? 2 : 1;

        if (isIdle())
        {
            // not heard, but the dry delay is kept filled so it is ready when Mix goes up
            for (int i = 0; i < numSamples; ++i)
            {
                for (int c = 0; c < numChannels; ++c)
                    channels[(size_t) c].dry[(size_t) ((writePos + i) & kDryMask)] = io[c][i];
            }
            writePos = (writePos + numSamples) & kDryMask;
            mix.value = mix.target;
            return;
        }

        for (int i = 0; i < numSamples; ++i)
        {
            const float a = amount.next();
            const bool settled = mix.target <= 0.0f && mix.value < 1.0e-3f;
            bypass.target = settled ? 1.0f : 0.0f;
            place.target = wantPost == placedPost ? 1.0f : 0.0f;
            float dry, wet;
            mixGains (mix.next() * place.next(), dry, wet);
            const float b = bypass.next();

            const float gain = preGain (type, a), makeUp = compensation (a);
            for (int c = 0; c < numChannels; ++c)
            {
                auto& ch = channels[(size_t) c];
                const float in = io[c][i];
                ch.dry[(size_t) writePos] = in;
                const float delayed = ch.dry[(size_t) ((writePos - dryDelay) & kDryMask)];

                // up to four times the rate, shape, and back down. Each filter delays by its half
                // length, and the sample kept at the end is picked (the second of the four) so
                // that the whole round trip comes to a whole number of samples.
                float shaped = 0.0f;
                for (int k = 0; k < 2; ++k)
                {
                    const float u1 = ch.up1.push (k == 0 ? 2.0f * in : 0.0f);
                    float y = 0.0f;
                    for (int j = 0; j < 2; ++j)
                    {
                        const float v = ch.down2.push (shape (ch.up2.push (j == 0 ? 2.0f * u1 : 0.0f) * gain));
                        if (j == 0)
                            y = v;
                    }
                    const float d = ch.down1.push (y);
                    if (k == 1)
                        shaped = d;
                }
                // the shapers aren't symmetric, so take the DC they make out of the wet signal
                ch.dc += dcCoef * (shaped - ch.dc);
                const float wetSignal = (shaped - ch.dc) * makeUp;

                const float heard = dry * delayed + wet * wetSignal;
                io[c][i] = in * b + heard * (1.0f - b);
            }
            writePos = (writePos + 1) & kDryMask;
        }

        // the fade is done: now it can change place
        if (wantPost != placedPost && place.value < 1.0e-3f)
        {
            placedPost = wantPost;
            place.value = 0.0f;
        }
        if (bypass.value > 0.9999f && mix.target <= 0.0f && mix.value < 1.0e-3f)
        {
            bypass.value = 1.0f;
            mix.value = 0.0f;
            for (auto& ch : channels)
            {
                ch.up1.reset();
                ch.up2.reset();
                ch.down2.reset();
                ch.down1.reset();
                ch.dc = 0.0f;
            }
        }
    }

private:
    static constexpr int kLongFilter = 47, kShortFilter = 15; // taps of the pair at 2x and at 4x
    static constexpr int kDryRing = 128, kDryMask = kDryRing - 1;
    static constexpr int kTable = 33;
    static constexpr float kReferenceLevel = 0.3f; // the sine the compensation is worked out for
    // Fold stops sooner: past about 30 dB its folds are so many that the overtones run well past
    // what four times oversampling can carry, and what is left in band is less than it was worked out for
    static constexpr float kMaxDriveDb[] = { 48.0f, 48.0f, 30.0f };

    struct Channel
    {
        HalfBand up1, up2, down2, down1;
        std::vector<float> dry;
        float dc = 0.0f;
    };

    bool isIdle() const { return primed && bypass.target >= 1.0f && bypass.value >= 0.9999f && mix.target <= 0.0f; }

    static float preGain (DriveType t, float a) { return std::pow (10.0f, a * kMaxDriveDb[(size_t) t] / 20.0f); }

    float shape (float x) const { return shapeOf (type, x); }

    static float shapeOf (DriveType t, float x)
    {
        switch (t)
        {
            case DriveType::Warm: return std::tanh (x);
            case DriveType::Hard: return std::clamp (x, -0.85f, 1.0f); // the lower edge sooner: even harmonics, like a fuzz
            case DriveType::Fold: return std::sin (kPi * 0.5f * x);
        }
        return x;
    }

    float compensation (float a) const
    {
        const float at = std::clamp (a, 0.0f, 1.0f) * (float) (kTable - 1);
        const int i = std::min ((int) at, kTable - 2);
        const auto& row = makeUpTable[(size_t) type];
        return row[(size_t) i] + (row[(size_t) i + 1] - row[(size_t) i]) * (at - (float) i);
    }

    // For each type and drive, the gain that gives the shaper the same RMS out as in for the
    // reference sine; the tails and quiet notes come out relatively louder, which is the point.
    void buildCompensation()
    {
        constexpr int points = 256;
        for (int t = 0; t < kNumDriveTypes; ++t)
            for (int i = 0; i < kTable; ++i)
            {
                const float g = preGain ((DriveType) t, (float) i / (float) (kTable - 1));
                double in = 0.0, out = 0.0, mean = 0.0;
                for (int n = 0; n < points; ++n)
                {
                    const float x = kReferenceLevel * std::sin (kTwoPi * (float) n / (float) points);
                    const float y = shapeOf ((DriveType) t, x * g);
                    in += (double) x * x;
                    out += (double) y * y;
                    mean += y;
                }
                mean /= points;
                out = out / points - mean * mean; // without the DC, which is taken out later
                makeUpTable[(size_t) t][(size_t) i] = (float) std::sqrt (in / points / std::max (1.0e-12, out));
            }
    }

    float fs = 48000.0f, dcCoef = 0.0f;
    DriveType type = DriveType::Warm;
    bool wantPost = true, placedPost = true, primed = false;
    int writePos = 0, dryDelay = 1;
    std::array<Channel, 2> channels;
    std::array<std::array<float, kTable>, kNumDriveTypes> makeUpTable {};
    Smoothed amount, mix, place, bypass;
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
        drive.prepare (fs);
    }

    void reset()
    {
        filter.reset();
        crusher.reset();
        delay.reset();
        room.reset();
        drive.reset();
    }

    void setParams (const EffectParams& p, double bpm)
    {
        filter.setParams (p.filterType, p.cutoffHz, p.resonance);
        crusher.setParams (p.crushBits, p.crushRateHz);
        delay.setParams (p.delayNote, p.delayFeel, bpm, p.delayFeedback, p.delayMix);
        room.setParams (p.roomSize, p.roomDecay, p.roomTone, p.roomMix, p.roomDuck);
        drive.setParams (p.driveType, p.drivePost, p.driveAmount, p.driveMix);
    }

    // right may be null for a mono output
    void process (float* left, float* right, int numSamples)
    {
        const bool post = drive.isPost(); // may change while it fades, so asked once
        if (! post)
            drive.process (left, right, numSamples);
        filter.process (left, right, numSamples);
        crusher.process (left, right, numSamples);
        delay.process (left, right, numSamples);
        room.process (left, right, numSamples);
        if (post)
            drive.process (left, right, numSamples);
    }

private:
    Filter filter;
    Crusher crusher;
    Delay delay;
    Plate room;
    Drive drive;
};
} // namespace bleepwerk

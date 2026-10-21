// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "BleepSynth.h"
#include "Effects.h"
#include "FoleySynth.h"
#include "MachineSynth.h"

#include <cstring>
#include <utility>
#include <vector>

// A sound patch: every parameter except the pitch bend range. The table below is the one place
// their ranges and defaults are set; the plug-in builds its parameters from it and the tests
// check the factory presets against it. readPatch() turns parameter values into the engines'
// settings, for the plug-in and for rendering a patch offline (tests, loudness matching).
// JUCE-free.
namespace bleepforge
{
enum class Engine { Bleep, Foley, Machine };
inline constexpr const char* kEngineNames[] = { "Bleep", "Foley", "Machine" };
inline constexpr int kNumEngines = (int) std::size (kEngineNames);

struct SoundParam
{
    const char* id;
    float min, max, defaultValue;
};

// Choices run from 0 to their count - 1.
inline constexpr SoundParam kSoundParams[] = {
    { "engine", 0, kNumEngines - 1, 0 },

    { "bleepWave", 0, kNumBleepWaves - 1, 0 },
    { "bleepPulseWidth", 0.05f, 0.95f, 0.5f },
    { "bleepFmRatio", 0.5f, 8.0f, 2.0f },
    { "bleepFmAmount", 0, 1, 0.3f },
    { "bleepPitchAmount", -24, 48, 12 },
    { "bleepPitchDecay", 1, 500, 25 },
    { "bleepAttack", 0, 50, 0.5f },
    { "bleepDecay", 5, 2000, 180 },
    { "bleepRelease", 1, 500, 30 },
    { "bleepNoise", 0, 1, 0 },

    { "foleyExciter", 0, kNumFoleyExciters - 1, (float) FoleyExciter::Mallet },
    { "foleyLength", 0.05f, 200, 0.3f },
    { "foleyMaterial", 0, 1, 0 },
    { "foleySize", 0, 1, 0.5f },
    { "foleyDamping", 0, 1, 0.5f },
    { "foleyBrightness", 0, 1, 0.5f },
    { "foleySweep", -24, 24, 0 },

    { "machineType", 0, kNumMachineTypes - 1, (float) MachineType::Hit },
    { "machineLength", 10, 6000, 400 },
    { "machineTone", 0, 1, 0.5f },
    { "machineMotion", 0, 1, 0 },
    { "machineFocus", 0, 1, 0.5f },
    { "machineMetal", 0, 1, 0.5f },

    { "chaosAmount", 0, 1, 0.0f }, // off until asked for: presets and new instances play the same every hit
    { "chaosPitch", 0, 1, 0.2f },
    { "chaosDecay", 0, 1, 0.5f },
    { "chaosTone", 0, 1, 0.5f },
    { "chaosPan", 0, 1, 0.5f },
    { "chaosVelocity", 0, 1, 0.3f },

    { "voiceMode", 0, 1, 1 }, // Mono, Poly
    { "glide", 0, 500, 0 },

    { "filterType", 0, kNumFilterTypes - 1, 0 },
    { "filterCutoff", 20, 20000, 2000 },
    { "filterResonance", 0, 1, 0.2f },
    { "crushBits", 1, kCrushBitsOff, kCrushBitsOff },
    { "crushRate", 200, kCrushRateOff, kCrushRateOff },
    { "delayTime", 0, kNumDelayTimes - 1, kDefaultDelayTime },
    { "delayFeedback", 0, 0.95f, 0.35f },
    { "delayMix", 0, 1, 0 },
    { "roomSize", 0, 1, 0.4f },
    { "roomDecay", 0.3f, 6, 1.2f }, // seconds
    { "roomTone", 0, 1, 0.5f },
    { "roomMix", 0, 1, 0 },
    { "roomDuck", 0, 1, 0 },

    { "outputGain", -48, 12, 0 },
};

inline const SoundParam* findSoundParam (const char* id)
{
    for (const auto& p : kSoundParams)
        if (std::strcmp (p.id, id) == 0)
            return &p;
    return nullptr;
}

inline float soundDefault (const char* id)
{
    const auto* p = findSoundParam (id);
    return p != nullptr ? p->defaultValue : 0.0f;
}

struct Patch
{
    Engine engine = Engine::Bleep;
    BleepParams bleep;
    FoleyParams foleyParams;
    MachineParams machine;
    ChaosDepths chaos;
    bool mono = false;
    float glideMs = 0.0f;
    EffectParams effects;
    float outputDb = 0.0f;
};

// get (id) returns a parameter's current value in its own units.
template <typename Get>
Patch readPatch (Get&& get)
{
    auto index = [&get] (const char* id, int count) { return std::clamp ((int) std::lround (get (id)), 0, count - 1); };

    Patch patch;
    patch.engine = (Engine) index ("engine", kNumEngines);

    auto& b = patch.bleep;
    b.wave = (BleepWave) index ("bleepWave", kNumBleepWaves);
    b.pulseWidth = get ("bleepPulseWidth");
    b.fmRatio = get ("bleepFmRatio");
    b.fmAmount = get ("bleepFmAmount");
    b.pitchEnvSemis = get ("bleepPitchAmount");
    b.pitchEnvDecayMs = get ("bleepPitchDecay");
    b.attackMs = get ("bleepAttack");
    b.decayMs = get ("bleepDecay");
    b.releaseMs = get ("bleepRelease");
    b.noise = get ("bleepNoise");

    auto& f = patch.foleyParams;
    f.exciter = (FoleyExciter) index ("foleyExciter", kNumFoleyExciters);
    f.exciterMs = get ("foleyLength");
    f.material = get ("foleyMaterial");
    f.size = get ("foleySize");
    f.damping = get ("foleyDamping");
    f.brightness = get ("foleyBrightness");
    f.sweepSemis = get ("foleySweep");

    auto& m = patch.machine;
    m.type = (MachineType) index ("machineType", kNumMachineTypes);
    m.lengthMs = get ("machineLength");
    m.tone = get ("machineTone");
    m.motion = get ("machineMotion");
    m.focus = get ("machineFocus");
    m.metal = get ("machineMetal");

    // Amount scales every target's depth
    const float amount = get ("chaosAmount");
    patch.chaos.pitch = amount * get ("chaosPitch");
    patch.chaos.decay = amount * get ("chaosDecay");
    patch.chaos.tone = amount * get ("chaosTone");
    patch.chaos.pan = amount * get ("chaosPan");
    patch.chaos.velocity = amount * get ("chaosVelocity");

    patch.mono = index ("voiceMode", 2) == 0;
    patch.glideMs = get ("glide");

    auto& e = patch.effects;
    e.filterType = (FilterType) index ("filterType", kNumFilterTypes);
    e.cutoffHz = get ("filterCutoff");
    e.resonance = get ("filterResonance");
    e.crushBits = get ("crushBits");
    e.crushRateHz = get ("crushRate");
    e.delayTime = index ("delayTime", kNumDelayTimes);
    e.delayFeedback = get ("delayFeedback");
    e.delayMix = get ("delayMix");
    e.roomSize = get ("roomSize");
    e.roomDecay = get ("roomDecay");
    e.roomTone = get ("roomTone");
    e.roomMix = get ("roomMix");
    e.roomDuck = get ("roomDuck");

    patch.outputDb = get ("outputGain");
    return patch;
}

// Plays a short phrase through a patch offline, the same way the plug-in would: four 1/16
// notes at 120 BPM rising over an octave from middle C, then the tail. Stereo, before the
// output gain. Chaos is left out, so the result is the same every time.
inline std::pair<std::vector<float>, std::vector<float>> renderPatch (const Patch& patch, double sampleRate = 48000.0,
                                                                      double seconds = 2.0)
{
    const int numSamples = (int) (seconds * sampleRate);
    std::vector<float> left ((size_t) numSamples, 0.0f), right ((size_t) numSamples, 0.0f);

    BleepSynth bleep;
    FoleySynth foley;
    MachineSynth machine;
    Effects effects;
    bleep.prepare (sampleRate);
    foley.prepare (sampleRate);
    machine.prepare (sampleRate);
    effects.prepare (sampleRate);
    bleep.setParams (patch.bleep);
    foley.setParams (patch.foleyParams);
    machine.setParams (patch.machine);
    bleep.setMode (patch.mono ? BleepSynth::Mode::Mono : BleepSynth::Mode::Poly);
    foley.setMode (patch.mono ? FoleySynth::Mode::Mono : FoleySynth::Mode::Poly);
    machine.setMode (patch.mono ? MachineSynth::Mode::Mono : MachineSynth::Mode::Poly);
    bleep.setGlideMs (patch.glideMs);
    foley.setGlideMs (patch.glideMs);
    machine.setGlideMs (patch.glideMs);
    effects.setParams (patch.effects, 120.0);

    constexpr int notes[] = { 60, 64, 67, 72 };
    const int step = (int) (0.125 * sampleRate), gate = (int) (0.1 * sampleRate);
    auto noteOn = [&] (int note)
    {
        switch (patch.engine)
        {
            case Engine::Bleep: bleep.noteOn (note, 0.8f); break;
            case Engine::Foley: foley.noteOn (note, 0.8f); break;
            case Engine::Machine: machine.noteOn (note, 0.8f); break;
        }
    };
    auto noteOff = [&] (int note)
    {
        bleep.noteOff (note);
        foley.noteOff (note);
        machine.noteOff (note);
    };

    // events in time order: each note's on at k * step, its off a gate later
    std::vector<std::pair<int, int>> events; // sample, +note for on / -note - 1 for off
    for (int k = 0; k < 4; ++k)
    {
        events.push_back ({ k * step, notes[k] });
        events.push_back ({ k * step + gate, -notes[k] - 1 });
    }
    std::sort (events.begin(), events.end());

    int position = 0;
    auto renderTo = [&] (int to)
    {
        for (; position < to; position += 256)
        {
            const int n = std::min (256, to - position);
            bleep.renderAdding (left.data() + position, right.data() + position, n);
            foley.renderAdding (left.data() + position, right.data() + position, n);
            machine.renderAdding (left.data() + position, right.data() + position, n);
            effects.process (left.data() + position, right.data() + position, n);
        }
        position = to;
    };
    for (const auto& [at, event] : events)
    {
        renderTo (std::min (at, numSamples));
        event >= 0 ? noteOn (event) : noteOff (-event - 1);
    }
    renderTo (numSamples);
    return { std::move (left), std::move (right) };
}

struct PatchLevel
{
    float loudest = 0.0f; // RMS of the loudest 50 ms
    float peak = 0.0f;
};

inline PatchLevel measurePatch (const Patch& patch, double sampleRate = 48000.0)
{
    const auto [left, right] = renderPatch (patch, sampleRate);
    const size_t window = (size_t) (0.05 * sampleRate);
    PatchLevel level;
    for (size_t start = 0; start + window <= left.size(); start += window / 2)
    {
        double sum = 0.0;
        for (size_t i = start; i < start + window; ++i)
        {
            const float mid = 0.5f * (left[i] + right[i]);
            sum += (double) mid * mid;
            level.peak = std::max ({ level.peak, std::abs (left[i]), std::abs (right[i]) });
        }
        level.loudest = std::max (level.loudest, (float) std::sqrt (sum / (double) window));
    }
    return level;
}

// The output gain that brings a patch to the same loudness as the others, without its peaks
// going above -1 dB. Short ticks are held back by the peak limit rather than pushed up.
inline constexpr float kTargetLoudness = 0.2f, kMaxPeak = 0.89f;

inline float matchedOutputDb (const Patch& patch)
{
    const auto level = measurePatch (patch);
    if (level.loudest < 1.0e-6f)
        return 0.0f;
    const float gain = std::min (kTargetLoudness / level.loudest, kMaxPeak / level.peak);
    return std::clamp (20.0f * std::log10 (gain), -24.0f, 12.0f);
}
} // namespace bleepforge

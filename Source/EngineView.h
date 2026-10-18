// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "Controls.h"

#include <optional>

// The sound section below the grid. Left: the engine (Bleep or Foley), Mono or Poly, Glide and
// Output. Right: the controls of the selected engine, which swap when the engine changes, and
// by the engine's name the dice, which roll a random sound for that engine, with a back arrow.
class EngineView : public juce::Component, private juce::Timer
{
public:
    explicit EngineView (BleepForgeProcessor&);
    ~EngineView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showEngine (bool foley);
    void rollSound();
    void undoRoll();

    BleepForgeProcessor& processor;
    Toggle engine, voices;
    Knob glide, output;

    Choice bleepWave;
    Knob pulseWidth, fmRatio, fmAmount, pitchAmount, pitchDecay, attack, decay, release, noise;
    Choice foleyExciter;
    Knob foleyLength, material, size, damping, brightness, sweep;

    // the dice: a random sound for the engine in view, and back to the sound before it
    IconButton dice { IconButton::Icon::Dice, "Random sound for this engine" },
        back { IconButton::Icon::Back, "Back to the sound before the last roll" };
    struct SoundBefore
    {
        int entry = 0;
        juce::String name;
        std::vector<float> values; // in kSoundParams order
    };
    std::optional<SoundBefore> beforeRoll;

    std::vector<juce::Component*> bleepControls, foleyControls;
    int shownEngine = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EngineView)
};

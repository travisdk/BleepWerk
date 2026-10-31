// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "Controls.h"

#include <optional>

// The sound section. Left: the engine (Bleep or Strike), and below it
// Mono or Poly, Glide, Bend and Output. Right: the controls of the selected engine, which swap when the engine changes, and
// by the engine's name the dice, which roll a random sound for that engine, with a back arrow.
// Strike has two rows of buttons, what hits and what is hit, above its knobs.
class EngineView : public juce::Component, private juce::Timer
{
public:
    explicit EngineView (BleepForgeProcessor&);
    ~EngineView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showEngine (int engineIndex);
    void rollSound();
    void undoRoll();

    BleepForgeProcessor& processor;
    Toggle engine, voices;
    Knob glide, bend, output;

    Choice bleepWave;
    Knob pulseWidth, fmRatio, fmAmount, pitchAmount, pitchDecay, attack, decay, release, noise;
    Toggle exciter, body;
    Knob strikeLength, ring, strikeTone, motion, sweep;

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

    bool isKey (const std::vector<juce::Component*>* list, size_t index) const;

    std::vector<juce::Component*> bleepControls, strikeControls;
    std::vector<size_t> bleepKeys, strikeKeys; // indices of the larger knobs
    int shownEngine = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EngineView)
};

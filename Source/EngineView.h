// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// The sound section below the grid. Left: the engine (Bleep or Foley), Mono or Poly, Glide and
// Output. Right: the controls of the selected engine, which swap when the engine changes.
class EngineView : public juce::Component, private juce::Timer
{
public:
    explicit EngineView (BleepForgeProcessor&);
    ~EngineView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // A rotary control for one parameter: name above, value below; double-click resets it.
    struct Knob : juce::Component
    {
        Knob (juce::AudioProcessorValueTreeState&, const juce::String& parameterId, const juce::String& name, bool bipolar = false);
        void resized() override;

        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    };

    // A choice parameter shown as a drop-down with its name above.
    struct Choice : juce::Component
    {
        Choice (juce::AudioProcessorValueTreeState&, const juce::String& parameterId, const juce::String& name);
        void resized() override;

        juce::ComboBox box;
        juce::Label label;
        juce::AudioProcessorValueTreeState::ComboBoxAttachment attachment;
    };

    // Two buttons side by side for a two-way choice parameter.
    struct Toggle : juce::Component
    {
        Toggle (juce::AudioProcessorValueTreeState&, const juce::String& parameterId, const juce::String& name);
        void resized() override;
        void update(); // follows the parameter

        juce::RangedAudioParameter& parameter;
        juce::Label label;
        juce::TextButton left, right;
    };

    void timerCallback() override;
    void showEngine (bool foley);

    BleepForgeProcessor& processor;
    Toggle engine, voices;
    Knob glide, output;

    Choice bleepWave;
    Knob pulseWidth, fmRatio, fmAmount, pitchAmount, pitchDecay, attack, decay, release, noise;
    Choice foleyExciter;
    Knob foleyLength, material, size, damping, brightness, sweep;

    std::vector<juce::Component*> bleepControls, foleyControls;
    int shownEngine = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EngineView)
};

// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// The controls the sound panels are built from, each tied to one parameter.

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

// A small icon button drawn without fonts or images: a die (roll random values) or a back
// arrow (undo the last roll). Dimmed while disabled.
struct IconButton : juce::Button
{
    enum class Icon { Dice, Back };
    IconButton (Icon, const juce::String& tooltip);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

    Icon icon;
};

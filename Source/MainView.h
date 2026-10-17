// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "GridView.h"

// Everything in the editor, laid out at a fixed design size; the editor scales it as a whole.
// Top: title and the sequencer's Scale, Root and Octave. Middle: the grid and a status line.
// Bottom: the engine section (later milestones).
class MainView : public juce::Component
{
public:
    static constexpr int kWidth = 960, kHeight = 600;

    explicit MainView (BleepForgeProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    juce::Rectangle<int> headerArea() const;
    juce::Rectangle<int> engineArea() const;
    void showStatus (const juce::String&);

    BleepForgeProcessor& processor;
    GridView gridView;
    juce::Label status;

    juce::Label scaleLabel { {}, "Scale" }, rootLabel { {}, "Root" }, octaveLabel { {}, "Octave" };
    juce::ComboBox scaleBox, rootBox;
    juce::Slider octaveSlider { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft };
    std::unique_ptr<ComboAttachment> scaleAttachment, rootAttachment;
    std::unique_ptr<SliderAttachment> octaveAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainView)
};

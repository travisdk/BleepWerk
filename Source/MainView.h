// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "GridView.h"
#include "EngineView.h"
#include "ChaosView.h"
#include "EffectsView.h"

// Everything in the editor, laid out at a fixed design size; the editor scales it as a whole.
// Top: title, edit mode and a settings menu (visible range). Below it the sequencer bar:
// Scale, Root, Steps, Rate and Swing. Middle: the grid and a status line. Bottom: the sound:
// the engine panel, then Chaos and the effects.
class MainView : public juce::Component, private juce::Timer
{
public:
    static constexpr int kWidth = 960, kHeight = 760;

    explicit MainView (BleepForgeProcessor&);
    ~MainView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    void timerCallback() override;
    juce::Rectangle<int> headerArea() const;
    void showStatus (const juce::String&);
    void setEditMode (BleepForgeProcessor::EditMode);
    void showSettingsMenu();

    // A gear icon, drawn so it needs no font or image.
    struct GearButton : juce::Button
    {
        GearButton() : juce::Button ("Settings") {}
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    };

    BleepForgeProcessor& processor;
    GridView gridView;
    EngineView engineView;
    ChaosView chaosView;
    EffectsView effectsView;
    juce::Label status;
    juce::Label editLabel { {}, "Edit" };
    juce::TextButton probabilityButton { "Probability" }, velocityButton { "Velocity" }, ratchetButton { "Ratchet" };
    GearButton settingsButton;

    // the sequencer bar
    juce::Label scaleLabel { {}, "Scale" }, rootLabel { {}, "Root" }, stepsLabel { {}, "Steps" },
        rateLabel { {}, "Rate" }, swingLabel { {}, "Swing" };
    juce::ComboBox scaleBox, rootBox, rateBox;
    juce::Slider stepsSlider { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft }; // the grid's length, not a parameter
    juce::Slider swingSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    std::unique_ptr<ComboAttachment> scaleAttachment, rootAttachment, rateAttachment;
    std::unique_ptr<SliderAttachment> swingAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainView)
};

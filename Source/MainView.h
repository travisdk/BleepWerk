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
// Top: title, edit mode, the sound preset browser and a settings menu (visible range, preset
// files). Below it the sequencer bar:
// Scale, Root, Steps, Rate, Swing and the pattern dice. Middle: the grid and a status line,
// with Cycle and Seed at its right. Bottom: the sound:
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
    void showPresetMenu();
    void stepPreset (int delta);
    void refreshPreset();
    void rollPattern();
    void undoPatternRoll();
    void savePreset (const juce::String& name);
    void showNameDialog (const juce::String& suggestion, std::function<void (const juce::String&)> onName);
    void confirm (const juce::String& title, const juce::String& message, const juce::String& okText, std::function<void()> onOk);

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
    juce::TextButton presetPrev { "<" }, presetButton, presetNext { ">" }, saveButton { "Save" };
    int shownPresetVersion = -1;

    // the sequencer bar
    juce::Label scaleLabel { {}, "Scale" }, rootLabel { {}, "Root" }, stepsLabel { {}, "Steps" },
        rateLabel { {}, "Rate" }, swingLabel { {}, "Swing" };
    juce::ComboBox scaleBox, rootBox, rateBox;
    juce::Slider stepsSlider { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft }; // the grid's length, not a parameter
    juce::Slider swingSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    std::unique_ptr<ComboAttachment> scaleAttachment, rootAttachment, rateAttachment;
    std::unique_ptr<SliderAttachment> swingAttachment;

    // under the grid, right of the status line: Cycle and Seed (only used with a cycle)
    juce::Label cycleLabel { {}, "Cycle" }, seedLabel { {}, "Seed" };
    juce::ComboBox cycleBox;
    juce::Slider seedSlider { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft };
    std::unique_ptr<ComboAttachment> cycleAttachment;
    std::unique_ptr<SliderAttachment> seedAttachment;
    IconButton patternDice { IconButton::Icon::Dice, "Random pattern in the rows in view" },
        patternBack { IconButton::Icon::Back, "Back to the pattern before the last roll" };
    std::unique_ptr<bleepforge::Grid> patternBeforeRoll;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainView)
};

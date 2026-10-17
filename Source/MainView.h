// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "GridView.h"
#include "EngineView.h"

// Everything in the editor, laid out at a fixed design size; the editor scales it as a whole.
// Top: title, edit mode, the sequencer's Scale and Root, and a settings menu (visible range).
// Middle: the grid and a status line.
// Bottom: the sound: engine, voices and the selected engine's controls.
class MainView : public juce::Component
{
public:
    static constexpr int kWidth = 960, kHeight = 600;

    explicit MainView (BleepForgeProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    juce::Rectangle<int> headerArea() const;
    juce::Rectangle<int> engineArea() const;
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
    juce::Label status;
    juce::Label editLabel { {}, "Edit" };
    juce::TextButton probabilityButton { "Probability" }, velocityButton { "Velocity" };

    juce::Label scaleLabel { {}, "Scale" }, rootLabel { {}, "Root" };
    juce::ComboBox scaleBox, rootBox;
    GearButton settingsButton;
    std::unique_ptr<ComboAttachment> scaleAttachment, rootAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainView)
};

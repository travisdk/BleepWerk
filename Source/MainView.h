// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "EngineView.h"
#include "ChaosView.h"
#include "DriveView.h"
#include "EffectsView.h"
#include "SequencerView.h"

// Everything in the editor, laid out at a fixed design size; the editor scales it as a whole.
// Top: title, the sound preset browser and a settings menu (colour theme, preset files).
// Below it the engine panel, then Chaos and the effects. Notes come from the host.
class MainView : public juce::Component, private juce::Timer
{
public:
    static constexpr int kWidth = 860, kHeight = 568; // 16 + 40 + 10 + 212 + 10 + 128 + 10 + 120 + 16
    static constexpr int kSequencerExtra = 10 + SequencerView::kHeight; // the sequencer panel and the gap above it

    // The height at the design size: taller while the sequencer is shown. The editor follows
    // when it changes.
    int designHeight() const { return kHeight + (sequencerShown ? kSequencerExtra : 0); }
    std::function<void()> onDesignSizeChanged;

    explicit MainView (BleepForgeProcessor&);
    ~MainView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void lookAndFeelChanged() override; // the theme changed

private:
    void timerCallback() override;
    void chooseTheme (int index);
    void refreshSequencer();
    juce::Rectangle<int> headerArea() const;
    void showSettingsMenu();
    void showPresetMenu();
    void stepPreset (int delta);
    void refreshPreset();
    void savePreset (const juce::String& name);
    void savePattern (const juce::String& name);
    void showNameDialog (const juce::String& title, const juce::String& where, const juce::String& suggestion,
                         std::function<void (const juce::String&)> onName);
    void confirm (const juce::String& title, const juce::String& message, const juce::String& okText, std::function<void()> onOk);

    // A gear icon, drawn so it needs no font or image.
    struct GearButton : juce::Button
    {
        GearButton() : juce::Button ("Settings") {}
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    };

    BleepForgeProcessor& processor;
    EngineView engineView;
    ChaosView chaosView;
    DriveView driveView;
    EffectsView effectsView;
    SequencerView sequencerView;
    juce::TextButton sequencerButton { "SEQ" };
    bool sequencerShown = false;
    GearButton settingsButton;
    juce::TextButton presetPrev { "<" }, presetButton, presetNext { ">" }, saveButton { "Save" };
    int shownPresetVersion = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainView)
};

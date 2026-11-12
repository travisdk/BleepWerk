// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "MainView.h"
#include "Theme.h"

// Hosts MainView and scales it as a whole between 75 % and 200 % of its design size, keeping
// the aspect ratio while the resize corner is dragged.
class BleepForgeEditor : public juce::AudioProcessorEditor
{
public:
    explicit BleepForgeEditor (BleepForgeProcessor&);
    ~BleepForgeEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;

private:
    void applyDesignSize (float scale);

    BleepForgeProcessor& processor;
    bleepforge::theme::LookAndFeel lookAndFeel;
    MainView view;
    // The window for the controls' tooltips. Off in the settings, it shows nothing.
    struct TipWindow : juce::TooltipWindow
    {
        TipWindow (juce::Component* parent, int delayMs, std::function<bool()> isOn)
            : juce::TooltipWindow (parent, delayMs), showing (std::move (isOn)) {}
        juce::String getTipFor (juce::Component& c) override
        {
            return showing() ? juce::TooltipWindow::getTipFor (c) : juce::String();
        }
        std::function<bool()> showing;
    };
    TipWindow tooltips { this, 600, [this] { return processor.getSettings().getBoolValue ("tooltips", true); } };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BleepForgeEditor)
};

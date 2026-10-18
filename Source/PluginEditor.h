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
    BleepForgeProcessor& processor;
    bleepforge::theme::LookAndFeel lookAndFeel;
    MainView view;
    juce::TooltipWindow tooltips { this, 600 }; // the buttons' tooltips

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BleepForgeEditor)
};

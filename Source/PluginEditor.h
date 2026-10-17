// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

// Placeholder editor: the grid (top) and engine section (bottom) arrive in later milestones.
// Drawn at a fixed design size and scaled as a whole, so the window resizes with a fixed
// aspect ratio between 75 % and 200 %.
class BleepForgeEditor : public juce::AudioProcessorEditor
{
public:
    explicit BleepForgeEditor (BleepForgeProcessor&);

    void paint (juce::Graphics&) override;
    void parentHierarchyChanged() override;

    static constexpr int kWidth = 960, kHeight = 600;

private:
    BleepForgeProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BleepForgeEditor)
};

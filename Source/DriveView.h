// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "Controls.h"

// Drive, the distortion, in a panel of its own with larger knobs and its own accent colour, so it
// stands out from the rest of the effects. It sits first or last in the chain by the Pre/Post
// switch, which says where; the panel itself stays where it is.
class DriveView : public juce::Component, private juce::Timer
{
public:
    static constexpr int kDiameter = 64; // the knobs here and in Chaos

    explicit DriveView (BleepForgeProcessor&);
    ~DriveView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void lookAndFeelChanged() override;

private:
    void timerCallback() override;

    Choice type;
    Toggle place;
    Knob amount, mix;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DriveView)
};

// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "Controls.h"

// The effects, each a small group with its name above: Filter, Crush and Delay on the first row,
// Reverb and Drive on the second. Drive sits first or last in the chain depending on its Pre/Post
// switch, but keeps its place here. Cutoff and Resonance grey out while the filter is Off.
class EffectsView : public juce::Component, private juce::Timer
{
public:
    explicit EffectsView (BleepForgeProcessor&);
    ~EffectsView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    BleepForgeProcessor& processor;
    Choice filterType;
    Knob cutoff, resonance, bits, rate, delayTime, feedback, delayMix, roomSize, roomDecay, roomTone, roomMix, roomDuck;
    Choice driveType;
    Knob driveAmount, driveMix;
    Toggle delayFeel; // straight, triplet or dotted, stacked beside Time
    Toggle drivePost; // before the filter or after the reverb, stacked beside Type

public:
    static constexpr int kHeight = 232; // two rows and the padding round them

private:
    // the groups as laid out, for paint() to put the names and dividers on
    struct Group { const char* name; juce::Rectangle<int> area; bool divider = false; };
    std::array<Group, 5> groups { { { "FILTER", {} }, { "CRUSH", {} }, { "DELAY", {} }, { "REVERB", {} }, { "DRIVE", {} } } };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectsView)
};

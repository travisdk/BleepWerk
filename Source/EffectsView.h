// Copyright (C) 2026 Henrik Juhl. Part of BleepWerk, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "Controls.h"

// The effects in signal order on one row, each a small group with its name above: Filter, Crush,
// Delay and Reverb. Drive, which sits first or last, has its own panel. Cutoff and Resonance
// grey out while the filter is Off.
class EffectsView : public juce::Component, private juce::Timer
{
public:
    static constexpr int kHeight = 120;

    explicit EffectsView (BleepWerkProcessor&);
    ~EffectsView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    BleepWerkProcessor& processor;
    Choice filterType;
    Knob cutoff, resonance, bits, rate, delayTime, feedback, delayMix, roomSize, roomDecay, roomTone, roomMix, roomDuck;
    Toggle delayFeel; // straight, triplet or dotted, stacked beside Time

    // the groups as laid out, for paint() to put the names and dividers on
    struct Group { const char* name; juce::Rectangle<int> area; };
    std::array<Group, 4> groups { { { "FILTER", {} }, { "CRUSH", {} }, { "DELAY", {} }, { "REVERB", {} } } };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectsView)
};

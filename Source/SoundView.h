// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "SoundShape.h"

// A small picture of the sound in the engine panel, redrawn as the knobs move: for the Bleep its
// amplitude envelope with the pitch envelope over it, for Foley and Machine the tones of the
// resonator: how the sound rings out over time, and its tones from low to high as bars.
// Only to look at; it has no controls.
class SoundView : public juce::Component, private juce::Timer
{
public:
    explicit SoundView (BleepForgeProcessor&);
    ~SoundView() override;

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    void paintBleep (juce::Graphics&, juce::Rectangle<float>) const;
    void paintModes (juce::Graphics&, juce::Rectangle<float>) const;
    void paintRing (juce::Graphics&, juce::Rectangle<float>, const std::vector<bleepforge::ShapeMode>&) const;
    void paintTones (juce::Graphics&, juce::Rectangle<float>, const std::vector<bleepforge::ShapeMode>&, bool screech) const;

    BleepForgeProcessor& processor;
    bleepforge::Patch patch;
    std::vector<float> lastValues;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundView)
};

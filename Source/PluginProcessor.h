// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "BleepSynth.h"

class BleepForgeProcessor : public juce::AudioProcessor
{
public:
    BleepForgeProcessor();
    ~BleepForgeProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; } // sequencer notes to the host (MIDI output mode)
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; } // longest release

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

private:
    void handleMidi (const juce::MidiMessage& message);
    void readParams();

    // Saved with the plug-in state so later versions can migrate older projects.
    //   1: 0.1.0
    static constexpr int kStateVersion = 1;

    bleepforge::BleepSynth bleep;
    std::vector<float> mono; // one block of engine output, sized in prepareToPlay
    juce::LinearSmoothedValue<float> outputGain { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BleepForgeProcessor)
};

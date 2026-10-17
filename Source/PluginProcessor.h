// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "BleepSynth.h"
#include "Sequencer.h"

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
    bool acceptsMidi() const override { return false; } // plays its own sequencer only
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

    // The pattern, edited by the GUI while the audio thread plays it.
    bleepforge::Grid& getGrid() { return grid; }
    const bleepforge::SequencerEngine& getSequencer() const { return sequencer; }
    bleepforge::SequencerSettings sequencerSettings() const; // scale and root from the parameters

    // What dragging a cell edits; kept here so it survives closing the editor.
    enum class EditMode { Probability, Velocity };
    std::atomic<EditMode> editMode { EditMode::Probability };
    std::atomic<int> firstVisibleRow { 0 }; // the grid's bottom row while browsing a large range

    // Tells the host the project changed after a grid edit (the grid isn't a parameter).
    void gridEdited();

    juce::AudioProcessorValueTreeState apvts;

private:
    void handleNote (const juce::MidiMessage& message);
    void readParams();
    void runSequencer (int numSamples);

    // Saved with the plug-in state so later versions can migrate older projects.
    //   1: 0.1.0
    static constexpr int kStateVersion = 1;

    bleepforge::BleepSynth bleep;
    bleepforge::Grid grid;
    bleepforge::SequencerEngine sequencer;
    juce::MidiBuffer seqMidi; // the sequencer's notes for one block
    std::vector<float> mono; // one block of engine output, sized in prepareToPlay
    juce::LinearSmoothedValue<float> outputGain { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BleepForgeProcessor)
};

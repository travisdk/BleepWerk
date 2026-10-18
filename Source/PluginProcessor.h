// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Patch.h"
#include "Sequencer.h"
#include "SoundPresets.h"

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
    double getTailLengthSeconds() const override { return 6.0; } // a big, undamped metal object

    // Host programs are the factory sound presets only.
    int getNumPrograms() override { return soundPresets.numFactory(); }
    int getCurrentProgram() override { return soundPresets.isUser (soundPresets.current()) ? 0 : juce::jmax (0, soundPresets.current()); }
    void setCurrentProgram (int index) override { soundPresets.load (index); }
    const juce::String getProgramName (int index) override { return soundPresets.entryName (index); }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // The pattern, edited by the GUI while the audio thread plays it.
    bleepforge::Grid& getGrid() { return grid; }
    const bleepforge::SequencerEngine& getSequencer() const { return sequencer; }
    bleepforge::SequencerSettings sequencerSettings() const; // scale and root from the parameters
    static constexpr int kMaxVisibleOctaves = 4;

    // What dragging a cell edits; kept here so it survives closing the editor.
    enum class EditMode { Probability, Velocity, Ratchet };
    std::atomic<EditMode> editMode { EditMode::Probability };
    std::atomic<int> bottomNote { -1 }; // the grid's lowest note in view; -1 until first shown

    // Tells the host the project changed after a grid edit (the grid isn't a parameter).
    void gridEdited();

    // Plays a note briefly so a grid edit can be heard, also while the host is stopped. Only
    // the built-in sound plays it, never the MIDI output. Callable from the GUI thread; a newer
    // request replaces one the audio thread hasn't picked up yet.
    void previewNote (int note, float velocity);
    bool previewEnabled() const { return previewOn.load(); }
    void setPreviewEnabled (bool on); // a per-user preference

    juce::AudioProcessorValueTreeState apvts;
    juce::PropertiesFile& getSettings() { return userSettings; } // per-user preferences

    // Never touches the sequencer: a sound can be changed under a pattern.
    SoundPresets& getSoundPresets() { return soundPresets; }

private:
    void handleNote (const juce::MidiMessage& message);
    void readParams();
    void runSequencer (int numSamples);
    void runPreview (int numSamples);

    juce::PropertiesFile userSettings;

    // Saved with the plug-in state so later versions can migrate older projects.
    //   1: 0.1.0
    static constexpr int kStateVersion = 1;

    // Both engines render every block, so switching lets the old one ring out; notes go to
    // the selected one.
    bleepforge::BleepSynth bleep;
    bleepforge::FoleySynth foley;
    bleepforge::Patch patch; // this block's sound parameters
    SoundPresets soundPresets { apvts };
    bleepforge::Effects effects;
    double hostBpm = 120.0; // for the delay's synced time, also while the host is stopped
    bleepforge::Grid grid;
    bleepforge::SequencerEngine sequencer;
    juce::MidiBuffer seqMidi; // the sequencer's notes for one block
    juce::LinearSmoothedValue<float> outputGain { 1.0f };

    // A preview request packed into one word so the GUI can hand it over without a lock:
    // bit 16 set means pending, bits 8-15 the velocity (1-127), bits 0-7 the note.
    std::atomic<uint32_t> previewRequest { 0 };
    std::atomic<bool> previewOn { true };
    int previewPlaying = -1; // the preview note that is sounding, owned by the audio thread
    int previewSamplesLeft = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BleepForgeProcessor)
};

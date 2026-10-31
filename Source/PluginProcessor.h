// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Patch.h"
#include "NoteInput.h"
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
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    using AudioProcessor::processBlockBypassed;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; } // played from the host's notes
    bool producesMidi() const override { return false; }
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

    juce::AudioProcessorValueTreeState apvts;
    juce::PropertiesFile& getSettings() { return userSettings; } // per-user preferences

    // Never touches the pitch bend range, a setting of the player.
    SoundPresets& getSoundPresets() { return soundPresets; }

private:
    void readParams();
    void readTempo();
    void allNotesOff();

    // What NoteInput plays: a note-on goes to the selected engine, everything else to all
    // three, so a note that started before an engine switch still ends.
    struct Engines
    {
        BleepForgeProcessor& p;
        void noteOn (int note, float velocity);
        void noteOff (int note);
        void allNotesOff() { p.allNotesOff(); }
        void pitchBend (float amount);
    };

    juce::PropertiesFile userSettings;

    // Saved with the plug-in state so later versions can migrate older projects.
    //   1: 0.1.0
    //   2: 0.10.0
    //   3: Delay Time split into a time and a feel (an old delayTime is converted on loading)
    //   4: Foley and Machine became one engine, Strike (their parameters are dropped on loading)
    static constexpr int kStateVersion = 4;

    // Every engine renders every block, so switching lets the old one ring out; notes go to
    // the selected one.
    bleepforge::BleepSynth bleep;
    bleepforge::StrikeSynth strike;
    bleepforge::Patch patch; // this block's sound parameters
    SoundPresets soundPresets { apvts };
    bleepforge::Effects effects;
    double hostBpm = 120.0; // for the delay's synced time, also while the host is stopped
    bleepforge::NoteInput noteInput;
    juce::LinearSmoothedValue<float> outputGain { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BleepForgeProcessor)
};

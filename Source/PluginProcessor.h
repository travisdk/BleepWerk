// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Patch.h"
#include "NoteInput.h"
#include "SoundPresets.h"
#include "PatternPresets.h"
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

    // The step sequencer: eight patterns, which of them plays, and the pattern files. The
    // sequencer plays while it is switched on and the host's transport runs.
    bleepforge::PatternBank& getPatternBank() { return patternBank; }
    PatternPresets& getPatternPresets() { return patternPresets; }
    bool isSequencerRunning() const { return sequencerRunning.load(); }
    int getSequencerStep() const { return layers[(size_t) patternBank.active()].getCurrentStep(); } // of the slot the panel shows
    void editPatternSlot (int slot) { patternBank.select (slot); }
    static juce::File patternFolder();

private:
    void readParams();
    void readTempo();
    bool readTransport (double& ppq, bool& playing); // false when the host gives no position
    void allNotesOff();
    void patternsChanged() { updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true)); }

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
    bleepforge::PatternBank patternBank;
    std::array<bleepforge::SequencerEngine, bleepforge::kNumPatternSlots> layers; // one engine for each pattern that plays
    PatternPresets patternPresets { apvts, patternBank, [] { return BleepForgeProcessor::patternFolder(); }, [this] { patternsChanged(); } };
    std::atomic<bool> sequencerRunning { false };
    std::atomic<int> lastPatternParam { 0 };
    std::array<int, 16> heldKeys {}; // keys held while the sequencer runs; the last one is the root
    int numHeldKeys = 0;
    juce::LinearSmoothedValue<float> outputGain { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BleepForgeProcessor)
};

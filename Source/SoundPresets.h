// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "FactoryPresets.h"

#include <optional>

// Sound presets: the factory presets followed by the user's .bfpreset files. A preset covers
// every parameter except the sequencer's (IDs starting with "seq"), so loading a sound never
// touches the pattern or the sequencer settings. Adapted from SubForge. Runs on the message
// thread.
class SoundPresets
{
public:
    static constexpr const char* kExtension = ".bfpreset";

    explicit SoundPresets (juce::AudioProcessorValueTreeState&);

    static bool isSoundParameter (const juce::String& id) { return ! id.startsWith ("seq"); }

    // factory entries first, then user files
    int numFactory() const { return (int) bleepforge::factoryPresets().size(); }
    int numEntries() const { return numFactory() + userFiles.size(); }
    juce::String entryName (int index) const;
    juce::String entryCategory (int index) const; // "User" for the user's files
    bool isUser (int index) const { return index >= numFactory() && index < numEntries(); }
    int current() const { return currentEntry; }
    juce::String name() const { return currentName; }

    void load (int index);

    // A sound that is no entry (the dice) shows as entry -1 under a name; restoring what came
    // before a roll sets the entry and name back.
    void setCurrent (int entry, const juce::String& newName);
    void rescan();
    bool exists (const juce::String& presetName) const;
    bool save (const juce::String& presetName);
    void remove (int index);

    static juce::File folder();

    // Counts changes to the list or the current preset, for the editor to follow.
    int version() const { return changeCount; }

    // the current name and entry, stored with the project
    void writeState (juce::ValueTree&) const;
    void readState (const juce::ValueTree&);

private:
    void loadFactory (int index);
    bool loadFile (const juce::File&);
    void changed() { ++changeCount; }

    juce::AudioProcessorValueTreeState& state;
    juce::Array<juce::File> userFiles;
    std::vector<std::optional<float>> matchedGains; // per factory preset, measured the first time it loads
    int currentEntry = 0;
    juce::String currentName { "Init" };
    int changeCount = 0;
};

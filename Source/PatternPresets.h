// Copyright (C) 2026 Henrik Juhl. Part of BleepWerk, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "LayerClip.h"
#include "PatternLibrary.h"

// Sequencer patterns: the factory patterns, the factory polymeter sets (several layers loaded
// at once into as many slots, from the shown one on, named "Set name · A", "Set name · B"...; after
// that the slots are ordinary, independent patterns), followed by the user's .bfpattern
// files. A pattern covers the steps plus the groove settings (length, swing,
// rate, gate) and nothing of the sound; the root note is left alone so a
// pattern follows the key being played. Everything works on the slot that
// plays, and each slot keeps the name of what was loaded into it and its own
// groove settings. Runs on the message thread.
class PatternPresets
{
public:
    static constexpr const char* kExtension = ".bfpattern";

    PatternPresets (juce::AudioProcessorValueTreeState&, bleepwerk::PatternBank&, std::function<juce::File()> folderProvider,
                    std::function<void()> onChange);

    // browser: factory entries first, then user files
    int numEntries() const { return bleepwerk::kNumFactoryEntries + userFiles.size(); }
    juce::String entryName (int index) const;
    bool isUser (int index) const { return index >= bleepwerk::kNumFactoryEntries && index < numEntries(); }
    bool isSet (int index) const { return index >= bleepwerk::kNumFactoryPatterns && index < bleepwerk::kNumFactoryEntries; }
    int current() const { return slot().entry; }
    juce::String name() const { return slot().name; }
    juce::String slotName (int slotIndex) const { return slots[(size_t) slotIndex].name; }

    // Loads an entry into the shown slot. A set fills as many slots as it has layers, from the
    // shown slot on (and only those layers play). Returns a message for the user if it could not
    // be loaded (a set that doesn't fit), else an empty string; nothing is touched then.
    juce::String load (int index);
    // Every note of every slot cleared, and no layer playing. The grooves (length, rate...), the
    // shown slot, Root and the sound are left alone.
    void clearAll();
    // What a new instance has: the set Five Over Seven in A and B, both playing, shown from A.
    void loadStarterSet();
    void randomize();
    void clear();
    void markEdited(); // a step was changed in the grid
    void copySlot (int from, int to);

    // A slot as a one-pass MIDI clip, "BleepWerk B - Lonely Pulse.mid" in the MIDI
    // folder (next to the pattern folder), replacing an earlier export of the same
    // name. Returns the file, or a default File if it could not be written.
    juce::File exportMidi (int slotIndex);
    // Everything the layers that play do over a number of bars, as one clip on one track, with
    // the notes the voices would play (see LayerClip.h): "BleepWerk layers 16 bars.mid", or with
    // a number added when that file exists, so nothing is replaced. The notes are from the Root
    // knob. A default File if no layer has a note to play or the file could not be written.
    juce::File exportLayersMidi (int bars);
    // after how many bars the layers repeat exactly (0: nothing to play, -1: more than 64)
    int barsUntilRepeat() const { return bleepwerk::barsUntilRepeat (bank); }
    juce::File midiFolder() const { return getFolder().getParentDirectory().getChildFile ("MIDI"); }

    void rescan();
    bool exists (const juce::String& patternName) const;
    bool save (const juce::String& patternName);
    void remove (int index);
    juce::File folder() const { return getFolder(); }

    // the steps, grooves and names of all slots, stored with the project
    void writeState (juce::ValueTree&) const;
    void readState (const juce::ValueTree&);

private:
    // groove settings read from a file or text; missing ones leave the slot's value
    struct LoadedGroove
    {
        int length = bleepwerk::kMaxSteps;
        float swing = 0.5f;
        int rateIndex = -1;  // -1: leave the current rate
        float gate = -1.0f;  // < 0: leave the current gate
        int drift = 0;
        int vary = 0;
        int shape = 0;
        int transpose = 0;
    };

    struct Slot
    {
        juce::String name { "Default" };
        int entry = -1;
    };

    Slot& slot() { return slots[(size_t) bank.active()]; }
    const Slot& slot() const { return slots[(size_t) bank.active()]; }
    bleepwerk::Pattern& pattern() { return bank.activePattern(); }
    const bleepwerk::Pattern& pattern() const { return bank.activePattern(); }
    int entryNamed (const juce::String& wanted) const;
    void applyGroove (bleepwerk::Pattern&, const LoadedGroove&);
    juce::String loadSet (int setIndex);
    static int rateIndexFor (const juce::String& rateName);
    void changed() { onChange(); }

    juce::AudioProcessorValueTreeState& state;
    bleepwerk::PatternBank& bank;
    std::function<juce::File()> getFolder;
    std::function<void()> onChange;

    juce::Array<juce::File> userFiles;
    std::array<Slot, bleepwerk::kNumPatternSlots> slots;
    int randomCount = 0;
    std::mt19937 rng { std::random_device {}() };
};

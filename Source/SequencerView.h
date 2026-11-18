// Copyright (C) 2026 Henrik Juhl. Part of BleepWerk, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "Controls.h"

// The step sequencer's panel, below the effects: the patterns A-H, the name of the one shown
// with its menu, the dice and the MIDI export; sixteen steps; and the groove (length, rate,
// swing, gate, drift, hits, vary, transpose; the first row and Transpose belong to the shown
// pattern) and, apart from them, the root note, which is common to all layers.
//   A-H:   click a letter to edit that pattern; the tick under it makes it play (as a layer, any
//          number at once, each with its own length)
//   step:  click the bar to switch it on or off, drag it up and down or scroll to change the
//          note, double-click to reset the note to the root; "ACC" above it is the accent; the
//          arrows below move the note an octave up or down
//   CLEAR: under the MIDI button; asks whether to clear every note of all eight patterns (and
//          switch every layer off) or only those of the shown one
//   MIDI:  click for a menu: the shown pattern as a MIDI file (one pass), or everything the
//          layers play over 4 to 64 bars; on Windows drag the button into the host to take the
//          last choice as a clip
class SequencerView : public juce::Component, private juce::Timer
{
public:
    static constexpr int kHeight = 232; // 24 + 22 + the grid, and two rows of knobs on the right

    explicit SequencerView (BleepWerkProcessor&);
    ~SequencerView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Set by the main view, which owns the dialogs.
    std::function<void()> onSave;                    // the playing pattern, to be saved as a file
    std::function<void (int entry)> onDelete;        // a user pattern, to be moved to the trash
    std::function<void()> onClearAll;                // asks all or the shown pattern

private:
    class StepGrid;
    struct GrooveKnob;
    struct MidiButton;
    struct PlayDot;

    void timerCallback() override;
    void showPatternMenu();
    void loadEntry (int entry);
    void showExternalRoot (int note);
    void showMidiMenu();
    void exportMidi (int bars);
    void showExported (const juce::File&, bool layers);
    juce::File writeMidi() const;
    void refresh();
    juce::Rectangle<int> rootBounds() const;

    BleepWerkProcessor& processor;
    juce::OwnedArray<juce::TextButton> slotButtons;
    juce::OwnedArray<PlayDot> playDots;
    juce::TextButton patternButton;
    juce::TextButton clearButton { "CLEAR" }; // under the MIDI button: every note of every pattern
    IconButton dice { IconButton::Icon::Dice, "A random pattern in the playing slot" };
    std::unique_ptr<MidiButton> midiButton;
    std::unique_ptr<StepGrid> grid;
    std::unique_ptr<GrooveKnob> length, rate, swing, gate, drift, hits, vary, transpose;
    juce::TextButton shapeButton; // how Vary moves the length: Walk, Random or Skip
    Knob root;

    int externalRoot = -1; // the key from the host that plays the pattern from its note, or -1
    int shownVersion = -1;
    int exportBars = 0; // what the MIDI button writes: 0 the shown pattern for one pass, else all layers for this many bars

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SequencerView)
};

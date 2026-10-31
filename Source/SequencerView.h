// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "Controls.h"

// The step sequencer's panel, below the effects: the patterns A-H, the name of the one shown
// with its menu, the dice and the MIDI export; sixteen steps; and the groove (length, rate,
// swing, gate, drift, hits) and the root note.
//   A-H:   click a letter to edit that pattern; the dot under it makes it play (as a layer, any
//          number at once, each with its own length)
//   step:  click the bar to switch it on or off, drag it up and down or scroll to change the
//          note, double-click to reset the note to the root; the dot above is an accent; the
//          arrows below move the note an octave up or down
//   MIDI:  click to write the shown pattern as a MIDI file; on Windows drag it into the host
class SequencerView : public juce::Component, private juce::Timer
{
public:
    static constexpr int kHeight = 188;

    explicit SequencerView (BleepForgeProcessor&);
    ~SequencerView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Set by the main view, which owns the dialogs.
    std::function<void()> onSave;                    // the playing pattern, to be saved as a file
    std::function<void (int entry)> onDelete;        // a user pattern, to be moved to the trash

private:
    class StepGrid;
    struct GrooveKnob;
    struct MidiButton;
    struct PlayDot;

    void timerCallback() override;
    void showPatternMenu();
    void exportMidi();
    void refresh();

    BleepForgeProcessor& processor;
    juce::OwnedArray<juce::TextButton> slotButtons;
    juce::OwnedArray<PlayDot> playDots;
    juce::TextButton patternButton;
    IconButton dice { IconButton::Icon::Dice, "A random pattern in the playing slot" };
    std::unique_ptr<MidiButton> midiButton;
    std::unique_ptr<StepGrid> grid;
    std::unique_ptr<GrooveKnob> length, rate, swing, gate, drift, hits;
    Knob root;

    int shownVersion = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SequencerView)
};

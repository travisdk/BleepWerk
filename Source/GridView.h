// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

// The probability grid: rows are every MIDI note in the scale and key (low at the bottom),
// columns the steps. The view shows the visible range (1-4 octaves) at a time, browsed with
// the mouse wheel or the overview strip on the right, which shows all of MIDI, the rows that
// have notes and the part in view.
// The edit mode chooses what a cell shows and what dragging it changes: its probability
// (mint) or its velocity (amber), as the cell's fill height. Cells flash when they actually
// play, and the playing step is highlighted.
//   click: on/off   drag up/down: the mode's value   double-click: reset the mode's value
class GridView : public juce::Component, private juce::Timer
{
public:
    explicit GridView (BleepForgeProcessor&);
    ~GridView() override;

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    void editModeChanged() { repaint(); }

    // A description of the cell under the mouse, or empty when there is none.
    std::function<void (const juce::String&)> onStatus;

    static constexpr float kLabelWidth = 44.0f, kFooterHeight = 18.0f, kOverviewWidth = 12.0f, kOverviewGap = 8.0f;

private:
    struct CellPos
    {
        int row = -1, step = -1;
        bool valid() const { return row >= 0 && step >= 0; }
        bool operator== (const CellPos& o) const { return row == o.row && step == o.step; }
    };

    void timerCallback() override;
    bleepforge::SequencerSettings settings() const { return processor.sequencerSettings(); }
    int rows() const;
    int steps() const;
    int visibleRows() const;
    int firstRow() const; // the bottom row in view
    int slotOf (int row) const;
    void scrollTo (int bottomRow);
    void scrollToOverview (float y);
    int lowestUsedRow() const; // -1 when the grid is empty
    juce::Rectangle<float> cellArea() const;
    juce::Rectangle<float> overviewArea() const;
    juce::Rectangle<float> cellBounds (int row, int step) const;
    CellPos cellAt (juce::Point<float>) const;
    juce::String describe (CellPos) const;
    void setHover (CellPos);

    BleepForgeProcessor& processor;
    bleepforge::Grid& grid;

    CellPos hover, drag;
    bool dragged = false, browsing = false;
    float dragStartValue = 0.0f, wheelAccumulator = 0.0f;

    // what was drawn, so the timer repaints only when something changed
    int shownStep = -1, shownScale = -1, shownRoot = -1, shownLength = -1, shownRange = -1;
    std::array<uint32_t, bleepforge::kNumSlots> shownHits {};
    std::array<float, (size_t) (bleepforge::kNumSlots * bleepforge::kMaxSteps)> flash {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GridView)
};

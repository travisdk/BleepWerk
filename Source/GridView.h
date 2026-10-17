// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

// The probability grid: rows are the scale's notes (root at the bottom), columns the steps.
// A cell's fill height is its probability and its brightness its velocity; cells flash when
// they actually play, and the playing step is highlighted.
//   click: on/off   drag up/down: probability   shift+drag: velocity   double-click: reset
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

    // A description of the cell under the mouse, or empty when there is none.
    std::function<void (const juce::String&)> onStatus;

    static constexpr float kLabelWidth = 44.0f, kFooterHeight = 18.0f;

private:
    struct CellPos
    {
        int row = -1, step = -1;
        bool valid() const { return row >= 0 && step >= 0; }
        bool operator== (const CellPos& o) const { return row == o.row && step == o.step; }
    };

    void timerCallback() override;
    int rows() const;
    int steps() const;
    juce::Rectangle<float> cellArea() const;
    juce::Rectangle<float> cellBounds (int row, int step) const;
    CellPos cellAt (juce::Point<float>) const;
    juce::String describe (CellPos) const;
    void setHover (CellPos);

    BleepForgeProcessor& processor;
    bleepforge::Grid& grid;

    CellPos hover, drag;
    bool dragged = false, dragVelocity = false;
    float dragStartValue = 0.0f;

    // what was drawn, so the timer repaints only when something changed
    int shownStep = -1, shownScale = -1, shownRoot = -1, shownLength = -1;
    std::array<uint32_t, bleepforge::kMaxRows> shownHits {};
    std::array<float, (size_t) (bleepforge::kMaxRows * bleepforge::kMaxSteps)> flash {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GridView)
};

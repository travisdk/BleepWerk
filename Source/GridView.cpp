// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#include "GridView.h"
#include "Theme.h"

using namespace bleepforge;

namespace
{
constexpr float kDragPixelsForFullRange = 150.0f;
constexpr float kDragThreshold = 3.0f;
const Cell kDefaultCell;

juce::String noteName (int note)
{
    return juce::String (kRootNames[(size_t) (((note % 12) + 12) % 12)]) + juce::String (note / 12 - 1);
}
} // namespace

GridView::GridView (BleepForgeProcessor& p)
    : processor (p), grid (p.getGrid())
{
    for (int row = 0; row < kMaxRows; ++row)
        shownHits[(size_t) row] = processor.getSequencer().getLastHit (row);
    startTimerHz (30);
}

GridView::~GridView()
{
    stopTimer();
}

int GridView::rows() const { return numRows (processor.sequencerSettings().scale); }
int GridView::steps() const { return grid.getLength(); }

juce::Rectangle<float> GridView::cellArea() const
{
    return getLocalBounds().toFloat().withTrimmedLeft (kLabelWidth).withTrimmedBottom (kFooterHeight);
}

// Row 0 (the root) at the bottom.
juce::Rectangle<float> GridView::cellBounds (int row, int step) const
{
    const auto area = cellArea();
    const float w = area.getWidth() / (float) steps(), h = area.getHeight() / (float) rows();
    return { area.getX() + (float) step * w, area.getY() + (float) (rows() - 1 - row) * h, w, h };
}

GridView::CellPos GridView::cellAt (juce::Point<float> pos) const
{
    const auto area = cellArea();
    if (! area.contains (pos))
        return {};
    const int step = juce::jlimit (0, steps() - 1, (int) ((pos.x - area.getX()) / area.getWidth() * (float) steps()));
    const int fromTop = juce::jlimit (0, rows() - 1, (int) ((pos.y - area.getY()) / area.getHeight() * (float) rows()));
    return { rows() - 1 - fromTop, step };
}

juce::String GridView::describe (CellPos pos) const
{
    if (! pos.valid())
        return {};
    const auto settings = processor.sequencerSettings();
    const auto cell = grid.get (pos.row, pos.step);
    auto text = noteName (rowNote (pos.row, settings.scale, settings.rootNote)) + "  step " + juce::String (pos.step + 1);
    if (! cell.on)
        return text + "  off";
    return text + "  probability " + juce::String (juce::roundToInt (cell.probability * 100.0f)) + " %"
         + "  velocity " + juce::String (juce::roundToInt (cell.velocity * 127.0f));
}

void GridView::setHover (CellPos pos)
{
    if (pos == hover)
        return;
    hover = pos;
    repaint();
    if (onStatus)
        onStatus (describe (pos));
}

void GridView::paint (juce::Graphics& g)
{
    const auto settings = processor.sequencerSettings();
    const int numRowsShown = rows(), numSteps = steps();
    const int playing = processor.getSequencer().getCurrentStep();
    const auto area = cellArea();

    // row labels: the note each row plays, roots in the accent colour
    for (int row = 0; row < numRowsShown; ++row)
    {
        const int note = rowNote (row, settings.scale, settings.rootNote);
        const auto r = cellBounds (row, 0).withX (0.0f).withWidth (kLabelWidth - 6.0f);
        const bool isRoot = row % scaleAt (settings.scale).size == 0;
        g.setColour (isRoot ? theme::accent : (hover.row == row ? theme::text : theme::dimText));
        g.setFont (juce::FontOptions (12.0f, isRoot ? juce::Font::bold : juce::Font::plain));
        g.drawText (noteName (note), r, juce::Justification::centredRight);
    }

    // the playing step's column
    if (playing >= 0 && playing < numSteps)
    {
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.fillRect (cellBounds (0, playing).withY (area.getY()).withHeight (area.getHeight()));
    }

    for (int step = 0; step < numSteps; ++step)
        for (int row = 0; row < numRowsShown; ++row)
        {
            const auto cell = grid.get (row, step);
            const auto r = cellBounds (row, step).reduced (2.0f);
            const float corner = 3.0f;

            g.setColour ((step / 4) % 2 == 1 ? theme::panelAlt : theme::panel);
            g.fillRoundedRectangle (r, corner);

            if (cell.on)
            {
                // fill height: probability; brightness: velocity
                const auto fill = r.withTop (r.getBottom() - r.getHeight() * cell.probability);
                g.setColour (theme::accent.withAlpha (0.25f + 0.75f * cell.velocity));
                g.fillRoundedRectangle (fill, corner);
                g.setColour (theme::accent);
                g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.2f);

                if (cell.probability < 1.0f && r.getWidth() >= 24.0f && r.getHeight() >= 14.0f)
                {
                    g.setColour (theme::text.withAlpha (0.85f));
                    g.setFont (juce::FontOptions (10.0f));
                    g.drawText (juce::String (juce::roundToInt (cell.probability * 100.0f)), r.reduced (2.0f, 1.0f),
                                juce::Justification::topRight);
                }
            }
            else if (hover.row == row && hover.step == step)
            {
                g.setColour (theme::accent.withAlpha (0.35f));
                g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
            }

            if (const float f = flash[(size_t) (row * kMaxSteps + step)]; f > 0.01f)
            {
                g.setColour (juce::Colours::white.withAlpha (0.55f * f));
                g.fillRoundedRectangle (r, corner);
            }
        }

    // footer: beat numbers and the playhead
    const auto footer = getLocalBounds().toFloat().removeFromBottom (kFooterHeight).withTrimmedLeft (kLabelWidth);
    const float w = footer.getWidth() / (float) numSteps;
    g.setFont (juce::FontOptions (11.0f));
    for (int step = 0; step < numSteps; ++step)
    {
        const auto r = juce::Rectangle<float> (footer.getX() + (float) step * w, footer.getY(), w, footer.getHeight());
        if (step == playing)
        {
            g.setColour (theme::accent);
            g.fillRoundedRectangle (r.reduced (3.0f, 7.0f), 1.5f);
        }
        else if (step % 4 == 0)
        {
            g.setColour (theme::dimText);
            g.drawText (juce::String (step + 1), r, juce::Justification::centred);
        }
    }
}

void GridView::timerCallback()
{
    const auto& sequencer = processor.getSequencer();
    bool changed = false;

    for (auto& f : flash)
        if (f > 0.0f)
        {
            f = f < 0.02f ? 0.0f : f * 0.8f;
            changed = true;
        }

    for (int row = 0; row < kMaxRows; ++row)
    {
        const auto hit = sequencer.getLastHit (row);
        if (hit != shownHits[(size_t) row])
        {
            shownHits[(size_t) row] = hit;
            flash[(size_t) (row * kMaxSteps + (int) (hit & 63u) % kMaxSteps)] = 1.0f;
            changed = true;
        }
    }

    const auto settings = processor.sequencerSettings();
    const int step = sequencer.getCurrentStep();
    if (step != shownStep || settings.scale != shownScale || settings.rootNote != shownRoot || steps() != shownLength)
    {
        shownStep = step;
        shownScale = settings.scale;
        shownRoot = settings.rootNote;
        shownLength = steps();
        changed = true;
    }

    if (changed)
        repaint();
}

void GridView::mouseMove (const juce::MouseEvent& e)
{
    setHover (cellAt (e.position));
}

void GridView::mouseExit (const juce::MouseEvent&)
{
    setHover ({});
}

void GridView::mouseDown (const juce::MouseEvent& e)
{
    drag = cellAt (e.position);
    dragged = false;
    if (! drag.valid())
        return;
    dragVelocity = e.mods.isShiftDown();
    const auto cell = grid.get (drag.row, drag.step);
    dragStartValue = dragVelocity ? cell.velocity : cell.probability;
}

void GridView::mouseDrag (const juce::MouseEvent& e)
{
    if (! drag.valid())
        return;
    const float dy = (float) e.getDistanceFromDragStartY();
    if (! dragged && std::abs (dy) < kDragThreshold)
        return;
    dragged = true;

    // dragging a cell sets it, and switches it on
    auto cell = grid.get (drag.row, drag.step);
    cell.on = true;
    const float value = juce::jlimit (0.0f, 1.0f, dragStartValue - dy / kDragPixelsForFullRange);
    if (dragVelocity)
        cell.velocity = juce::jmax (1.0f / 127.0f, value);
    else
        cell.probability = value;
    grid.set (drag.row, drag.step, cell);

    hover = drag;
    repaint();
    if (onStatus)
        onStatus (describe (drag));
}

void GridView::mouseUp (const juce::MouseEvent&)
{
    if (! drag.valid())
        return;
    if (! dragged)
    {
        auto cell = grid.get (drag.row, drag.step);
        cell.on = ! cell.on;
        grid.set (drag.row, drag.step, cell);
    }
    processor.gridEdited();
    repaint();
    if (onStatus)
        onStatus (describe (drag));
    drag = {};
}

void GridView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto pos = cellAt (e.position);
    if (! pos.valid())
        return;
    auto cell = kDefaultCell;
    cell.on = true;
    grid.set (pos.row, pos.step, cell);
    processor.gridEdited();
    repaint();
    if (onStatus)
        onStatus (describe (pos));
}

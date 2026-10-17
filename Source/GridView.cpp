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

using EditMode = BleepForgeProcessor::EditMode;

float valueOf (const Cell& cell, EditMode mode)
{
    return mode == EditMode::Velocity ? cell.velocity : cell.probability;
}

void setValue (Cell& cell, EditMode mode, float value)
{
    if (mode == EditMode::Velocity)
        cell.velocity = juce::jlimit (1.0f / 127.0f, 1.0f, value);
    else
        cell.probability = juce::jlimit (0.0f, 1.0f, value);
}

juce::String valueText (const Cell& cell, EditMode mode)
{
    return mode == EditMode::Velocity ? juce::String (juce::roundToInt (cell.velocity * 127.0f))
                                      : juce::String (juce::roundToInt (cell.probability * 100.0f));
}

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

int GridView::rows() const
{
    const auto settings = processor.sequencerSettings();
    return numRows (settings.scale, settings.range);
}

int GridView::steps() const { return grid.getLength(); }
int GridView::visibleRows() const { return juce::jmin (rows(), kMaxVisibleRows); }
int GridView::firstRow() const { return juce::jlimit (0, rows() - visibleRows(), processor.firstVisibleRow.load()); }

void GridView::scrollTo (int bottomRow)
{
    const int clamped = juce::jlimit (0, rows() - visibleRows(), bottomRow);
    if (clamped == processor.firstVisibleRow.load())
        return;
    processor.firstVisibleRow = clamped;
    hover = {};
    repaint();
}

// Centres the view on the row at height y of the overview strip.
void GridView::scrollToOverview (float y)
{
    const auto strip = overviewArea();
    const float fromBottom = (strip.getBottom() - y) / strip.getHeight() * (float) rows();
    scrollTo (juce::roundToInt (fromBottom - (float) visibleRows() * 0.5f));
}

juce::Rectangle<float> GridView::cellArea() const
{
    return getLocalBounds().toFloat().withTrimmedLeft (kLabelWidth).withTrimmedBottom (kFooterHeight)
        .withTrimmedRight (kOverviewWidth + kOverviewGap);
}

juce::Rectangle<float> GridView::overviewArea() const
{
    const auto area = cellArea();
    return { area.getRight() + kOverviewGap, area.getY() + 2.0f, kOverviewWidth, area.getHeight() - 4.0f };
}

// The bottom row in view at the bottom.
juce::Rectangle<float> GridView::cellBounds (int row, int step) const
{
    const auto area = cellArea();
    const int shown = visibleRows();
    const float w = area.getWidth() / (float) steps(), h = area.getHeight() / (float) shown;
    return { area.getX() + (float) step * w, area.getY() + (float) (firstRow() + shown - 1 - row) * h, w, h };
}

GridView::CellPos GridView::cellAt (juce::Point<float> pos) const
{
    const auto area = cellArea();
    if (! area.contains (pos))
        return {};
    const int shown = visibleRows();
    const int step = juce::jlimit (0, steps() - 1, (int) ((pos.x - area.getX()) / area.getWidth() * (float) steps()));
    const int fromTop = juce::jlimit (0, shown - 1, (int) ((pos.y - area.getY()) / area.getHeight() * (float) shown));
    return { firstRow() + shown - 1 - fromTop, step };
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
    const int numSteps = steps(), first = firstRow(), last = first + visibleRows() - 1;
    const int playing = processor.getSequencer().getCurrentStep();
    const auto area = cellArea();
    const auto mode = processor.editMode.load();
    const auto colour = mode == EditMode::Velocity ? theme::velocity : theme::accent;

    // row labels: the note each row plays, roots in the accent colour
    for (int row = first; row <= last; ++row)
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
        for (int row = first; row <= last; ++row)
        {
            const auto cell = grid.get (row, step);
            const auto r = cellBounds (row, step).reduced (2.0f);
            const float corner = 3.0f;

            g.setColour ((step / 4) % 2 == 1 ? theme::panelAlt : theme::panel);
            g.fillRoundedRectangle (r, corner);

            if (cell.on)
            {
                // the mode's value as the fill height, its number while below the maximum
                const float value = valueOf (cell, mode);
                g.setColour (colour.withAlpha (0.8f));
                g.fillRoundedRectangle (r.withTop (r.getBottom() - r.getHeight() * value), corner);
                g.setColour (colour);
                g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.2f);

                if (value < 1.0f && r.getWidth() >= 24.0f && r.getHeight() >= 14.0f)
                {
                    g.setColour (theme::text.withAlpha (0.85f));
                    g.setFont (juce::FontOptions (10.0f));
                    g.drawText (valueText (cell, mode), r.reduced (2.0f, 1.0f), juce::Justification::topRight);
                }
            }
            else if (hover.row == row && hover.step == step)
            {
                g.setColour (colour.withAlpha (0.35f));
                g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
            }

            if (const float f = flash[(size_t) (row * kMaxSteps + step)]; f > 0.01f)
            {
                g.setColour (juce::Colours::white.withAlpha (0.55f * f));
                g.fillRoundedRectangle (r, corner);
            }
        }

    // overview of the whole range: a tick for each row with notes, roots as faint lines, and
    // the part in view; only when the range has more rows than fit
    if (const int total = rows(); total > visibleRows())
    {
        const auto strip = overviewArea();
        const float rowH = strip.getHeight() / (float) total;
        auto rowRect = [&] (int row) { return juce::Rectangle<float> (strip.getX(), strip.getBottom() - (float) (row + 1) * rowH, strip.getWidth(), rowH); };

        g.setColour (theme::panel);
        g.fillRoundedRectangle (strip, 3.0f);
        for (int row = 0; row < total; ++row)
        {
            if (row % scaleAt (settings.scale).size == 0)
            {
                g.setColour (theme::outline);
                g.fillRect (rowRect (row).withSizeKeepingCentre (strip.getWidth(), 1.0f));
            }
            bool used = false;
            for (int step = 0; step < numSteps && ! used; ++step)
                used = grid.get (row, step).on;
            if (used)
            {
                g.setColour (colour);
                g.fillRect (rowRect (row).reduced (2.0f, 0.0f).withSizeKeepingCentre (strip.getWidth() - 4.0f, juce::jmax (1.5f, rowH - 1.0f)));
            }
        }
        const auto window = rowRect (last).withBottom (rowRect (first).getBottom());
        g.setColour (theme::text.withAlpha (0.12f));
        g.fillRoundedRectangle (window, 3.0f);
        g.setColour (theme::text.withAlpha (0.6f));
        g.drawRoundedRectangle (window.reduced (0.5f), 3.0f, 1.0f);
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
    if (step != shownStep || settings.scale != shownScale || settings.rootNote != shownRoot || steps() != shownLength
        || settings.range != shownRange)
    {
        shownRange = settings.range;
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

// Mouse wheels give about 0.12 per notch, trackpads many small steps; a row per 0.1.
void GridView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (rows() <= visibleRows())
        return;
    wheelAccumulator += wheel.isReversed ? -wheel.deltaY : wheel.deltaY;
    int rowsToMove = 0;
    for (; wheelAccumulator >= 0.1f; wheelAccumulator -= 0.1f)
        ++rowsToMove;
    for (; wheelAccumulator <= -0.1f; wheelAccumulator += 0.1f)
        --rowsToMove;
    if (rowsToMove != 0)
    {
        scrollTo (firstRow() + rowsToMove);
        setHover (cellAt (e.position));
    }
}

void GridView::mouseExit (const juce::MouseEvent&)
{
    setHover ({});
}

void GridView::mouseDown (const juce::MouseEvent& e)
{
    browsing = rows() > visibleRows() && overviewArea().expanded (4.0f, 0.0f).contains (e.position);
    if (browsing)
    {
        scrollToOverview (e.position.y);
        return;
    }
    drag = cellAt (e.position);
    dragged = false;
    if (! drag.valid())
        return;
    dragStartValue = valueOf (grid.get (drag.row, drag.step), processor.editMode.load());
}

void GridView::mouseDrag (const juce::MouseEvent& e)
{
    if (browsing)
    {
        scrollToOverview (e.position.y);
        return;
    }
    if (! drag.valid())
        return;
    const float dy = (float) e.getDistanceFromDragStartY();
    if (! dragged && std::abs (dy) < kDragThreshold)
        return;
    dragged = true;

    // dragging a cell sets it, and switches it on
    auto cell = grid.get (drag.row, drag.step);
    cell.on = true;
    setValue (cell, processor.editMode.load(), dragStartValue - dy / kDragPixelsForFullRange);
    grid.set (drag.row, drag.step, cell);

    hover = drag;
    repaint();
    if (onStatus)
        onStatus (describe (drag));
}

void GridView::mouseUp (const juce::MouseEvent&)
{
    browsing = false;
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
    const auto mode = processor.editMode.load();
    auto cell = grid.get (pos.row, pos.step);
    cell.on = true;
    setValue (cell, mode, valueOf (kDefaultCell, mode));
    grid.set (pos.row, pos.step, cell);
    processor.gridEdited();
    repaint();
    if (onStatus)
        onStatus (describe (pos));
}

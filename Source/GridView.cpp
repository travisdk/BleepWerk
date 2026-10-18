// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#include "GridView.h"
#include "Theme.h"

using namespace bleepforge;

namespace
{
constexpr float kDragPixelsForFullRange = 150.0f;
constexpr float kDragPixelsPerRatchet = 24.0f;
constexpr float kDragThreshold = 3.0f;
constexpr int kEmptyGridBottomNote = 48; // C3: where an empty grid opens
const Cell kDefaultCell;

using EditMode = BleepForgeProcessor::EditMode;

// The value the mode edits: 0..1 for probability and velocity, the number of hits for Ratchet.
float valueOf (const Cell& cell, EditMode mode)
{
    switch (mode)
    {
        case EditMode::Velocity: return cell.velocity;
        case EditMode::Ratchet: return (float) cell.ratchet;
        case EditMode::Probability: break;
    }
    return cell.probability;
}

void setValue (Cell& cell, EditMode mode, float value)
{
    switch (mode)
    {
        case EditMode::Velocity: cell.velocity = juce::jlimit (1.0f / 127.0f, 1.0f, value); break;
        case EditMode::Ratchet: cell.ratchet = juce::jlimit (1, 4, juce::roundToInt (value)); break;
        case EditMode::Probability: cell.probability = juce::jlimit (0.0f, 1.0f, value); break;
    }
}

juce::Colour modeColour (EditMode mode)
{
    switch (mode)
    {
        case EditMode::Velocity: return theme::secondary();
        case EditMode::Ratchet: return theme::secondary();
        case EditMode::Probability: break;
    }
    return theme::primary();
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
    for (int slot = 0; slot < kNumSlots; ++slot)
        shownHits[(size_t) slot] = processor.getSequencer().getLastHit (slot);

    // the first time the grid is shown, it opens on the pattern's lowest note
    if (processor.bottomNote.load() < 0)
    {
        const int lowest = lowestUsedRow();
        processor.bottomNote = lowest >= 0 ? rowNote (lowest, settings().scale, settings().root) : kEmptyGridBottomNote;
    }
    startTimerHz (30);
}

GridView::~GridView()
{
    stopTimer();
}

int GridView::rows() const { return numRows (settings().scale, settings().root); }
int GridView::steps() const { return grid.getLength(); }
int GridView::slotOf (int row) const { return rowSlot (row, settings().scale); }

// The visible range in octaves, from the root up to the root again: 15 rows for two octaves
// of a 7-note scale.
int GridView::visibleRows() const
{
    const int octaves = (int) processor.apvts.getRawParameterValue ("seqRange")->load();
    return juce::jmin (rows(), scaleAt (settings().scale).size * octaves + 1);
}

// The view is kept as a note rather than a row, so it stays in the same register when the
// scale or key changes.
int GridView::firstRow() const
{
    const auto s = settings();
    return juce::jlimit (0, rows() - visibleRows(), rowAtOrAbove (processor.bottomNote.load(), s.scale, s.root));
}

void GridView::scrollTo (int bottomRow)
{
    const auto s = settings();
    const int clamped = juce::jlimit (0, rows() - visibleRows(), bottomRow);
    const int note = rowNote (clamped, s.scale, s.root);
    if (note == processor.bottomNote.load())
        return;
    processor.bottomNote = note;
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

int GridView::lowestUsedRow() const
{
    const int total = rows(), numSteps = steps();
    for (int row = 0; row < total; ++row)
        for (int step = 0; step < numSteps; ++step)
            if (grid.get (slotOf (row), step).on)
                return row;
    return -1;
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
    const auto s = settings();
    const auto cell = grid.get (slotOf (pos.row), pos.step);
    auto text = noteName (rowNote (pos.row, s.scale, s.root)) + "  step " + juce::String (pos.step + 1);
    if (! cell.on)
        return text + "  off";
    return text + "  probability " + juce::String (juce::roundToInt (cell.probability * 100.0f)) + " %"
         + "  velocity " + juce::String (juce::roundToInt (cell.velocity * 127.0f))
         + (cell.ratchet > 1 ? "  ratchet " + juce::String (cell.ratchet) : juce::String());
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
    const auto s = settings();
    const int scaleSize = scaleAt (s.scale).size;
    const int numSteps = steps(), first = firstRow(), last = first + visibleRows() - 1;
    const int playing = processor.getSequencer().getCurrentStep();
    const auto area = cellArea();
    const auto mode = processor.editMode.load();
    const auto colour = modeColour (mode);
    const float rowHeight = cellBounds (first, 0).getHeight();

    // row labels: the note each row plays, roots in the accent colour; on rows too low for
    // text, only the roots
    const float labelFont = juce::jmin (12.0f, rowHeight * 0.85f);
    for (int row = first; row <= last; ++row)
    {
        const bool isRoot = row % scaleSize == 0;
        if (rowHeight < 9.0f && ! isRoot)
            continue;
        const auto r = cellBounds (row, 0).withX (0.0f).withWidth (kLabelWidth - 6.0f);
        g.setColour (isRoot ? theme::primary() : (hover.row == row ? theme::text : theme::dimText));
        g.setFont (juce::FontOptions (juce::jmax (8.0f, labelFont), isRoot ? juce::Font::bold : juce::Font::plain));
        g.drawText (noteName (rowNote (row, s.scale, s.root)), r.withSizeKeepingCentre (r.getWidth(), juce::jmax (r.getHeight(), 10.0f)),
                    juce::Justification::centredRight);
    }

    // the playing step's column
    if (playing >= 0 && playing < numSteps)
    {
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.fillRect (cellBounds (first, playing).withY (area.getY()).withHeight (area.getHeight()));
    }

    const float gap = rowHeight < 12.0f ? 1.0f : 2.0f;
    for (int step = 0; step < numSteps; ++step)
        for (int row = first; row <= last; ++row)
        {
            const int slot = slotOf (row);
            const auto cell = grid.get (slot, step);
            const auto r = cellBounds (row, step).reduced (2.0f, gap);

            g.setColour ((step / 4) % 2 == 1 ? theme::panelAlt : theme::panel);
            g.fillRect (r);

            if (cell.on && mode == EditMode::Ratchet)
            {
                // one bar per hit
                constexpr float barGap = 3.0f;
                const float barWidth = (r.getWidth() - barGap * (float) (cell.ratchet - 1)) / (float) cell.ratchet;
                g.setColour (colour.withAlpha (0.8f));
                for (int i = 0; i < cell.ratchet; ++i)
                    g.fillRect (r.withX (r.getX() + (float) i * (barWidth + barGap)).withWidth (barWidth));
                g.setColour (colour);
                g.drawRect (r, 1.2f);
            }
            else if (cell.on)
            {
                // the mode's value as the fill height, its number while below the maximum
                const float value = valueOf (cell, mode);
                g.setColour (colour.withAlpha (0.8f));
                g.fillRect (r.withTop (r.getBottom() - r.getHeight() * value));
                g.setColour (colour);
                g.drawRect (r, 1.2f);

                if (value < 1.0f && r.getWidth() >= 24.0f && r.getHeight() >= 14.0f)
                {
                    g.setColour (theme::text.withAlpha (0.85f));
                    g.setFont (juce::FontOptions (10.0f));
                    g.drawText (valueText (cell, mode), r.reduced (2.0f, 1.0f), juce::Justification::topRight);
                }

                // a ratchet as one dot per hit in the top left corner, ringed so it shows on the fill
                if (cell.ratchet > 1 && r.getHeight() >= 10.0f)
                    for (int i = 0; i < cell.ratchet; ++i)
                    {
                        const auto dot = juce::Rectangle<float> (r.getX() + 3.0f + (float) i * 7.0f, r.getY() + 3.0f, 6.0f, 6.0f);
                        g.setColour (theme::background);
                        g.fillEllipse (dot);
                        g.setColour (theme::secondary());
                        g.fillEllipse (dot.reduced (1.2f));
                    }
            }
            else if (hover.row == row && hover.step == step)
            {
                g.setColour (colour.withAlpha (0.35f));
                g.drawRect (r, 1.0f);
            }

            if (const float f = flash[(size_t) (slot * kMaxSteps + step)]; f > 0.01f)
            {
                g.setColour (juce::Colours::white.withAlpha (0.55f * f));
                g.fillRect (r);
            }
        }

    // overview of all rows: a tick for each row with notes, roots as faint lines, and the
    // part in view
    {
        const int total = rows();
        const auto strip = overviewArea();
        const float rowH = strip.getHeight() / (float) total;
        auto rowRect = [&] (int row) { return juce::Rectangle<float> (strip.getX(), strip.getBottom() - (float) (row + 1) * rowH, strip.getWidth(), rowH); };

        g.setColour (theme::panel);
        g.fillRect (strip);
        for (int row = 0; row < total; ++row)
        {
            if (row % scaleSize == 0)
            {
                g.setColour (theme::outline);
                g.fillRect (rowRect (row).withSizeKeepingCentre (strip.getWidth(), 1.0f));
            }
            bool used = false;
            for (int step = 0; step < numSteps && ! used; ++step)
                used = grid.get (slotOf (row), step).on;
            if (used)
            {
                g.setColour (colour);
                g.fillRect (rowRect (row).withSizeKeepingCentre (strip.getWidth() - 4.0f, juce::jmax (2.0f, rowH)));
            }
        }
        const auto window = rowRect (last).withBottom (rowRect (first).getBottom());
        g.setColour (theme::text.withAlpha (0.12f));
        g.fillRect (window);
        g.setColour (theme::text.withAlpha (0.6f));
        g.drawRect (window, 1.0f);
    }

    // footer: beat numbers and the playhead
    const auto footer = getLocalBounds().toFloat().removeFromBottom (kFooterHeight).withTrimmedLeft (kLabelWidth)
                            .withWidth (area.getWidth());
    const float w = footer.getWidth() / (float) numSteps;
    g.setFont (juce::FontOptions (11.0f));
    for (int step = 0; step < numSteps; ++step)
    {
        const auto r = juce::Rectangle<float> (footer.getX() + (float) step * w, footer.getY(), w, footer.getHeight());
        if (step == playing)
        {
            g.setColour (theme::primary());
            g.fillRect (r.reduced (3.0f, 7.0f));
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

    for (int slot = 0; slot < kNumSlots; ++slot)
    {
        const auto hit = sequencer.getLastHit (slot);
        if (hit != shownHits[(size_t) slot])
        {
            shownHits[(size_t) slot] = hit;
            flash[(size_t) (slot * kMaxSteps + (int) (hit & 63u) % kMaxSteps)] = 1.0f;
            changed = true;
        }
    }

    const auto s = settings();
    const int step = sequencer.getCurrentStep();
    const int range = (int) processor.apvts.getRawParameterValue ("seqRange")->load();
    if (step != shownStep || s.scale != shownScale || s.root != shownRoot || steps() != shownLength || range != shownRange)
    {
        shownStep = step;
        shownScale = s.scale;
        shownRoot = s.root;
        shownLength = steps();
        shownRange = range;
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

// Mouse wheels give about 0.12 per notch, trackpads many small steps; a row per 0.1.
void GridView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
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

void GridView::mouseDown (const juce::MouseEvent& e)
{
    browsing = overviewArea().expanded (4.0f, 0.0f).contains (e.position);
    if (browsing)
    {
        scrollToOverview (e.position.y);
        return;
    }
    drag = cellAt (e.position);
    dragged = false;
    if (! drag.valid())
        return;
    const auto cell = grid.get (slotOf (drag.row), drag.step);
    dragStartValue = valueOf (cell, processor.editMode.load());
    dragStartedOn = cell.on;
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
    const int slot = slotOf (drag.row);
    auto cell = grid.get (slot, drag.step);
    cell.on = true;
    const auto mode = processor.editMode.load();
    setValue (cell, mode, dragStartValue - dy / (mode == EditMode::Ratchet ? kDragPixelsPerRatchet : kDragPixelsForFullRange));
    grid.set (slot, drag.step, cell);

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
        const int slot = slotOf (drag.row);
        auto cell = grid.get (slot, drag.step);
        cell.on = ! cell.on;
        grid.set (slot, drag.step, cell);
        if (cell.on)
            preview (drag);
    }
    // Probability and ratchet don't change how a note sounds, so dragging them stays quiet
    // unless it switched the cell on. Velocity plays once, on release, not on every move.
    else if (! dragStartedOn || processor.editMode.load() == EditMode::Velocity)
    {
        preview (drag);
    }
    processor.gridEdited();
    repaint();
    if (onStatus)
        onStatus (describe (drag));
    drag = {};
}

void GridView::preview (CellPos pos)
{
    const auto s = settings();
    processor.previewNote (rowNote (pos.row, s.scale, s.root), grid.get (slotOf (pos.row), pos.step).velocity);
}

void GridView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto pos = cellAt (e.position);
    if (! pos.valid())
        return;
    const auto mode = processor.editMode.load();
    const int slot = slotOf (pos.row);
    auto cell = grid.get (slot, pos.step);
    cell.on = true;
    setValue (cell, mode, valueOf (kDefaultCell, mode));
    grid.set (slot, pos.step, cell);
    if (mode == EditMode::Velocity)
        preview (pos);
    processor.gridEdited();
    repaint();
    if (onStatus)
        onStatus (describe (pos));
}

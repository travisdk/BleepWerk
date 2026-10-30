// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#include "SoundView.h"
#include "SoundShape.h"
#include "Theme.h"

SoundView::SoundView (BleepForgeProcessor& p)
    : processor (p)
{
    setInterceptsMouseClicks (false, false);
    timerCallback();
    startTimerHz (20);
}

SoundView::~SoundView()
{
    stopTimer();
}

// Reads the parameters and repaints when one of the sound's changed.
void SoundView::timerCallback()
{
    using namespace bleepforge;
    auto get = [this] (const char* id) { return processor.apvts.getRawParameterValue (id)->load(); };
    std::vector<float> values;
    for (const auto& param : kSoundParams)
        values.push_back (get (param.id));
    if (values == lastValues)
        return;
    lastValues = std::move (values);
    patch = readPatch (get);
    repaint();
}

void SoundView::paint (juce::Graphics& g)
{
    using namespace bleepforge;
    const auto area = getLocalBounds().toFloat();

    // a small screen cut into the panel
    g.setColour (juce::Colour (0xff0b0c0e));
    g.fillRect (area);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawRect (area, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.fillRect (area.getX(), area.getBottom() - 1.0f, area.getWidth(), 1.0f);

    // a faint grid, like a scope's
    g.setColour (theme::primary().withAlpha (0.07f));
    for (int i = 1; i < 8; ++i)
        g.fillRect (area.getX() + area.getWidth() * (float) i / 8.0f, area.getY() + 1.0f, 1.0f, area.getHeight() - 2.0f);
    g.fillRect (area.getX() + 1.0f, area.getCentreY(), area.getWidth() - 2.0f, 1.0f);

    const auto inner = area.reduced (8.0f, 5.0f);
    if (patch.engine == Engine::Bleep)
        paintBleep (g, inner);
    else
        paintModes (g, inner);
}

void SoundView::paintBleep (juce::Graphics& g, juce::Rectangle<float> r) const
{
    using namespace bleepforge;
    const auto& b = patch.bleep;
    const float end = bleepShapeEndMs (b);
    const int steps = juce::jmax (8, (int) r.getWidth());

    juce::Path amp, line, pitch;
    amp.startNewSubPath (r.getX(), r.getBottom());
    for (int i = 0; i <= steps; ++i)
    {
        const float t = end * (float) i / (float) steps;
        const float x = r.getX() + r.getWidth() * (float) i / (float) steps;
        const float y = r.getBottom() - r.getHeight() * bleepAmplitudeAt (b, t);
        amp.lineTo (x, y);
        i == 0 ? line.startNewSubPath (x, y) : line.lineTo (x, y);
    }
    amp.lineTo (r.getRight(), r.getBottom());
    amp.closeSubPath();
    g.setColour (theme::primary().withAlpha (0.2f));
    g.fillPath (amp);
    g.setColour (theme::primary());
    g.strokePath (line, juce::PathStrokeType (1.6f));

    // the pitch envelope over it: up for a drop in from above, down for one from below, drawn
    // from the middle with 48 semitones at the edge
    for (int i = 0; i <= steps; ++i)
    {
        const float t = end * (float) i / (float) steps;
        const float x = r.getX() + r.getWidth() * (float) i / (float) steps;
        const float y = r.getCentreY() - 0.5f * r.getHeight() * juce::jlimit (-1.0f, 1.0f, bleepPitchAt (b, t) / 48.0f);
        i == 0 ? pitch.startNewSubPath (x, y) : pitch.lineTo (x, y);
    }
    g.setColour (theme::text.withAlpha (0.8f));
    g.strokePath (pitch, juce::PathStrokeType (1.0f));

    g.setFont (bleepforge::fonts::value (10.0f));
    g.setColour (theme::dimText);
    g.drawText ("AMP / PITCH", r.withHeight (12.0f).withTrimmedLeft (0.0f), juce::Justification::topRight);
    g.drawText (juce::String (juce::roundToInt (end)) + " ms", r.removeFromBottom (12.0f), juce::Justification::bottomRight);
}

void SoundView::paintModes (juce::Graphics& g, juce::Rectangle<float> r) const
{
    using namespace bleepforge;
    const bool screech = patch.engine == Engine::Machine && patch.machine.type == MachineType::Screech;
    const auto modes = patch.engine == Engine::Foley ? foleyShape (patch.foleyParams) : machineShape (patch.machine);
    if (modes.empty())
        return;

    float lowest = 1.0e9f, highest = 0.0f;
    for (const auto& m : modes)
    {
        lowest = juce::jmin (lowest, m.ratio);
        highest = juce::jmax (highest, m.ratio);
    }
    const float span = std::log (juce::jmax (1.5f, highest / lowest) * 1.12f);
    r.removeFromBottom (11.0f); // room for the frequency marks

    // marks at the note and each octave above it
    g.setFont (bleepforge::fonts::value (10.0f));
    for (float ratio = 1.0f; ratio < highest * 1.12f; ratio *= 2.0f)
    {
        const float x = r.getX() + r.getWidth() * std::log (ratio / lowest) / span;
        g.setColour (theme::primary().withAlpha (0.18f));
        g.fillRect (x, r.getY(), 1.0f, r.getHeight() + 3.0f);
    }

    for (const auto& m : modes)
    {
        const float x = r.getX() + 3.0f + (r.getWidth() - 6.0f) * std::log (m.ratio / lowest) / span;
        const float height = juce::jmax (2.0f, r.getHeight() * m.strength);
        // a bar that rings longer is brighter: from 50 ms to 3 s
        const float ring = screech ? 1.0f : juce::jlimit (0.0f, 1.0f, std::log (juce::jmax (0.05f, m.ringSeconds) / 0.05f) / std::log (60.0f));
        g.setColour (theme::primary().withAlpha (0.3f + 0.7f * ring));
        g.fillRect (juce::Rectangle<float> (4.0f, height).withCentre ({ x, r.getBottom() - height * 0.5f }));
    }

    g.setColour (theme::dimText);
    g.drawText (screech ? "PARTIALS" : "RESONATOR", r.withHeight (12.0f), juce::Justification::topRight);
    g.drawText ("NOTE", juce::Rectangle<float> (r.getX() - 4.0f, r.getBottom(), 40.0f, 11.0f), juce::Justification::centredLeft);
}

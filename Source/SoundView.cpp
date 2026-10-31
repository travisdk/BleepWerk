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

    // a plain screen: a darker fill with a one-pixel outline
    g.setColour (theme::background);
    g.fillRect (area);
    g.setColour (theme::outline);
    g.drawRect (area, 1.0f);

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

    // a legend in plain words, and the length of the time axis
    g.setFont (bleepforge::fonts::value (10.0f));
    g.setColour (theme::primary());
    g.drawText ("VOLUME", r.withHeight (12.0f).withWidth (50.0f), juce::Justification::topLeft);
    g.setColour (theme::text.withAlpha (0.8f));
    g.drawText ("PITCH", r.withHeight (12.0f).withTrimmedLeft (52.0f).withWidth (40.0f), juce::Justification::topLeft);
    g.setColour (theme::dimText);
    auto bottom = r.removeFromBottom (12.0f);
    g.drawText ("0 ms", bottom, juce::Justification::bottomLeft);
    g.drawText (juce::String (juce::roundToInt (end)) + " ms", bottom, juce::Justification::bottomRight);
}

// Two small pictures side by side, each with its title: how the sound dies away over time
// (the same volume-over-time view as the Bleep), and its tones from low to high as bars.
// Screech does not ring, so it shows only its overtones.
void SoundView::paintModes (juce::Graphics& g, juce::Rectangle<float> r) const
{
    using namespace bleepforge;
    const bool screech = patch.engine == Engine::Machine && patch.machine.type == MachineType::Screech;
    const auto modes = patch.engine == Engine::Foley ? foleyShape (patch.foleyParams) : machineShape (patch.machine);
    if (modes.empty())
        return;

    g.setFont (bleepforge::fonts::value (10.0f));
    auto tones = r;
    if (! screech)
    {
        auto ringArea = tones.removeFromLeft ((r.getWidth() - 16.0f) * 0.5f);
        tones.removeFromLeft (16.0f);
        paintRing (g, ringArea, modes);
        g.setColour (theme::outline);
        g.fillRect (tones.getX() - 8.0f, r.getY(), 1.0f, r.getHeight());
    }
    paintTones (g, tones, modes, screech);
}

void SoundView::paintRing (juce::Graphics& g, juce::Rectangle<float> r, const std::vector<bleepforge::ShapeMode>& modes) const
{
    using namespace bleepforge;
    const float end = ringEndSeconds (modes);
    const int steps = juce::jmax (8, (int) r.getWidth());
    auto bottom = r.removeFromBottom (12.0f);
    auto title = r.removeFromTop (12.0f);
    r.removeFromTop (2.0f);

    juce::Path fill, line;
    fill.startNewSubPath (r.getX(), r.getBottom());
    for (int i = 0; i <= steps; ++i)
    {
        const float x = r.getX() + r.getWidth() * (float) i / (float) steps;
        const float y = r.getBottom() - r.getHeight() * ringLevelAt (modes, end * (float) i / (float) steps);
        fill.lineTo (x, y);
        i == 0 ? line.startNewSubPath (x, y) : line.lineTo (x, y);
    }
    fill.lineTo (r.getRight(), r.getBottom());
    fill.closeSubPath();
    g.setColour (theme::primary().withAlpha (0.2f));
    g.fillPath (fill);
    g.setColour (theme::primary());
    g.strokePath (line, juce::PathStrokeType (1.6f));

    g.setColour (theme::dimText);
    g.drawText ("HOW IT RINGS OUT", title, juce::Justification::topLeft);
    g.drawText ("hit", bottom, juce::Justification::bottomLeft);
    const int ms = juce::roundToInt (end * 1000.0f);
    g.drawText (ms >= 1000 ? juce::String (end, 1) + " s" : juce::String (ms) + " ms", bottom, juce::Justification::bottomRight);
}

void SoundView::paintTones (juce::Graphics& g, juce::Rectangle<float> r, const std::vector<bleepforge::ShapeMode>& modes, bool screech) const
{
    using namespace bleepforge;
    auto bottom = r.removeFromBottom (12.0f);
    auto title = r.removeFromTop (12.0f);
    r.removeFromTop (2.0f);

    float lowest = 1.0e9f, highest = 0.0f;
    for (const auto& m : modes)
    {
        lowest = juce::jmin (lowest, m.ratio);
        highest = juce::jmax (highest, m.ratio);
    }
    const float span = std::log (juce::jmax (1.5f, highest / lowest) * 1.12f);

    for (const auto& m : modes)
    {
        const float x = r.getX() + 3.0f + (r.getWidth() - 6.0f) * std::log (m.ratio / lowest) / span;
        const float height = juce::jmax (2.0f, r.getHeight() * m.strength);
        g.setColour (theme::primary());
        g.fillRect (juce::Rectangle<float> (4.0f, height).withCentre ({ x, r.getBottom() - height * 0.5f }));
    }

    g.setColour (theme::dimText);
    g.drawText (screech ? "OVERTONES, TALL = LOUD" : "TONES, TALL = LOUD", title, juce::Justification::topLeft);
    g.drawText ("low", bottom, juce::Justification::bottomLeft);
    g.drawText ("high", bottom, juce::Justification::bottomRight);
}

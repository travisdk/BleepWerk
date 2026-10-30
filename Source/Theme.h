// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Fonts.h"
#include "Themes.h"

// BleepForge's colours and the look of the standard JUCE controls it uses. The two theme
// shades come from the chosen colour theme (Themes.h), a per-user setting shared by every
// instance; the rest is fixed.
namespace bleepforge::theme
{
inline const juce::Colour background { 0xff000000 | kBackgroundRgb };
inline const juce::Colour panel { 0xff000000 | kPanelRgb };
inline const juce::Colour panelAlt { 0xff000000 | kPanelAltRgb }; // tooltips
inline const juce::Colour outline { 0xff3a3e46 };
inline const juce::Colour text { 0xffe9ecf1 };
inline const juce::Colour dimText { 0xff9ba4b3 }; // names; light enough to read at a glance

inline int& currentIndex()
{
    static int index = 0;
    return index;
}
inline const ThemeChoice& current() { return kThemes[(size_t) juce::jlimit (0, kNumThemes - 1, currentIndex())]; }

// The bright shade: knobs, titles, probability, root rows, active buttons.
inline juce::Colour primary() { return juce::Colour (0xff000000 | current().primary); }
// The deeper shade: velocity and ratchets.
inline juce::Colour secondary() { return juce::Colour (0xff000000 | current().secondary); }

inline int indexOf (const juce::String& name)
{
    for (int i = 0; i < kNumThemes; ++i)
        if (name == kThemes[i].name)
            return i;
    return 0;
}

// A panel of the control-panel look: brushed dark metal with a lit top-left edge and a shaded
// bottom-right one, a thin hazard stripe along the top in the theme's colour, and two screws
// at the bottom corners. The panels' own controls are laid out inside it.
inline void paintPanel (juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setGradientFill (juce::ColourGradient (panelAlt, 0.0f, area.getY(), panel.darker (0.2f), 0.0f, area.getBottom(), false));
    g.fillRect (area);
    g.setColour (juce::Colours::white.withAlpha (0.016f)); // the brushing
    for (float y = area.getY() + 2.0f; y < area.getBottom(); y += 3.0f)
        g.fillRect (area.getX(), y, area.getWidth(), 1.0f);

    g.setColour (juce::Colours::white.withAlpha (0.09f));
    g.fillRect (area.getX(), area.getY(), area.getWidth(), 1.0f);
    g.fillRect (area.getX(), area.getY(), 1.0f, area.getHeight());
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRect (area.getX(), area.getBottom() - 1.0f, area.getWidth(), 1.0f);
    g.fillRect (area.getRight() - 1.0f, area.getY(), 1.0f, area.getHeight());

    // the hazard stripe
    const auto strip = juce::Rectangle<float> (area.getX() + 1.0f, area.getY() + 1.0f, area.getWidth() - 2.0f, 3.0f);
    g.saveState();
    g.reduceClipRegion (strip.toNearestInt());
    g.setColour (primary().withAlpha (0.85f));
    g.fillRect (strip);
    g.setColour (juce::Colour (0xff17181b));
    for (float x = strip.getX() - 4.0f; x < strip.getRight(); x += 8.0f)
    {
        juce::Path bar;
        bar.addQuadrilateral (x, strip.getBottom(), x + 4.0f, strip.getBottom(), x + 7.0f, strip.getY(), x + 3.0f, strip.getY());
        g.fillPath (bar);
    }
    g.restoreState();

    // screws, each with its slot turned a different way
    int turn = 0;
    for (auto centre : { juce::Point<float> (area.getX() + 6.0f, area.getBottom() - 6.0f), juce::Point<float> (area.getRight() - 6.0f, area.getBottom() - 6.0f) })
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6b717b), centre.x - 1.5f, centre.y - 1.5f, juce::Colour (0xff23262b), centre.x + 2.5f, centre.y + 2.5f, true));
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (centre));
        g.setColour (juce::Colour (0xff111214));
        const float angle = 0.6f + 1.1f * (float) turn++;
        g.drawLine (juce::Line<float> (centre.getPointOnCircumference (2.4f, angle), centre.getPointOnCircumference (2.4f, angle + juce::MathConstants<float>::pi)), 1.2f);
    }
}

// A divider cut into the metal: a dark line with a light one beside it.
inline void paintEngravedLine (juce::Graphics& g, float x, float y, float height)
{
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRect (x, y, 1.0f, height);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.fillRect (x + 1.0f, y, 1.0f, height);
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        setDefaultSansSerifTypeface (fonts::labelTypeface());
        setColour (juce::ResizableWindow::backgroundColourId, background);
        setColour (juce::ComboBox::backgroundColourId, panel);
        setColour (juce::ComboBox::outlineColourId, outline);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::arrowColourId, dimText);
        setColour (juce::PopupMenu::backgroundColourId, panel);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedTextColourId, text);
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxBackgroundColourId, panel);
        setColour (juce::Slider::textBoxOutlineColourId, outline);
        setColour (juce::TextButton::buttonColourId, panel);
        setColour (juce::TextButton::textColourOffId, dimText);
        setColour (juce::TextButton::buttonOnColourId, outline);
        setColour (juce::TextButton::textColourOnId, text);
        setColour (juce::Label::textColourId, dimText);
        setColour (juce::Slider::backgroundColourId, outline);
        setColour (juce::Slider::thumbColourId, text);
        setColour (juce::TooltipWindow::backgroundColourId, panelAlt);
        setColour (juce::TooltipWindow::textColourId, text);
        setColour (juce::TooltipWindow::outlineColourId, outline);
        applyTheme();
    }

    // Takes on the current theme; the editor then sends a look-and-feel change so every
    // component repaints and re-reads its colours.
    void applyTheme()
    {
        setColour (juce::ComboBox::focusedOutlineColourId, primary());
        setColour (juce::PopupMenu::highlightedBackgroundColourId, primary().withAlpha (0.25f));
        setColour (juce::Slider::textBoxHighlightColourId, primary().withAlpha (0.3f));
        setColour (juce::Slider::trackColourId, primary());
    }

    // A ring with the value as an arc (from the middle for bipolar controls, from the start
    // otherwise) and a pointer.
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float position,
                           float startAngle, float endAngle, juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (2.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float angle = startAngle + position * (endAngle - startAngle);
        const bool bipolar = slider.getProperties()["bipolar"];
        const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
        const float thickness = juce::jmax (2.0f, radius * 0.14f);

        juce::Path track, value;
        track.addCentredArc (centre.x, centre.y, radius - thickness, radius - thickness, 0.0f, startAngle, endAngle, true);
        value.addCentredArc (centre.x, centre.y, radius - thickness, radius - thickness, 0.0f, juce::jmin (from, angle),
                             juce::jmax (from, angle), true);
        const juce::PathStrokeType stroke (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        g.setColour (outline);
        g.strokePath (track, stroke);
        g.setColour (slider.isEnabled() ? primary() : dimText);
        g.strokePath (value, stroke);

        // the cap: metal, lit from the top left
        const float body = radius - thickness * 2.4f;
        const auto cap = juce::Rectangle<float> (body * 2.0f, body * 2.0f).withCentre (centre);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff50555e), centre.x - body * 0.5f, centre.y - body * 0.6f,
                                                 juce::Colour (0xff17181b), centre.x + body * 0.7f, centre.y + body * 0.8f, true));
        g.fillEllipse (cap);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawEllipse (cap, 1.0f);
        g.setColour (text);
        g.drawLine (juce::Line<float> (centre.getPointOnCircumference (body * 0.25f, angle),
                                       centre.getPointOnCircumference (body * 0.9f, angle)), 2.0f);
    }

    // Buttons and selectors with square corners like the panels; JUCE's own are rounded.
    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                               bool highlighted, bool down) override
    {
        const auto bounds = button.getLocalBounds().toFloat();
        auto base = backgroundColour.withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.5f);
        if (down || highlighted)
            base = base.contrasting (down ? 0.2f : 0.05f);
        g.setColour (base);
        g.fillRect (bounds);
        g.setColour (button.findColour (juce::ComboBox::outlineColourId));
        g.drawRect (bounds, 1.0f);
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
    {
        const auto bounds = juce::Rectangle<int> (width, height).toFloat();
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRect (bounds);
        g.setColour (box.findColour (box.hasKeyboardFocus (true) ? juce::ComboBox::focusedOutlineColourId
                                                                  : juce::ComboBox::outlineColourId));
        g.drawRect (bounds, 1.0f);

        const juce::Rectangle<float> arrow ((float) width - 30.0f, 0.0f, 20.0f, (float) height);
        juce::Path path;
        path.startNewSubPath (arrow.getX() + 3.0f, arrow.getCentreY() - 2.0f);
        path.lineTo (arrow.getCentreX(), arrow.getCentreY() + 3.0f);
        path.lineTo (arrow.getRight() - 3.0f, arrow.getCentreY() - 2.0f);
        g.setColour (box.findColour (juce::ComboBox::arrowColourId).withAlpha (box.isEnabled() ? 0.9f : 0.2f));
        g.strokePath (path, juce::PathStrokeType (2.0f));
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return fonts::label (15.0f); }
    juce::Font getPopupMenuFont() override { return fonts::label (16.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return fonts::label (14.5f); }

    juce::Label* createSliderTextBox (juce::Slider& slider) override
    {
        auto* label = LookAndFeel_V4::createSliderTextBox (slider);
        label->setFont (fonts::value (12.0f));
        // a small readout cut into the panel
        label->setColour (juce::Label::outlineColourId, juce::Colours::black.withAlpha (0.55f));
        label->setColour (juce::Label::backgroundColourId, juce::Colour (0xff0e0f11));
        return label;
    }
};
} // namespace bleepforge::theme

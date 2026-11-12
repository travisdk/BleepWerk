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

// A panel: a flat fill with a plain one-pixel outline.
inline void paintPanel (juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour (panel);
    g.fillRect (area);
    g.setColour (outline);
    g.drawRect (area, 1.0f);
}

// A divider: a plain one-pixel line.
inline void paintDivider (juce::Graphics& g, float x, float y, float height)
{
    g.setColour (outline);
    g.fillRect (x, y, 1.0f, height);
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

    // Tooltips: narrow boxes that wrap onto more lines, with more room around the text. The
    // lines are balanced, so a box has no short last line. JUCE's own are 400 px wide and tight.
    static constexpr int kTipTextWidth = 220, kTipPadX = 16, kTipPadY = 10;

    juce::TextLayout tipLayout (const juce::String& tip, juce::Colour colour)
    {
        juce::AttributedString s;
        s.setWordWrap (juce::AttributedString::byWord);
        s.setJustification (juce::Justification::centred);
        s.append (tip, juce::FontOptions (13.0f, juce::Font::bold).withMetricsKind (getDefaultMetricsKind()), colour);
        juce::TextLayout layout;
        layout.createLayoutWithBalancedLineLengths (s, (float) kTipTextWidth);
        return layout;
    }

    juce::Rectangle<int> getTooltipBounds (const juce::String& tip, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override
    {
        const auto layout = tipLayout (tip, juce::Colours::black);
        const int w = juce::roundToInt (layout.getWidth()) + 2 * kTipPadX;
        const int h = juce::roundToInt (layout.getHeight()) + 2 * kTipPadY;
        return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                     screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6,
                                     w, h)
            .constrainedWithin (parentArea);
    }

    void drawTooltip (juce::Graphics& g, const juce::String& tip, int width, int height) override
    {
        const auto box = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
        g.setColour (findColour (juce::TooltipWindow::backgroundColourId));
        g.fillRoundedRectangle (box, 5.0f);
        g.setColour (findColour (juce::TooltipWindow::outlineColourId));
        g.drawRoundedRectangle (box.reduced (0.5f), 5.0f, 1.0f);
        tipLayout (tip, findColour (juce::TooltipWindow::textColourId))
            .draw (g, box.reduced ((float) kTipPadX, (float) kTipPadY));
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
        label->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        label->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        return label;
    }
};
} // namespace bleepforge::theme

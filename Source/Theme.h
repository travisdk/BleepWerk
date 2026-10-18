// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Themes.h"

// BleepForge's colours and the look of the standard JUCE controls it uses. The two theme
// shades come from the chosen colour theme (Themes.h), a per-user setting shared by every
// instance; the rest is fixed.
namespace bleepforge::theme
{
inline const juce::Colour background { 0xff000000 | kBackgroundRgb };
inline const juce::Colour panel { 0xff000000 | kPanelRgb };
inline const juce::Colour panelAlt { 0xff000000 | kPanelAltRgb }; // every second beat in the grid
inline const juce::Colour outline { 0xff2b3039 };
inline const juce::Colour text { 0xffd8dce4 };
inline const juce::Colour dimText { 0xff7a8190 };

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

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
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
        const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (4.0f);
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

        const float body = radius - thickness * 2.4f;
        g.setColour (background);
        g.fillEllipse (juce::Rectangle<float> (body * 2.0f, body * 2.0f).withCentre (centre));
        g.setColour (text);
        g.drawLine (juce::Line<float> (centre.getPointOnCircumference (body * 0.25f, angle),
                                       centre.getPointOnCircumference (body * 0.9f, angle)), 2.0f);
    }

    juce::Label* createSliderTextBox (juce::Slider& slider) override
    {
        auto* label = LookAndFeel_V4::createSliderTextBox (slider);
        label->setFont (juce::FontOptions (11.0f));
        label->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        label->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        return label;
    }
};
} // namespace bleepforge::theme

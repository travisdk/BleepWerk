// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// BleepForge's colours and the look of the standard JUCE controls it uses.
namespace bleepforge::theme
{
inline const juce::Colour background { 0xff111317 };
inline const juce::Colour panel { 0xff1a1d23 };
inline const juce::Colour panelAlt { 0xff20242c }; // every second beat in the grid
inline const juce::Colour outline { 0xff2b3039 };
inline const juce::Colour text { 0xffd8dce4 };
inline const juce::Colour dimText { 0xff7a8190 };
inline const juce::Colour accent { 0xff4fe0b0 };
inline const juce::Colour velocity { 0xffffb547 }; // the grid in Velocity mode

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
        setColour (juce::ComboBox::focusedOutlineColourId, accent);
        setColour (juce::PopupMenu::backgroundColourId, panel);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.25f));
        setColour (juce::PopupMenu::highlightedTextColourId, text);
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxBackgroundColourId, panel);
        setColour (juce::Slider::textBoxOutlineColourId, outline);
        setColour (juce::TextButton::buttonColourId, panel);
        setColour (juce::TextButton::textColourOffId, dimText);
        setColour (juce::TextButton::buttonOnColourId, outline);
        setColour (juce::TextButton::textColourOnId, text);
        setColour (juce::Label::textColourId, dimText);
    }
};
} // namespace bleepforge::theme

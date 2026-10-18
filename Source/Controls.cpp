// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#include "Controls.h"
#include "Theme.h"

namespace
{
void styleName (juce::Label& label, const juce::String& name)
{
    label.setText (name, juce::dontSendNotification);
    label.setFont (juce::FontOptions (12.0f));
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, bleepforge::theme::dimText);
    label.setInterceptsMouseClicks (false, false);
}
} // namespace

Knob::Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& parameterId, const juce::String& name, bool bipolar)
    : attachment (apvts, parameterId, slider)
{
    styleName (label, name);
    addAndMakeVisible (label);
    slider.getProperties().set ("bipolar", bipolar);
    if (auto* parameter = apvts.getParameter (parameterId))
        slider.setDoubleClickReturnValue (true, parameter->convertFrom0to1 (parameter->getDefaultValue()));
    addAndMakeVisible (slider);
}

void Knob::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (16));
    // the value box takes the knob's full width, so narrow knobs still show their value
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, getWidth(), 16);
    slider.setBounds (area);
}

Choice::Choice (juce::AudioProcessorValueTreeState& apvts, const juce::String& parameterId, const juce::String& name)
    : attachment (apvts, parameterId, box)
{
    // the attachment fills nothing in: the items come from the choice parameter
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (parameterId)))
    {
        box.addItemList (choice->choices, 1);
        box.setSelectedItemIndex (choice->getIndex(), juce::dontSendNotification);
    }
    styleName (label, name);
    addAndMakeVisible (label);
    addAndMakeVisible (box);
}

void Choice::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (16));
    box.setBounds (area.removeFromTop (28).reduced (2, 0));
}

Toggle::Toggle (juce::AudioProcessorValueTreeState& apvts, const juce::String& parameterId, const juce::String& name)
    : parameter (*apvts.getParameter (parameterId))
{
    styleName (label, name);
    label.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (label);

    const auto names = parameter.getAllValueStrings();
    left.setButtonText (names[0]);
    right.setButtonText (names[1]);
    left.setConnectedEdges (juce::Button::ConnectedOnRight);
    right.setConnectedEdges (juce::Button::ConnectedOnLeft);
    for (auto* button : { &left, &right })
        addAndMakeVisible (*button);
    lookAndFeelChanged();
    auto choose = [this] (float value)
    {
        parameter.beginChangeGesture();
        parameter.setValueNotifyingHost (value);
        parameter.endChangeGesture();
        update();
    };
    left.onClick = [choose] { choose (0.0f); };
    right.onClick = [choose] { choose (1.0f); };
    update();
}

void Toggle::lookAndFeelChanged()
{
    for (auto* button : { &left, &right })
        button->setColour (juce::TextButton::textColourOnId, bleepforge::theme::primary());
}

void Toggle::update()
{
    const bool second = parameter.getValue() >= 0.5f;
    left.setToggleState (! second, juce::dontSendNotification);
    right.setToggleState (second, juce::dontSendNotification);
}

void Toggle::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (16));
    area = area.removeFromTop (26);
    left.setBounds (area.removeFromLeft (area.getWidth() / 2));
    right.setBounds (area);
}


IconButton::IconButton (Icon i, const juce::String& tooltip)
    : juce::Button (tooltip), icon (i)
{
    setTooltip (tooltip);
}

void IconButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    using namespace bleepforge;
    const auto colour = ! isEnabled() ? theme::outline : down ? theme::primary() : highlighted ? theme::text : theme::dimText;
    g.setColour (colour);
    const auto bounds = getLocalBounds().toFloat();
    const float side = juce::jmin (bounds.getWidth(), bounds.getHeight()) - 4.0f;
    const auto area = juce::Rectangle<float> (side, side).withCentre (bounds.getCentre());

    if (icon == Icon::Dice)
    {
        // the five face: an outlined square with five pips
        g.drawRect (area, 1.5f);
        const float pip = side * 0.16f, inset = side * 0.28f;
        const auto inner = area.reduced (inset);
        for (auto centre : { inner.getTopLeft(), inner.getTopRight(), inner.getCentre(), inner.getBottomLeft(), inner.getBottomRight() })
            g.fillEllipse (juce::Rectangle<float> (pip, pip).withCentre (centre));
        return;
    }

    // a counter-clockwise arc ending in an arrowhead at its top left
    const auto centre = area.getCentre();
    const float radius = side * 0.36f;
    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, -1.6f, 1.9f, true);
    g.strokePath (arc, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const auto tip = centre.getPointOnCircumference (radius, -1.6f);
    juce::Path head;
    head.addTriangle (tip.translated (-side * 0.2f, 0.0f), tip.translated (side * 0.08f, -side * 0.18f), tip.translated (side * 0.08f, side * 0.18f));
    g.fillPath (head);
}

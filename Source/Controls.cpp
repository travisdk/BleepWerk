// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#include "Controls.h"
#include "Theme.h"

namespace
{
void styleName (juce::Label& label, const juce::String& name)
{
    label.setText (name, juce::dontSendNotification);
    label.setFont (bleepforge::fonts::label (14.0f));
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
    label.setBounds (area.removeFromTop (kLabelHeight));
    // the value box takes the knob's full width, so narrow knobs still show their value
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, getWidth(), kValueHeight);
    const int diameter = juce::jmax (0, juce::jmin (area.getWidth(), area.getHeight() - kValueHeight, maxDiameter));
    slider.setBounds (area.removeFromTop (diameter + kValueHeight));
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
    box.setBounds (area.withSizeKeepingCentre (area.getWidth() - 4, 28));
}

Toggle::Toggle (juce::AudioProcessorValueTreeState& apvts, const juce::String& parameterId, const juce::String& name,
                bool stackButtons, const juce::StringArray& labels)
    : parameter (*apvts.getParameter (parameterId)), stacked (stackButtons)
{
    styleName (label, name);
    label.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (label);

    const auto names = labels.isEmpty() ? parameter.getAllValueStrings() : labels;
    const int last = names.size() - 1;
    const int before = stacked ? juce::Button::ConnectedOnTop : juce::Button::ConnectedOnLeft;
    const int after = stacked ? juce::Button::ConnectedOnBottom : juce::Button::ConnectedOnRight;
    for (int i = 0; i <= last; ++i)
    {
        auto* button = buttons.add (new juce::TextButton (names[i]));
        button->setConnectedEdges ((i > 0 ? before : 0) | (i < last ? after : 0));
        button->onClick = [this, i, last]
        {
            parameter.beginChangeGesture();
            parameter.setValueNotifyingHost ((float) i / (float) juce::jmax (1, last));
            parameter.endChangeGesture();
            update();
        };
        addAndMakeVisible (button);
    }
    lookAndFeelChanged();
    update();
}

void Toggle::lookAndFeelChanged()
{
    for (auto* button : buttons)
        button->setColour (juce::TextButton::textColourOnId, bleepforge::theme::primary());
}

void Toggle::update()
{
    const int selected = juce::roundToInt (parameter.getValue() * (float) juce::jmax (1, buttons.size() - 1));
    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setToggleState (i == selected, juce::dontSendNotification);
}

void Toggle::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (16));
    if (stacked)
    {
        const int height = area.getHeight() / juce::jmax (1, buttons.size());
        for (int i = 0; i < buttons.size(); ++i)
            buttons[i]->setBounds (i == buttons.size() - 1 ? area : area.removeFromTop (height));
        return;
    }
    area = area.withSizeKeepingCentre (area.getWidth(), 26);
    const int width = area.getWidth() / juce::jmax (1, buttons.size());
    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setBounds (i == buttons.size() - 1 ? area : area.removeFromLeft (width));
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

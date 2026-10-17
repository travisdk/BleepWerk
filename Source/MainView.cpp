// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#include "MainView.h"
#include "Theme.h"
#include "BleepForgeVersion.h"

namespace
{
constexpr int kMargin = 16, kHeaderHeight = 40, kStatusHeight = 22, kEngineHeight = 150;
juce::String helpText (BleepForgeProcessor::EditMode mode)
{
    const juce::String value = mode == BleepForgeProcessor::EditMode::Velocity ? "velocity" : "probability";
    return "Click: on/off    Drag up/down: " + value + "    Double-click: reset " + value;
}
} // namespace

MainView::MainView (BleepForgeProcessor& p)
    : processor (p), gridView (p)
{
    setSize (kWidth, kHeight);

    for (const auto& scale : bleepforge::kScales)
        scaleBox.addItem (scale.name, scaleBox.getNumItems() + 1);
    for (auto* name : bleepforge::kRootNames)
        rootBox.addItem (name, rootBox.getNumItems() + 1);
    scaleAttachment = std::make_unique<ComboAttachment> (processor.apvts, "seqScale", scaleBox);
    rootAttachment = std::make_unique<ComboAttachment> (processor.apvts, "seqRoot", rootBox);
    octaveSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 34, 24);
    octaveAttachment = std::make_unique<SliderAttachment> (processor.apvts, "seqOctave", octaveSlider);

    for (auto* label : { &scaleLabel, &rootLabel, &octaveLabel })
    {
        label->setJustificationType (juce::Justification::centredRight);
        label->setFont (juce::FontOptions (13.0f));
        addAndMakeVisible (*label);
    }
    addAndMakeVisible (scaleBox);
    addAndMakeVisible (rootBox);
    addAndMakeVisible (octaveSlider);

    status.setFont (juce::FontOptions (12.0f));
    status.setColour (juce::Label::textColourId, bleepforge::theme::dimText);
    addAndMakeVisible (status);
    showStatus ({});

    // edit mode: one value at a time, the other button pops out
    editLabel.setJustificationType (juce::Justification::centredRight);
    editLabel.setFont (juce::FontOptions (13.0f));
    addAndMakeVisible (editLabel);
    for (auto* button : { &probabilityButton, &velocityButton })
    {
        button->setClickingTogglesState (false);
        addAndMakeVisible (*button);
    }
    probabilityButton.setConnectedEdges (juce::Button::ConnectedOnRight);
    velocityButton.setConnectedEdges (juce::Button::ConnectedOnLeft);
    probabilityButton.onClick = [this] { setEditMode (BleepForgeProcessor::EditMode::Probability); };
    velocityButton.onClick = [this] { setEditMode (BleepForgeProcessor::EditMode::Velocity); };
    setEditMode (processor.editMode.load());

    gridView.onStatus = [this] (const juce::String& text) { showStatus (text); };
    addAndMakeVisible (gridView);
}

void MainView::showStatus (const juce::String& text)
{
    status.setText (text.isEmpty() ? helpText (processor.editMode.load()) : text, juce::dontSendNotification);
}

void MainView::setEditMode (BleepForgeProcessor::EditMode mode)
{
    using namespace bleepforge;
    processor.editMode = mode;
    const bool velocity = mode == BleepForgeProcessor::EditMode::Velocity;
    probabilityButton.setToggleState (! velocity, juce::dontSendNotification);
    velocityButton.setToggleState (velocity, juce::dontSendNotification);
    probabilityButton.setColour (juce::TextButton::textColourOnId, theme::accent);
    velocityButton.setColour (juce::TextButton::textColourOnId, theme::velocity);
    gridView.editModeChanged();
    showStatus ({});
}

juce::Rectangle<int> MainView::headerArea() const
{
    return getLocalBounds().reduced (kMargin).removeFromTop (kHeaderHeight);
}

juce::Rectangle<int> MainView::engineArea() const
{
    return getLocalBounds().reduced (kMargin).removeFromBottom (kEngineHeight);
}

void MainView::paint (juce::Graphics& g)
{
    using namespace bleepforge;
    g.fillAll (theme::background);

    auto header = headerArea().toFloat();
    g.setColour (theme::accent);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("BleepForge", header, juce::Justification::centredLeft);
    g.setColour (theme::dimText);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (BLEEPFORGE_VERSION_STRING, header.withTrimmedLeft (128.0f).withTrimmedTop (6.0f),
                juce::Justification::centredLeft);

    const auto engine = engineArea().toFloat();
    g.setColour (theme::panel);
    g.fillRoundedRectangle (engine, 6.0f);
    g.setColour (theme::outline);
    g.drawRoundedRectangle (engine.reduced (0.5f), 6.0f, 1.0f);
    g.setColour (theme::dimText);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("Engine / Chaos / Effects", engine, juce::Justification::centred);
}

void MainView::resized()
{
    auto header = headerArea();
    auto place = [&header] (juce::Component& c, int width) { c.setBounds (header.removeFromRight (width).withSizeKeepingCentre (width, 26)); };
    place (octaveSlider, 96);
    place (octaveLabel, 54);
    header.removeFromRight (12);
    place (rootBox, 64);
    place (rootLabel, 40);
    header.removeFromRight (12);
    place (scaleBox, 160);
    place (scaleLabel, 44);

    // edit mode, after the title
    auto left = headerArea();
    left.removeFromLeft (190);
    editLabel.setBounds (left.removeFromLeft (36).withSizeKeepingCentre (36, 26));
    left.removeFromLeft (6);
    probabilityButton.setBounds (left.removeFromLeft (90).withSizeKeepingCentre (90, 26));
    velocityButton.setBounds (left.removeFromLeft (76).withSizeKeepingCentre (76, 26));

    auto middle = getLocalBounds().reduced (kMargin);
    middle.removeFromTop (kHeaderHeight + 8);
    middle.removeFromBottom (kEngineHeight + 10);
    status.setBounds (middle.removeFromBottom (kStatusHeight).withTrimmedLeft ((int) GridView::kLabelWidth - 4));
    gridView.setBounds (middle);
}

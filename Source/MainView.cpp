// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#include "MainView.h"
#include "Theme.h"
#include "BleepForgeVersion.h"

namespace
{
constexpr int kMargin = 16, kHeaderHeight = 40, kStatusHeight = 22, kEngineHeight = 150;
const juce::String kHelp = "Click: on/off    Drag up/down: probability    Shift+drag: velocity    Double-click: reset";
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

    gridView.onStatus = [this] (const juce::String& text) { showStatus (text); };
    addAndMakeVisible (gridView);
}

void MainView::showStatus (const juce::String& text)
{
    status.setText (text.isEmpty() ? kHelp : text, juce::dontSendNotification);
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

    auto middle = getLocalBounds().reduced (kMargin);
    middle.removeFromTop (kHeaderHeight + 8);
    middle.removeFromBottom (kEngineHeight + 10);
    status.setBounds (middle.removeFromBottom (kStatusHeight).withTrimmedLeft ((int) GridView::kLabelWidth - 4));
    gridView.setBounds (middle);
}

// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#include "MainView.h"
#include "Theme.h"
#include "BleepForgeVersion.h"

namespace
{
constexpr int kMargin = 16, kHeaderHeight = 40, kBarHeight = 28, kStatusHeight = 22, kEngineHeight = 150, kBottomHeight = 120,
              kGap = 10, kChaosWidth = 350;

using EditMode = BleepForgeProcessor::EditMode;

juce::String helpText (EditMode mode)
{
    const juce::String value = mode == EditMode::Velocity ? "velocity" : mode == EditMode::Ratchet ? "ratchet (1-4 hits)" : "probability";
    return "Click: on/off    Drag up/down: " + value + "    Double-click: reset";
}

void styleBarLabel (juce::Label& label)
{
    label.setJustificationType (juce::Justification::centredRight);
    label.setFont (juce::FontOptions (13.0f));
}
} // namespace

MainView::MainView (BleepForgeProcessor& p)
    : processor (p), gridView (p), engineView (p), chaosView (p), effectsView (p)
{
    setSize (kWidth, kHeight);
    using namespace bleepforge;

    // header: edit mode and settings
    editLabel.setJustificationType (juce::Justification::centredRight);
    editLabel.setFont (juce::FontOptions (13.0f));
    addAndMakeVisible (editLabel);
    for (auto* button : { &probabilityButton, &velocityButton, &ratchetButton })
    {
        button->setClickingTogglesState (false);
        addAndMakeVisible (*button);
    }
    probabilityButton.setConnectedEdges (juce::Button::ConnectedOnRight);
    velocityButton.setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    ratchetButton.setConnectedEdges (juce::Button::ConnectedOnLeft);
    probabilityButton.onClick = [this] { setEditMode (EditMode::Probability); };
    velocityButton.onClick = [this] { setEditMode (EditMode::Velocity); };
    ratchetButton.onClick = [this] { setEditMode (EditMode::Ratchet); };
    settingsButton.setTooltip ("Settings");
    settingsButton.onClick = [this] { showSettingsMenu(); };
    addAndMakeVisible (settingsButton);

    // sequencer bar
    for (const auto& scale : kScales)
        scaleBox.addItem (scale.name, scaleBox.getNumItems() + 1);
    for (auto* name : kRootNames)
        rootBox.addItem (name, rootBox.getNumItems() + 1);
    for (auto* name : kSeqRateNames)
        rateBox.addItem (name, rateBox.getNumItems() + 1);
    scaleAttachment = std::make_unique<ComboAttachment> (processor.apvts, "seqScale", scaleBox);
    rootAttachment = std::make_unique<ComboAttachment> (processor.apvts, "seqRoot", rootBox);
    rateAttachment = std::make_unique<ComboAttachment> (processor.apvts, "seqRate", rateBox);

    stepsSlider.setRange (1.0, (double) kMaxSteps, 1.0);
    stepsSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 34, 24);
    stepsSlider.setValue (processor.getGrid().getLength(), juce::dontSendNotification);
    stepsSlider.onValueChange = [this]
    {
        processor.getGrid().setLength ((int) stepsSlider.getValue());
        processor.gridEdited();
    };

    swingSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 24);
    swingAttachment = std::make_unique<SliderAttachment> (processor.apvts, "seqSwing", swingSlider);
    if (auto* swing = processor.apvts.getParameter ("seqSwing"))
        swingSlider.setDoubleClickReturnValue (true, swing->convertFrom0to1 (swing->getDefaultValue()));

    for (auto* label : { &scaleLabel, &rootLabel, &stepsLabel, &rateLabel, &swingLabel })
    {
        styleBarLabel (*label);
        addAndMakeVisible (*label);
    }
    for (auto* c : std::initializer_list<juce::Component*> { &scaleBox, &rootBox, &stepsSlider, &rateBox, &swingSlider })
        addAndMakeVisible (c);

    // grid, status line and sound
    status.setFont (juce::FontOptions (12.0f));
    status.setColour (juce::Label::textColourId, theme::dimText);
    addAndMakeVisible (status);
    gridView.onStatus = [this] (const juce::String& text) { showStatus (text); };
    addAndMakeVisible (gridView);
    addAndMakeVisible (engineView);
    addAndMakeVisible (chaosView);
    addAndMakeVisible (effectsView);

    setEditMode (processor.editMode.load());
    startTimerHz (10);
}

MainView::~MainView()
{
    stopTimer();
}

// The grid's length isn't a parameter, so a loaded project doesn't tell the slider.
void MainView::timerCallback()
{
    const int length = processor.getGrid().getLength();
    if ((int) stepsSlider.getValue() != length && ! stepsSlider.isMouseButtonDown())
        stepsSlider.setValue (length, juce::dontSendNotification);
}

void MainView::showStatus (const juce::String& text)
{
    status.setText (text.isEmpty() ? helpText (processor.editMode.load()) : text, juce::dontSendNotification);
}

void MainView::setEditMode (EditMode mode)
{
    using namespace bleepforge;
    processor.editMode = mode;
    probabilityButton.setToggleState (mode == EditMode::Probability, juce::dontSendNotification);
    velocityButton.setToggleState (mode == EditMode::Velocity, juce::dontSendNotification);
    ratchetButton.setToggleState (mode == EditMode::Ratchet, juce::dontSendNotification);
    probabilityButton.setColour (juce::TextButton::textColourOnId, theme::accent);
    velocityButton.setColour (juce::TextButton::textColourOnId, theme::velocity);
    ratchetButton.setColour (juce::TextButton::textColourOnId, theme::ratchet);
    gridView.editModeChanged();
    showStatus ({});
}

void MainView::showSettingsMenu()
{
    auto* range = processor.apvts.getParameter ("seqRange");
    const int current = (int) processor.apvts.getRawParameterValue ("seqRange")->load();

    juce::PopupMenu rangeMenu;
    for (int octaves = 1; octaves <= BleepForgeProcessor::kMaxVisibleOctaves; ++octaves)
        rangeMenu.addItem (juce::String (octaves) + (octaves == 1 ? " octave" : " octaves"), true, octaves == current,
                           [range, octaves]
                           {
                               range->beginChangeGesture();
                               range->setValueNotifyingHost (range->convertTo0to1 ((float) octaves));
                               range->endChangeGesture();
                           });

    juce::PopupMenu menu;
    menu.addSectionHeader ("Grid");
    menu.addSubMenu ("Visible range: " + juce::String (current) + (current == 1 ? " octave" : " octaves"), rangeMenu);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (settingsButton));
}

void MainView::GearButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    using namespace bleepforge;
    const auto area = getLocalBounds().toFloat().reduced (4.0f);
    const auto centre = area.getCentre();
    const float outer = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f, inner = outer * 0.72f;

    juce::Path gear;
    constexpr int teeth = 8;
    for (int i = 0; i < teeth * 2; ++i)
    {
        // each tooth is a flat top on the outer radius, the gaps sit on the inner one
        const float radius = (i % 2 == 0) ? outer : inner;
        for (float edge : { -0.5f, 0.5f })
        {
            const float angle = juce::MathConstants<float>::twoPi * ((float) i + edge * 0.8f) / (float) (teeth * 2);
            const auto p = centre.getPointOnCircumference (radius, angle);
            if (i == 0 && edge < 0.0f)
                gear.startNewSubPath (p);
            else
                gear.lineTo (p);
        }
    }
    gear.closeSubPath();
    gear.addEllipse (juce::Rectangle<float> (outer * 0.7f, outer * 0.7f).withCentre (centre));
    gear.setUsingNonZeroWinding (false);

    g.setColour (down ? theme::accent : (highlighted ? theme::text : theme::dimText));
    g.fillPath (gear);
}

juce::Rectangle<int> MainView::headerArea() const
{
    return getLocalBounds().reduced (kMargin).removeFromTop (kHeaderHeight);
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
}

void MainView::resized()
{
    auto area = getLocalBounds().reduced (kMargin);

    // header: edit mode after the title, settings on the right
    auto header = area.removeFromTop (kHeaderHeight);
    settingsButton.setBounds (header.removeFromRight (26).withSizeKeepingCentre (26, 26));
    header.removeFromLeft (176);
    editLabel.setBounds (header.removeFromLeft (36).withSizeKeepingCentre (36, 26));
    header.removeFromLeft (6);
    for (auto [button, width] : { std::pair { &probabilityButton, 86 }, { &velocityButton, 70 }, { &ratchetButton, 70 } })
        button->setBounds (header.removeFromLeft (width).withSizeKeepingCentre (width, 26));

    // sequencer bar
    area.removeFromTop (4);
    auto bar = area.removeFromTop (kBarHeight);
    auto place = [&bar] (juce::Component& label, int labelWidth, juce::Component& control, int width)
    {
        label.setBounds (bar.removeFromLeft (labelWidth));
        bar.removeFromLeft (6);
        control.setBounds (bar.removeFromLeft (width).withSizeKeepingCentre (width, 24));
        bar.removeFromLeft (18);
    };
    place (scaleLabel, 44, scaleBox, 160);
    place (rootLabel, 36, rootBox, 64);
    place (stepsLabel, 44, stepsSlider, 96);
    place (rateLabel, 36, rateBox, 78);
    place (swingLabel, 44, swingSlider, 180);

    // sound at the bottom: the engine, then Chaos and the effects side by side
    auto bottom = area.removeFromBottom (kBottomHeight);
    chaosView.setBounds (bottom.removeFromLeft (kChaosWidth));
    bottom.removeFromLeft (kGap);
    effectsView.setBounds (bottom);
    area.removeFromBottom (kGap);
    engineView.setBounds (area.removeFromBottom (kEngineHeight));
    area.removeFromBottom (kGap);

    area.removeFromTop (8);
    status.setBounds (area.removeFromBottom (kStatusHeight).withTrimmedLeft ((int) GridView::kLabelWidth - 4));
    gridView.setBounds (area);
}

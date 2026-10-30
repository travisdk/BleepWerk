// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#include "DriveView.h"
#include "Theme.h"

namespace
{
constexpr int kPadding = 10, kTitleHeight = 12, kChoiceWidth = 72, kPlaceWidth = 88, kKnobWidth = 72;
} // namespace

DriveView::DriveView (BleepForgeProcessor& p)
    : type (p.apvts, "driveType", "Type"),
      place (p.apvts, "drivePost", "", true, { "Pre filter", "Post reverb" }),
      amount (p.apvts, "driveAmount", "Drive"),
      mix (p.apvts, "driveMix", "Mix")
{
    for (auto* knob : { &amount, &mix })
        knob->setMaxDiameter (kDiameter);
    for (auto* c : std::initializer_list<juce::Component*> { &type, &place, &amount, &mix })
        addAndMakeVisible (c);

    startTimerHz (10);
}

DriveView::~DriveView()
{
    stopTimer();
}

// The switch follows automation and the host too.
void DriveView::timerCallback()
{
    place.update();
}

void DriveView::paint (juce::Graphics& g)
{
    using namespace bleepforge;
    const auto area = getLocalBounds().toFloat();
    theme::paintPanel (g, area);

    g.setColour (theme::primary().withAlpha (0.8f));
    g.setFont (fonts::title (14.0f));
    g.drawText ("DRIVE", juce::Rectangle<float> ((float) kPadding, 6.0f, 120.0f, 14.0f), juce::Justification::centredLeft);
}

// Spread over the width, all on the lines of the knobs.
void DriveView::resized()
{
    auto area = getLocalBounds().reduced (kPadding);
    area.removeFromTop (kTitleHeight);
    area = area.removeFromTop (Knob::heightFor (kDiameter));

    const int spare = area.getWidth() - kChoiceWidth - kPlaceWidth - 2 * kKnobWidth;
    const int gap = spare / 3;
    type.setBounds (area.removeFromLeft (kChoiceWidth).withTrimmedBottom (Knob::kValueHeight));
    area.removeFromLeft (gap);
    place.setBounds (area.removeFromLeft (kPlaceWidth).withTrimmedBottom (Knob::kValueHeight));
    area.removeFromLeft (gap);
    amount.setBounds (area.removeFromLeft (kKnobWidth));
    area.removeFromLeft (gap);
    mix.setBounds (area.removeFromLeft (kKnobWidth));
}

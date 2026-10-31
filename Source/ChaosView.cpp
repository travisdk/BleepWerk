// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#include "ChaosView.h"
#include "DriveView.h"
#include "Theme.h"

namespace
{
constexpr int kPadding = 10, kTitleHeight = 12, kKnobWidth = 64;
} // namespace

ChaosView::ChaosView (BleepForgeProcessor& p)
    : amount (p.apvts, "chaosAmount", "Amount"),
      pitch (p.apvts, "chaosPitch", "Pitch"),
      decay (p.apvts, "chaosDecay", "Decay"),
      tone (p.apvts, "chaosTone", "Tone"),
      pan (p.apvts, "chaosPan", "Pan"),
      velocity (p.apvts, "chaosVelocity", "Velocity")
{
    for (auto* knob : { &amount, &pitch, &decay, &tone, &pan, &velocity })
    {
        knob->setMaxDiameter (DriveView::kDiameter);
        addAndMakeVisible (knob);
    }
}

void ChaosView::paint (juce::Graphics& g)
{
    using namespace bleepforge;
    const auto area = getLocalBounds().toFloat();
    theme::paintPanel (g, area);

    // Amount stands apart: it scales the others
    const float divider = (float) (kPadding + kKnobWidth + 8);
    theme::paintDivider (g, divider, (float) kPadding + kTitleHeight, area.getHeight() - 2.0f * kPadding - kTitleHeight);

    g.setColour (theme::primary().withAlpha (0.8f));
    g.setFont (fonts::title (14.0f));
    g.drawText (juce::String::fromUTF8 ("CHAOS  \xc2\xb7  PER NOTE"), juce::Rectangle<float> ((float) kPadding, 6.0f, 200.0f, 14.0f), juce::Justification::centredLeft);
}

void ChaosView::resized()
{
    auto area = getLocalBounds().reduced (kPadding);
    area.removeFromTop (kTitleHeight);
    amount.setBounds (area.removeFromLeft (kKnobWidth));
    area.removeFromLeft (16);
    for (auto* knob : { &pitch, &decay, &tone, &pan, &velocity })
        knob->setBounds (area.removeFromLeft (kKnobWidth));
}

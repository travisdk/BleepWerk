// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#include "ChaosView.h"
#include "Theme.h"

namespace
{
constexpr int kPadding = 10, kTitleHeight = 12, kKnobWidth = 62;
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
        addAndMakeVisible (knob);
}

void ChaosView::paint (juce::Graphics& g)
{
    using namespace bleepforge;
    const auto area = getLocalBounds().toFloat();
    g.setColour (theme::panel);
    g.fillRoundedRectangle (area, 6.0f);
    g.setColour (theme::outline);
    g.drawRoundedRectangle (area.reduced (0.5f), 6.0f, 1.0f);

    // Amount stands apart: it scales the others
    const float divider = (float) (kPadding + kKnobWidth + 8);
    g.fillRect (juce::Rectangle<float> (divider, (float) kPadding + kTitleHeight, 1.0f, area.getHeight() - 2.0f * kPadding - kTitleHeight));

    g.setColour (theme::accent.withAlpha (0.8f));
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("CHAOS", juce::Rectangle<float> ((float) kPadding, 4.0f, 120.0f, 14.0f), juce::Justification::centredLeft);
}

void ChaosView::resized()
{
    auto area = getLocalBounds().reduced (kPadding);
    area.removeFromTop (kTitleHeight);
    amount.setBounds (area.removeFromLeft (kKnobWidth));
    area.removeFromLeft (16);
    for (auto* knob : { &pitch, &decay, &tone, &pan, &velocity })
        knob->setBounds (area.removeFromLeft (kKnobWidth + 4));
}

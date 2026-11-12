// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#include "EffectsView.h"
#include "Theme.h"

namespace
{
constexpr int kPadding = 10, kTitleHeight = 12, kChoiceWidth = 72, kFeelWidth = 36, kGroupGap = 11;
} // namespace

EffectsView::EffectsView (BleepForgeProcessor& p)
    : processor (p),
      filterType (p.apvts, "filterType", "Type"),
      cutoff (p.apvts, "filterCutoff", "Cutoff"),
      resonance (p.apvts, "filterResonance", "Reso"),
      bits (p.apvts, "crushBits", "Bits"),
      rate (p.apvts, "crushRate", "Rate"),
      delayTime (p.apvts, "delayNote", "Time"),
      feedback (p.apvts, "delayFeedback", "Feedback"),
      delayMix (p.apvts, "delayMix", "Mix"),
      roomSize (p.apvts, "roomSize", "Size"),
      roomDecay (p.apvts, "roomDecay", "Decay"),
      roomTone (p.apvts, "roomTone", "Tone"),
      roomMix (p.apvts, "roomMix", "Mix"),
      roomDuck (p.apvts, "roomDuck", "Duck"),
      delayFeel (p.apvts, "delayFeel", "", true, { "Str", "Tri", "Dot" })
{
    for (auto* c : std::initializer_list<juce::Component*> { &filterType, &cutoff, &resonance, &bits, &rate, &delayTime,
                                                             &delayFeel, &feedback, &delayMix, &roomSize, &roomDecay, &roomTone, &roomMix, &roomDuck })
        addAndMakeVisible (c);

    timerCallback();
    startTimerHz (10);
}

EffectsView::~EffectsView()
{
    stopTimer();
}

// Follows the filter type and the delay feel, which automation or the host can change too.
void EffectsView::timerCallback()
{
    delayFeel.update();
    const bool filterOn = (int) processor.apvts.getRawParameterValue ("filterType")->load() != (int) bleepforge::FilterType::Off;
    cutoff.setActive (filterOn);
    resonance.setActive (filterOn);
}

void EffectsView::paint (juce::Graphics& g)
{
    using namespace bleepforge;
    const auto area = getLocalBounds().toFloat();
    theme::paintPanel (g, area);

    g.setFont (fonts::title (14.0f));
    for (size_t i = 0; i < groups.size(); ++i)
    {
        const auto& group = groups[i];
        if (i > 0)
        {
            const float x = (float) group.area.getX() - (float) kGroupGap * 0.5f;
            theme::paintDivider (g, x, (float) kPadding + kTitleHeight, area.getHeight() - 2.0f * kPadding - kTitleHeight);
        }
        g.setColour (theme::primary().withAlpha (0.8f));
        g.drawText (group.name, juce::Rectangle<float> ((float) group.area.getX(), 6.0f, 100.0f, 14.0f), juce::Justification::centredLeft);
    }
}

// One row across the whole width: the knobs share what the choice, the feel buttons and the gaps
// leave, all the same size, so their names, knobs and values stand on the same lines.
void EffectsView::resized()
{
    auto area = getLocalBounds().reduced (kPadding);
    area.removeFromTop (kTitleHeight);

    constexpr int numKnobs = 12, feelSpace = kFeelWidth + 6;
    const int knobWidth = (area.getWidth() - kChoiceWidth - feelSpace - 3 * kGroupGap) / numKnobs;
    const int diameter = juce::jmin (knobWidth, area.getHeight() - Knob::kLabelHeight - Knob::kValueHeight);
    for (auto* knob : { &cutoff, &resonance, &bits, &rate, &delayTime, &feedback, &delayMix, &roomSize, &roomDecay, &roomTone, &roomMix, &roomDuck })
        knob->setMaxDiameter (diameter);
    area = area.removeFromTop (Knob::heightFor (diameter));

    auto place = [&area] (juce::Component& c, int width) { c.setBounds (area.removeFromLeft (width)); };
    auto group = [this, &area] (size_t index, auto&& layOut)
    {
        const int start = area.getX();
        layOut();
        groups[index].area = juce::Rectangle<int> (start, 0, area.getX() - start, getHeight());
        area.removeFromLeft (kGroupGap);
    };
    // the choice and the buttons stand beside the knobs' circles, not their values
    auto beside = [&area, this] (juce::Component& c, int width) { juce::ignoreUnused (this); c.setBounds (area.removeFromLeft (width).withTrimmedBottom (Knob::kValueHeight)); };

    group (0, [&] { beside (filterType, kChoiceWidth); place (cutoff, knobWidth); place (resonance, knobWidth); });
    group (1, [&] { place (bits, knobWidth); place (rate, knobWidth); });
    group (2, [&] { place (delayTime, knobWidth); beside (delayFeel, kFeelWidth); area.removeFromLeft (6); place (feedback, knobWidth); place (delayMix, knobWidth); });
    group (3, [&]
           {
               for (auto* knob : { &roomSize, &roomDecay, &roomTone, &roomMix, &roomDuck })
                   place (*knob, knobWidth);
           });
}

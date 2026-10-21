// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#include "EffectsView.h"
#include "Theme.h"

namespace
{
constexpr int kPadding = 10, kTitleHeight = 12, kKnobWidth = 50, kChoiceWidth = 58, kFeelWidth = 36, kGroupGap = 13;
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
    cutoff.setEnabled (filterOn);
    resonance.setEnabled (filterOn);
}

void EffectsView::paint (juce::Graphics& g)
{
    using namespace bleepforge;
    const auto area = getLocalBounds().toFloat();
    g.setColour (theme::panel);
    g.fillRect (area);
    g.setColour (theme::outline);
    g.drawRect (area, 1.0f);

    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    for (size_t i = 0; i < groups.size(); ++i)
    {
        const auto& group = groups[i];
        if (i > 0)
        {
            g.setColour (theme::outline);
            const float x = (float) group.area.getX() - (float) kGroupGap * 0.5f;
            g.fillRect (juce::Rectangle<float> (x, (float) kPadding + kTitleHeight, 1.0f, area.getHeight() - 2.0f * kPadding - kTitleHeight));
        }
        g.setColour (theme::primary().withAlpha (0.8f));
        g.drawText (group.name, juce::Rectangle<float> ((float) group.area.getX(), 4.0f, 100.0f, 14.0f), juce::Justification::centredLeft);
    }
}

void EffectsView::resized()
{
    auto area = getLocalBounds().reduced (kPadding);
    area.removeFromTop (kTitleHeight);

    auto place = [&area] (juce::Component& c, int width) { c.setBounds (area.removeFromLeft (width)); };
    auto group = [this, &area] (size_t index, auto&& layOut)
    {
        const int start = area.getX();
        layOut();
        groups[index].area = juce::Rectangle<int> (start, 0, area.getX() - start, getHeight());
        area.removeFromLeft (kGroupGap);
    };

    group (0, [&] { filterType.setBounds (area.removeFromLeft (kChoiceWidth).withTrimmedTop (10)); place (cutoff, kKnobWidth); place (resonance, kKnobWidth); });
    group (1, [&] { place (bits, kKnobWidth); place (rate, kKnobWidth); });
    group (2, [&] { place (delayTime, kKnobWidth); place (delayFeel, kFeelWidth); area.removeFromLeft (6); place (feedback, kKnobWidth); place (delayMix, kKnobWidth); });
    group (3, [&]
           {
               for (auto* knob : { &roomSize, &roomDecay, &roomTone, &roomMix, &roomDuck })
                   place (*knob, kKnobWidth);
           });
}

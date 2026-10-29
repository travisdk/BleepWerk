// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#include "EffectsView.h"
#include "Theme.h"

namespace
{
constexpr int kPadding = 10, kTitleHeight = 12, kKnobWidth = 48, kChoiceWidth = 72, kFeelWidth = 42, kGroupGap = 11,
              kRowHeight = 106; // a title and a row of knobs; two rows make up the panel
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
      driveType (p.apvts, "driveType", "Type"),
      driveAmount (p.apvts, "driveAmount", "Drive"),
      driveMix (p.apvts, "driveMix", "Mix"),
      delayFeel (p.apvts, "delayFeel", "", true, { "Str", "Tri", "Dot" }),
      drivePost (p.apvts, "drivePost", "", true, { "Pre", "Post" })
{
    for (auto* c : std::initializer_list<juce::Component*> { &filterType, &cutoff, &resonance, &bits, &rate, &delayTime,
                                                             &delayFeel, &feedback, &delayMix, &roomSize, &roomDecay, &roomTone, &roomMix, &roomDuck,
                                                             &driveType, &drivePost, &driveAmount, &driveMix })
        addAndMakeVisible (c);

    timerCallback();
    startTimerHz (10);
}

EffectsView::~EffectsView()
{
    stopTimer();
}

// Follows the filter type, the delay feel and the drive place, which automation or the host can change too.
void EffectsView::timerCallback()
{
    delayFeel.update();
    drivePost.update();
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

    // a line between the two rows
    g.fillRect (juce::Rectangle<float> ((float) kPadding, (float) (kPadding + kRowHeight - 8), area.getWidth() - 2.0f * kPadding, 1.0f));

    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    for (size_t i = 0; i < groups.size(); ++i)
    {
        const auto& group = groups[i];
        if (i > 0 && groups[i - 1].area.getY() == group.area.getY())
        {
            g.setColour (theme::outline);
            const float x = (float) group.area.getX() - (float) kGroupGap * 0.5f;
            g.fillRect (juce::Rectangle<float> (x, (float) group.area.getY() + kTitleHeight, 1.0f, (float) kRowHeight - kTitleHeight - 12.0f));
        }
        g.setColour (theme::primary().withAlpha (0.8f));
        g.drawText (group.name, juce::Rectangle<float> ((float) group.area.getX(), (float) group.area.getY() - 6.0f, 100.0f, 14.0f), juce::Justification::centredLeft);
    }
}

// Two rows: the chain in order on the first, Drive on the second, since it sits first or last.
void EffectsView::resized()
{
    const auto inner = getLocalBounds().reduced (kPadding);
    juce::Rectangle<int> area;
    int rowTop = 0;

    auto startRow = [&] (int row)
    {
        rowTop = inner.getY() + row * kRowHeight;
        area = juce::Rectangle<int> (inner.getX(), rowTop + kTitleHeight, inner.getWidth(), kRowHeight - kTitleHeight - 6);
    };
    auto place = [&area] (juce::Component& c, int width) { c.setBounds (area.removeFromLeft (width)); };
    auto group = [&] (size_t index, auto&& layOut)
    {
        const int start = area.getX();
        layOut();
        groups[index].area = juce::Rectangle<int> (start, rowTop, area.getX() - start, kRowHeight);
        area.removeFromLeft (kGroupGap);
    };

    startRow (0);
    group (0, [&] { filterType.setBounds (area.removeFromLeft (kChoiceWidth).withTrimmedTop (10)); place (cutoff, kKnobWidth); place (resonance, kKnobWidth); });
    group (1, [&] { place (bits, kKnobWidth); place (rate, kKnobWidth); });
    group (2, [&] { place (delayTime, kKnobWidth); place (delayFeel, kFeelWidth); area.removeFromLeft (6); place (feedback, kKnobWidth); place (delayMix, kKnobWidth); });
    group (3, [&]
           {
               for (auto* knob : { &roomSize, &roomDecay, &roomTone, &roomMix, &roomDuck })
                   place (*knob, kKnobWidth);
           });

    startRow (1);
    group (4, [&]
           {
               driveType.setBounds (area.removeFromLeft (kChoiceWidth).withTrimmedTop (10));
               area.removeFromLeft (4);
               place (drivePost, kFeelWidth);
               area.removeFromLeft (4);
               place (driveAmount, kKnobWidth);
               place (driveMix, kKnobWidth);
           });
}

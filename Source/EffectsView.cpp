// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#include "EffectsView.h"
#include "Theme.h"

namespace
{
constexpr int kPadding = 10, kTitleHeight = 12, kKnobWidth = 48, kChoiceWidth = 72, kFeelWidth = 36, kPlaceWidth = 88, kGroupGap = 11,
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
      drivePost (p.apvts, "drivePost", "", true, { "Pre filter", "Post reverb" })
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

// Follows the filter type, the delay feel and the drive place, which automation or the host can
// change too. Drive moves in the layout with its place.
void EffectsView::timerCallback()
{
    delayFeel.update();
    drivePost.update();
    const bool filterOn = (int) processor.apvts.getRawParameterValue ("filterType")->load() != (int) bleepforge::FilterType::Off;
    cutoff.setEnabled (filterOn);
    resonance.setEnabled (filterOn);
    if (const bool post = processor.apvts.getRawParameterValue ("drivePost")->load() > 0.5f; post != drivePlacedPost)
    {
        resized();
        repaint();
    }
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
        if (group.divider)
        {
            g.setColour (theme::outline);
            const float x = (float) group.area.getX() - (float) kGroupGap * 0.5f;
            g.fillRect (juce::Rectangle<float> (x, (float) group.area.getY() + kTitleHeight, 1.0f, (float) kRowHeight - kTitleHeight - 12.0f));
        }
        g.setColour (theme::primary().withAlpha (0.8f));
        g.drawText (group.name, juce::Rectangle<float> ((float) group.area.getX(), (float) group.area.getY() - 6.0f, 100.0f, 14.0f), juce::Justification::centredLeft);
    }
}

// The chain reads like text, left to right and then on to the next row, in the order the sound
// goes through it. Drive moves: first when it is Pre, last when it is Post.
void EffectsView::resized()
{
    const auto inner = getLocalBounds().reduced (kPadding);
    juce::Rectangle<int> area;
    int rowTop = 0;
    drivePlacedPost = processor.apvts.getRawParameterValue ("drivePost")->load() > 0.5f;

    auto place = [&area] (juce::Component& c, int width) { c.setBounds (area.removeFromLeft (width)); };
    auto layOut = [&] (size_t index)
    {
        switch (index)
        {
            case 0: filterType.setBounds (area.removeFromLeft (kChoiceWidth).withTrimmedTop (10)); place (cutoff, kKnobWidth); place (resonance, kKnobWidth); break;
            case 1: place (bits, kKnobWidth); place (rate, kKnobWidth); break;
            case 2: place (delayTime, kKnobWidth); place (delayFeel, kFeelWidth); area.removeFromLeft (6); place (feedback, kKnobWidth); place (delayMix, kKnobWidth); break;
            case 3:
                for (auto* knob : { &roomSize, &roomDecay, &roomTone, &roomMix, &roomDuck })
                    place (*knob, kKnobWidth);
                break;
            default:
                driveType.setBounds (area.removeFromLeft (kChoiceWidth).withTrimmedTop (10));
                area.removeFromLeft (4);
                place (drivePost, kPlaceWidth);
                area.removeFromLeft (4);
                place (driveAmount, kKnobWidth);
                place (driveMix, kKnobWidth);
                break;
        }
    };

    // indices into groups: 0 filter, 1 crush, 2 delay, 3 reverb, 4 drive
    const std::vector<std::vector<size_t>> rows = drivePlacedPost ? std::vector<std::vector<size_t>> { { 0, 1, 2 }, { 3, 4 } }
                                                                  : std::vector<std::vector<size_t>> { { 4, 0, 1, 2 }, { 3 } };
    for (size_t row = 0; row < rows.size(); ++row)
    {
        rowTop = inner.getY() + (int) row * kRowHeight;
        area = juce::Rectangle<int> (inner.getX(), rowTop + kTitleHeight, inner.getWidth(), kRowHeight - kTitleHeight - 6);
        for (size_t k = 0; k < rows[row].size(); ++k)
        {
            auto& g = groups[rows[row][k]];
            const int start = area.getX();
            layOut (rows[row][k]);
            g.area = juce::Rectangle<int> (start, rowTop, area.getX() - start, kRowHeight);
            g.divider = k > 0;
            area.removeFromLeft (kGroupGap);
        }
    }
}

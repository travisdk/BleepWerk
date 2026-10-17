// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#include "EngineView.h"
#include "Theme.h"

namespace
{
constexpr int kPadding = 10, kLeftWidth = 250, kKnobWidth = 62, kChoiceWidth = 104;

void styleName (juce::Label& label, const juce::String& name)
{
    label.setText (name, juce::dontSendNotification);
    label.setFont (juce::FontOptions (12.0f));
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, bleepforge::theme::dimText);
    label.setInterceptsMouseClicks (false, false);
}
} // namespace

EngineView::Knob::Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& parameterId, const juce::String& name, bool bipolar)
    : attachment (apvts, parameterId, slider)
{
    styleName (label, name);
    addAndMakeVisible (label);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, kKnobWidth, 16);
    slider.getProperties().set ("bipolar", bipolar);
    if (auto* parameter = apvts.getParameter (parameterId))
        slider.setDoubleClickReturnValue (true, parameter->convertFrom0to1 (parameter->getDefaultValue()));
    addAndMakeVisible (slider);
}

void EngineView::Knob::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (16));
    slider.setBounds (area);
}

EngineView::Choice::Choice (juce::AudioProcessorValueTreeState& apvts, const juce::String& parameterId, const juce::String& name)
    : attachment (apvts, parameterId, box)
{
    // the attachment fills nothing in: the items come from the choice parameter
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (parameterId)))
    {
        box.addItemList (choice->choices, 1);
        box.setSelectedItemIndex (choice->getIndex(), juce::dontSendNotification);
    }
    styleName (label, name);
    addAndMakeVisible (label);
    addAndMakeVisible (box);
}

void EngineView::Choice::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (16));
    box.setBounds (area.removeFromTop (28).reduced (2, 0));
}

EngineView::Toggle::Toggle (juce::AudioProcessorValueTreeState& apvts, const juce::String& parameterId, const juce::String& name)
    : parameter (*apvts.getParameter (parameterId))
{
    styleName (label, name);
    label.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (label);

    const auto names = parameter.getAllValueStrings();
    left.setButtonText (names[0]);
    right.setButtonText (names[1]);
    left.setConnectedEdges (juce::Button::ConnectedOnRight);
    right.setConnectedEdges (juce::Button::ConnectedOnLeft);
    for (auto* button : { &left, &right })
    {
        button->setColour (juce::TextButton::textColourOnId, bleepforge::theme::accent);
        addAndMakeVisible (*button);
    }
    auto choose = [this] (float value)
    {
        parameter.beginChangeGesture();
        parameter.setValueNotifyingHost (value);
        parameter.endChangeGesture();
        update();
    };
    left.onClick = [choose] { choose (0.0f); };
    right.onClick = [choose] { choose (1.0f); };
    update();
}

void EngineView::Toggle::update()
{
    const bool second = parameter.getValue() >= 0.5f;
    left.setToggleState (! second, juce::dontSendNotification);
    right.setToggleState (second, juce::dontSendNotification);
}

void EngineView::Toggle::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (16));
    area = area.removeFromTop (26);
    left.setBounds (area.removeFromLeft (area.getWidth() / 2));
    right.setBounds (area);
}

EngineView::EngineView (BleepForgeProcessor& p)
    : processor (p),
      engine (p.apvts, "engine", "Engine"),
      voices (p.apvts, "voiceMode", "Voices"),
      glide (p.apvts, "glide", "Glide"),
      output (p.apvts, "outputGain", "Output"),
      bleepWave (p.apvts, "bleepWave", "Wave"),
      pulseWidth (p.apvts, "bleepPulseWidth", "Pulse W"),
      fmRatio (p.apvts, "bleepFmRatio", "FM Ratio"),
      fmAmount (p.apvts, "bleepFmAmount", "FM Amt"),
      pitchAmount (p.apvts, "bleepPitchAmount", "Pitch Env", true),
      pitchDecay (p.apvts, "bleepPitchDecay", "P Decay"),
      attack (p.apvts, "bleepAttack", "Attack"),
      decay (p.apvts, "bleepDecay", "Decay"),
      release (p.apvts, "bleepRelease", "Release"),
      noise (p.apvts, "bleepNoise", "Noise"),
      foleyExciter (p.apvts, "foleyExciter", "Exciter"),
      foleyLength (p.apvts, "foleyLength", "Length"),
      material (p.apvts, "foleyMaterial", "Material"),
      size (p.apvts, "foleySize", "Size"),
      damping (p.apvts, "foleyDamping", "Damping"),
      brightness (p.apvts, "foleyBrightness", "Bright"),
      sweep (p.apvts, "foleySweep", "Sweep", true)
{
    for (auto* c : std::initializer_list<juce::Component*> { &engine, &voices, &glide, &output })
        addAndMakeVisible (c);

    bleepControls = { &bleepWave, &pulseWidth, &fmRatio, &fmAmount, &pitchAmount, &pitchDecay, &attack, &decay, &release, &noise };
    foleyControls = { &foleyExciter, &foleyLength, &material, &size, &damping, &brightness, &sweep };
    for (auto* c : bleepControls)
        addChildComponent (c);
    for (auto* c : foleyControls)
        addChildComponent (c);

    timerCallback();
    startTimerHz (10);
}

EngineView::~EngineView()
{
    stopTimer();
}

// Follows the engine and voice parameters, which automation or the host can change too.
void EngineView::timerCallback()
{
    engine.update();
    voices.update();
    const int selected = (int) processor.apvts.getRawParameterValue ("engine")->load();
    if (selected != shownEngine)
    {
        shownEngine = selected;
        showEngine (selected == 1);
    }
}

void EngineView::showEngine (bool foley)
{
    for (auto* c : bleepControls)
        c->setVisible (! foley);
    for (auto* c : foleyControls)
        c->setVisible (foley);
    repaint();
}

void EngineView::paint (juce::Graphics& g)
{
    using namespace bleepforge;
    const auto area = getLocalBounds().toFloat();
    g.setColour (theme::panel);
    g.fillRoundedRectangle (area, 6.0f);
    g.setColour (theme::outline);
    g.drawRoundedRectangle (area.reduced (0.5f), 6.0f, 1.0f);

    // divider between the shared controls and the engine's own
    g.fillRect (juce::Rectangle<float> ((float) kLeftWidth, (float) kPadding, 1.0f, area.getHeight() - 2.0f * kPadding));

    g.setColour (theme::accent.withAlpha (0.8f));
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (shownEngine == 1 ? "FOLEY" : "BLEEP",
                juce::Rectangle<float> ((float) kLeftWidth + kPadding, 4.0f, 120.0f, 14.0f), juce::Justification::centredLeft);
}

void EngineView::resized()
{
    auto area = getLocalBounds().reduced (kPadding);

    // shared: two toggles stacked, then Glide and Output
    auto left = area.removeFromLeft (kLeftWidth - kPadding * 2);
    auto toggles = left.removeFromLeft (left.getWidth() - 2 * kKnobWidth - 6);
    engine.setBounds (toggles.removeFromTop (toggles.getHeight() / 2).reduced (0, 4));
    voices.setBounds (toggles.reduced (0, 4));
    left.removeFromLeft (6);
    glide.setBounds (left.removeFromLeft (kKnobWidth));
    output.setBounds (left.removeFromLeft (kKnobWidth));

    // the engine's own: its choice first, then the knobs
    area.removeFromLeft (kPadding * 2);
    area.removeFromTop (12); // the engine's name
    auto lay = [area] (const std::vector<juce::Component*>& controls) mutable
    {
        auto row = area;
        controls.front()->setBounds (row.removeFromLeft (kChoiceWidth).withTrimmedTop (10));
        row.removeFromLeft (8);
        const int width = juce::jmin (kKnobWidth + 8, row.getWidth() / ((int) controls.size() - 1));
        for (size_t i = 1; i < controls.size(); ++i)
            controls[i]->setBounds (row.removeFromLeft (width));
    };
    lay (bleepControls);
    lay (foleyControls);
}

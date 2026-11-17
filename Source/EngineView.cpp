// Copyright (C) 2026 Henrik Juhl. Part of BleepWerk, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#include "EngineView.h"
#include "Theme.h"
#include "PresetFiles.h"
#include "Randomizer.h"

namespace
{
constexpr int kPadding = 10, kLeftWidth = 280, kKnobWidth = 50, kDiameter = 47, kKeyDiameter = 62;
constexpr int kSelectorTop = 27, kSelectorHeight = 48; // Werk's row of buttons
} // namespace

EngineView::EngineView (BleepWerkProcessor& p)
    : processor (p),
      engine (p.apvts, "engine", "Engine"),
      voices (p.apvts, "voiceMode", "Voices"),
      glide (p.apvts, "glide", "Glide"),
      bend (p.apvts, "bendRange", "Bend"),
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
      exciter (p.apvts, "strikeExciter", "Play"),
      werkLength (p.apvts, "strikeLength", "Length"),
      ring (p.apvts, "strikeRing", "Ring"),
      werkTone (p.apvts, "strikeTone", "Tone"),
      motion (p.apvts, "strikeMotion", "Motion"),
      sweep (p.apvts, "strikeSweep", "Sweep", true)
{
    for (auto* c : std::initializer_list<juce::Component*> { &engine, &voices, &glide, &bend, &output, &dice, &back })
        addAndMakeVisible (c);
    engine.buttonHeight = 40; // the main choice, so larger buttons
    dice.onClick = [this] { rollSound(); };
    back.onClick = [this] { undoRoll(); };
    back.setActive (false);

    // the ones that matter most for each sound, drawn larger: pitch envelope and decay for the
    // bleep, ring and tone for Werk
    bleepKeys = { 4, 7 };
    werkKeys = { 2, 3 };
    bleepControls = { &bleepWave, &pulseWidth, &fmRatio, &fmAmount, &pitchAmount, &pitchDecay, &attack, &decay, &release, &noise };
    werkControls = { &exciter, &werkLength, &ring, &werkTone, &motion, &sweep };
    for (auto* list : { &bleepControls, &werkControls })
        for (size_t i = 0; i < list->size(); ++i)
        {
            auto* c = (*list)[i];
            addChildComponent (c);
            if (auto* knob = dynamic_cast<Knob*> (c))
                knob->setMaxDiameter (isKey (list, i) ? kKeyDiameter : kDiameter);
        }
    for (auto* knob : { &glide, &bend, &output })
        knob->setMaxDiameter (kDiameter);

    timerCallback();
    startTimerHz (10);
}

EngineView::~EngineView()
{
    stopTimer();
}

// Follows the engine, voice and wave parameters, which automation or the host can change too.
void EngineView::timerCallback()
{
    engine.update();
    voices.update();
    exciter.update();

    // Length only shapes the sources that last or burst, and Motion those that move
    const auto werkExciter = (bleepwerk::WerkExciter) (int) processor.apvts.getRawParameterValue ("strikeExciter")->load();
    werkLength.setActive (werkExciter != bleepwerk::WerkExciter::Knock);
    motion.setActive (werkExciter != bleepwerk::WerkExciter::Brush);

    // Pulse W and the FM knobs only shape their own wave; for the others they do nothing
    const auto wave = (bleepwerk::BleepWave) (int) processor.apvts.getRawParameterValue ("bleepWave")->load();
    pulseWidth.setActive (wave == bleepwerk::BleepWave::Pulse);
    fmRatio.setActive (wave == bleepwerk::BleepWave::FM);
    fmAmount.setActive (wave == bleepwerk::BleepWave::FM);
    const int selected = (int) processor.apvts.getRawParameterValue ("engine")->load();
    if (selected != shownEngine)
    {
        shownEngine = selected;
        showEngine (selected);
    }
}

bool EngineView::isKey (const std::vector<juce::Component*>* list, size_t index) const
{
    const auto& keys = list == &bleepControls ? bleepKeys : werkKeys;
    return std::find (keys.begin(), keys.end(), index) != keys.end();
}

void EngineView::showEngine (int engineIndex)
{
    for (auto* c : bleepControls)
        c->setVisible (engineIndex == (int) bleepwerk::Engine::Bleep);
    for (auto* c : werkControls)
        c->setVisible (engineIndex == (int) bleepwerk::Engine::Werk);
    repaint();
}

// Keeps the engine and the other engine's settings; the output gain is matched like a factory
// preset's. The sound then shows as "Random Bleep" or "Random Werk", ready
// to save.
void EngineView::rollSound()
{
    using namespace bleepwerk;
    auto& apvts = processor.apvts;
    auto& presets = processor.getSoundPresets();
    auto value = [&apvts] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    SoundBefore before { presets.current(), presets.name(), {} };
    for (const auto& p : kSoundParams)
        before.values.push_back (value (p.id));

    const int engineIndex = juce::jlimit (0, kNumEngines - 1, juce::roundToInt (value ("engine")));
    DiceRng rng ((uint64_t) juce::Random::getSystemRandom().nextInt64());
    for (const auto& v : randomMatchedSound ((Engine) engineIndex, rng, value))
        if (auto* param = apvts.getParameter (v.id))
            files::setParameterValue (*param, v.value);
    presets.setCurrent (-1, "Random " + juce::String (kEngineNames[engineIndex]));

    beforeRoll = std::move (before);
    back.setActive (true);
}

// One step back only: the arrow is spent once used.
void EngineView::undoRoll()
{
    using namespace bleepwerk;
    if (! beforeRoll.has_value())
        return;
    for (size_t i = 0; i < std::size (kSoundParams); ++i)
        if (auto* param = processor.apvts.getParameter (kSoundParams[i].id))
            files::setParameterValue (*param, beforeRoll->values[i]);
    processor.getSoundPresets().setCurrent (beforeRoll->entry, beforeRoll->name);
    beforeRoll.reset();
    back.setActive (false);
}

void EngineView::paint (juce::Graphics& g)
{
    using namespace bleepwerk;
    const auto area = getLocalBounds().toFloat();
    theme::paintPanel (g, area);

    // divider between the shared controls and the engine's own
    theme::paintDivider (g, (float) kLeftWidth, (float) kPadding + 4.0f, area.getHeight() - 2.0f * kPadding - 4.0f);

    g.setColour (theme::primary().withAlpha (0.8f));
    g.setFont (fonts::title (14.0f));
    g.drawText (juce::String (kEngineNames[juce::jlimit (0, kNumEngines - 1, shownEngine)]).toUpperCase(),
                juce::Rectangle<float> ((float) kLeftWidth + kPadding, 6.0f, 120.0f, 14.0f), juce::Justification::centredLeft);
}

void EngineView::resized()
{
    auto area = getLocalBounds().reduced (kPadding);
    const int knobHeight = Knob::heightFor (kDiameter);

    // shared: the engine across the full width, so all three names fit, then Voices, Glide,
    // Bend and Output below it, on the same lines as the engine's own knobs
    auto left = area.removeFromLeft (kLeftWidth - kPadding * 2);
    engine.setBounds (left.removeFromTop (Knob::kLabelHeight + 40));
    auto leftRow = left.removeFromBottom (knobHeight);
    voices.setBounds (leftRow.removeFromLeft (leftRow.getWidth() - 3 * kKnobWidth - 6).withTrimmedBottom (Knob::kValueHeight));
    leftRow.removeFromLeft (6);
    glide.setBounds (leftRow.removeFromLeft (kKnobWidth));
    bend.setBounds (leftRow.removeFromLeft (kKnobWidth));
    output.setBounds (leftRow.removeFromLeft (kKnobWidth));

    // the engine's own: its choice first, then the knobs
    area.removeFromLeft (kPadding * 2);
    // the engine's name, with the dice and back arrow at the right
    auto nameRow = juce::Rectangle<int> (area.getX(), 5, area.getWidth(), 18);
    dice.setBounds (nameRow.removeFromRight (18));
    nameRow.removeFromRight (4);
    back.setBounds (nameRow.removeFromRight (18));
    // Bleep in two rows spread over the width, like Werk: the source on top (wave, its pulse
    // width and FM, noise) and the envelopes below, where the key knobs, pitch envelope and
    // decay, are larger
    const int sourceWidth = area.getWidth() / 5;
    auto sourceRow = juce::Rectangle<int> (area.getX(), kSelectorTop, area.getWidth(), Knob::heightFor (kDiameter));
    bleepWave.setBounds (sourceRow.removeFromLeft (sourceWidth).reduced (4, 0).withHeight (Knob::kLabelHeight + kDiameter));
    for (auto* knob : { &pulseWidth, &fmRatio, &fmAmount, &noise })
        knob->setBounds (sourceRow.removeFromLeft (sourceWidth).withSizeKeepingCentre (kKnobWidth + 14, Knob::heightFor (kDiameter)));
    auto envelopeRow = area.withTrimmedTop (area.getHeight() - Knob::heightFor (kKeyDiameter));
    for (auto* knob : { &pitchAmount, &pitchDecay, &attack, &decay, &release })
    {
        const bool key = knob == &pitchAmount || knob == &decay;
        auto slot = envelopeRow.removeFromLeft (sourceWidth);
        knob->setBounds (slot.withSizeKeepingCentre (key ? kKeyDiameter + 4 : kKnobWidth + 14, slot.getHeight())
                             .removeFromBottom (Knob::heightFor (key ? kKeyDiameter : kDiameter)));
    }

    // Werk: the Play row across the width, and the five knobs spread under it
    exciter.setBounds (juce::Rectangle<int> (area.getX(), kSelectorTop, area.getWidth(), kSelectorHeight));
    auto knobRow = area.withTrimmedTop (area.getHeight() - Knob::heightFor (kKeyDiameter));
    const int slotWidth = knobRow.getWidth() / 5;
    for (size_t i = 1; i < werkControls.size(); ++i)
    {
        const bool key = isKey (&werkControls, i);
        auto slot = knobRow.removeFromLeft (slotWidth);
        werkControls[i]->setBounds (slot.withSizeKeepingCentre (key ? kKeyDiameter + 4 : kKnobWidth + 14, slot.getHeight())
                                          .removeFromBottom (Knob::heightFor (key ? kKeyDiameter : kDiameter)));
    }
}

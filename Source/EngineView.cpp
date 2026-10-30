// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#include "EngineView.h"
#include "Theme.h"
#include "PresetFiles.h"
#include "Randomizer.h"

namespace
{
constexpr int kPadding = 10, kLeftWidth = 280, kKnobWidth = 50, kChoiceWidth = 90, kDiameter = 47, kKeyDiameter = 62;
} // namespace

EngineView::EngineView (BleepForgeProcessor& p)
    : processor (p),
      engine (p.apvts, "engine", "Engine"),
      voices (p.apvts, "voiceMode", "Voices"),
      glide (p.apvts, "glide", "Glide"),
      bend (p.apvts, "bendRange", "Bend"),
      output (p.apvts, "outputGain", "Output"),
      picture (p),
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
      sweep (p.apvts, "foleySweep", "Sweep", true),
      machineType (p.apvts, "machineType", "Type"),
      machineLength (p.apvts, "machineLength", "Length"),
      machineTone (p.apvts, "machineTone", "Tone"),
      motion (p.apvts, "machineMotion", "Motion"),
      focus (p.apvts, "machineFocus", "Focus"),
      metal (p.apvts, "machineMetal", "Metal")
{
    for (auto* c : std::initializer_list<juce::Component*> { &engine, &voices, &glide, &bend, &output, &dice, &back, &picture })
        addAndMakeVisible (c);
    engine.buttonHeight = 40; // the main choice, so larger buttons
    dice.onClick = [this] { rollSound(); };
    back.onClick = [this] { undoRoll(); };
    back.setEnabled (false);

    // the ones that matter most for each sound, drawn larger: pitch envelope and decay for the
    // bleep, material and damping for the foley, length and metal for the machine
    bleepKeys = { 4, 7 };
    foleyKeys = { 2, 4 };
    machineKeys = { 1, 5 };
    bleepControls = { &bleepWave, &pulseWidth, &fmRatio, &fmAmount, &pitchAmount, &pitchDecay, &attack, &decay, &release, &noise };
    foleyControls = { &foleyExciter, &foleyLength, &material, &size, &damping, &brightness, &sweep };
    machineControls = { &machineType, &machineLength, &machineTone, &motion, &focus, &metal };
    for (auto* list : { &bleepControls, &foleyControls, &machineControls })
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

    // Pulse W and the FM knobs only shape their own wave; for the others they do nothing
    const auto wave = (bleepforge::BleepWave) (int) processor.apvts.getRawParameterValue ("bleepWave")->load();
    pulseWidth.setEnabled (wave == bleepforge::BleepWave::Pulse);
    fmRatio.setEnabled (wave == bleepforge::BleepWave::FM);
    fmAmount.setEnabled (wave == bleepforge::BleepWave::FM);
    const int selected = (int) processor.apvts.getRawParameterValue ("engine")->load();
    if (selected != shownEngine)
    {
        shownEngine = selected;
        showEngine (selected);
    }
}

bool EngineView::isKey (const std::vector<juce::Component*>* list, size_t index) const
{
    const auto& keys = list == &bleepControls ? bleepKeys : list == &foleyControls ? foleyKeys : machineKeys;
    return std::find (keys.begin(), keys.end(), index) != keys.end();
}

void EngineView::showEngine (int engineIndex)
{
    for (auto* c : bleepControls)
        c->setVisible (engineIndex == (int) bleepforge::Engine::Bleep);
    for (auto* c : foleyControls)
        c->setVisible (engineIndex == (int) bleepforge::Engine::Foley);
    for (auto* c : machineControls)
        c->setVisible (engineIndex == (int) bleepforge::Engine::Machine);
    repaint();
}

// Keeps the engine and the other engine's settings; the output gain is matched like a factory
// preset's. The sound then shows as "Random Bleep", "Random Foley" or "Random Machine", ready
// to save.
void EngineView::rollSound()
{
    using namespace bleepforge;
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
    back.setEnabled (true);
}

// One step back only: the arrow is spent once used.
void EngineView::undoRoll()
{
    using namespace bleepforge;
    if (! beforeRoll.has_value())
        return;
    for (size_t i = 0; i < std::size (kSoundParams); ++i)
        if (auto* param = processor.apvts.getParameter (kSoundParams[i].id))
            files::setParameterValue (*param, beforeRoll->values[i]);
    processor.getSoundPresets().setCurrent (beforeRoll->entry, beforeRoll->name);
    beforeRoll.reset();
    back.setEnabled (false);
}

void EngineView::paint (juce::Graphics& g)
{
    using namespace bleepforge;
    const auto area = getLocalBounds().toFloat();
    theme::paintPanel (g, area);

    // divider between the shared controls and the engine's own
    theme::paintEngravedLine (g, (float) kLeftWidth, (float) kPadding + 4.0f, area.getHeight() - 2.0f * kPadding - 4.0f);

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
    // the key knobs are larger and stand with their values on the same line as the rest, the
    // others share what is left
    auto lay = [area] (const std::vector<juce::Component*>& controls, const std::vector<size_t>& keys)
    {
        auto row = area.withTrimmedTop (area.getHeight() - Knob::heightFor (kKeyDiameter));
        controls.front()->setBounds (row.removeFromLeft (kChoiceWidth).withTrimmedBottom (Knob::kValueHeight + kKeyDiameter - kDiameter));
        row.removeFromLeft (8);
        const int keyWidth = kKeyDiameter + 4;
        const int others = (int) controls.size() - 1 - (int) keys.size();
        const int width = juce::jmin (kKnobWidth + 14, (row.getWidth() - keyWidth * (int) keys.size()) / juce::jmax (1, others));
        for (size_t i = 1; i < controls.size(); ++i)
        {
            const bool key = std::find (keys.begin(), keys.end(), i) != keys.end();
            auto slot = row.removeFromLeft (key ? keyWidth : width);
            controls[i]->setBounds (slot.removeFromBottom (Knob::heightFor (key ? kKeyDiameter : kDiameter)));
        }
    };
    // the picture takes what the key knobs leave above the row
    picture.setBounds (area.getX(), 27, area.getWidth(), area.getBottom() - Knob::heightFor (kKeyDiameter) - 4 - 27);
    lay (bleepControls, bleepKeys);
    lay (foleyControls, foleyKeys);
    lay (machineControls, machineKeys);
}

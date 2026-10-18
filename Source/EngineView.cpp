// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#include "EngineView.h"
#include "Theme.h"
#include "PresetFiles.h"
#include "Randomizer.h"

namespace
{
constexpr int kPadding = 10, kLeftWidth = 250, kKnobWidth = 62, kChoiceWidth = 104;
} // namespace

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
    for (auto* c : std::initializer_list<juce::Component*> { &engine, &voices, &glide, &output, &dice, &back })
        addAndMakeVisible (c);
    dice.onClick = [this] { rollSound(); };
    back.onClick = [this] { undoRoll(); };
    back.setEnabled (false);

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

// Keeps the engine and the other engine's settings; the output gain is matched like a factory
// preset's. The sound then shows as "Random Bleep" or "Random Foley", ready to save.
void EngineView::rollSound()
{
    using namespace bleepforge;
    auto& apvts = processor.apvts;
    auto& presets = processor.getSoundPresets();
    auto value = [&apvts] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    SoundBefore before { presets.current(), presets.name(), {} };
    for (const auto& p : kSoundParams)
        before.values.push_back (value (p.id));

    const bool foley = value ("engine") >= 0.5f;
    DiceRng rng ((uint64_t) juce::Random::getSystemRandom().nextInt64());
    for (const auto& v : randomMatchedSound (foley, rng, value))
        if (auto* param = apvts.getParameter (v.id))
            files::setParameterValue (*param, v.value);
    presets.setCurrent (-1, foley ? "Random Foley" : "Random Bleep");

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
    // the engine's name, with the dice and back arrow at the right
    auto nameRow = juce::Rectangle<int> (area.getX(), 2, area.getWidth(), 18);
    area.removeFromTop (12);
    dice.setBounds (nameRow.removeFromRight (18));
    nameRow.removeFromRight (4);
    back.setBounds (nameRow.removeFromRight (18));
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

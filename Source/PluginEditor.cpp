// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#include "PluginEditor.h"
#include "BleepForgeVersion.h"

namespace
{
constexpr float kMinScale = 0.75f, kMaxScale = 2.0f;

namespace Palette
{
const juce::Colour background { 0xff15171c };
const juce::Colour panel { 0xff1e2128 };
const juce::Colour outline { 0xff2c3039 };
const juce::Colour text { 0xffd8dce4 };
const juce::Colour dimText { 0xff7a8190 };
const juce::Colour accent { 0xff4fe0b0 };
} // namespace Palette
} // namespace

BleepForgeEditor::BleepForgeEditor (BleepForgeProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setResizable (true, true);
    setResizeLimits (juce::roundToInt (kWidth * kMinScale), juce::roundToInt (kHeight * kMinScale),
                     juce::roundToInt (kWidth * kMaxScale), juce::roundToInt (kHeight * kMaxScale));
    getConstrainer()->setFixedAspectRatio ((double) kWidth / (double) kHeight);
    setSize (kWidth, kHeight);
}

void BleepForgeEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);
    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / (float) kWidth));

    auto area = juce::Rectangle<float> (0.0f, 0.0f, (float) kWidth, (float) kHeight).reduced (16.0f);
    auto header = area.removeFromTop (36.0f);

    g.setColour (Palette::accent);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("BleepForge", header, juce::Justification::centredLeft);
    g.setColour (Palette::dimText);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText (BLEEPFORGE_VERSION_STRING, header, juce::Justification::centredRight);

    area.removeFromTop (8.0f);
    const auto grid = area.removeFromTop (area.getHeight() * 0.6f);
    area.removeFromTop (12.0f);
    const auto engine = area;

    auto placeholder = [&g] (juce::Rectangle<float> r, const char* label)
    {
        g.setColour (Palette::panel);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (Palette::outline);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        g.setColour (Palette::dimText);
        g.setFont (juce::FontOptions (14.0f));
        g.drawText (label, r, juce::Justification::centred);
    };
    placeholder (grid, "Sequencer grid");
    placeholder (engine, "Engine / Chaos / Effects");
}

void BleepForgeEditor::parentHierarchyChanged()
{
   #if JUCE_WINDOWS
    // Taken from SubForge: the CLAP wrapper (the only one that leaves wrapperType undefined)
    // scales the editor by the host's set_scale and reports sizes that include it, but JUCE 8
    // also scales a window embedded in the host by the host window's DPI, so the view came out
    // too big and was cropped above 100 %. Keep the window at 1:1 as the VST3 wrapper does, and
    // push the size to the native window, which addToDesktop created at the DPI scale.
    if (processor.wrapperType == juce::AudioProcessor::wrapperType_Undefined)
        if (auto* peer = getPeer(); peer != nullptr && ! peer->getCustomPlatformScaleFactor().has_value())
        {
            peer->setCustomPlatformScaleFactor (1.0);
            peer->setBounds (peer->getComponent().getBounds(), false);
        }
   #endif
}

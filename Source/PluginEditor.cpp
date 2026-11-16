// Copyright (C) 2026 Henrik Juhl. Part of BleepWerk, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#include "PluginEditor.h"

namespace
{
constexpr float kMinScale = 0.75f, kMaxScale = 2.0f;
} // namespace

BleepWerkEditor::BleepWerkEditor (BleepWerkProcessor& p)
    : AudioProcessorEditor (&p), processor (p), view (p)
{
    // the saved theme, before the look is set: setting it tells every component to re-read
    // its colours. The view goes in first, since only children present then hear of it (adding
    // one later doesn't): the knobs' value boxes kept JUCE's default white outline and the
    // toggles the default theme.
    addAndMakeVisible (view);
    bleepwerk::theme::currentIndex() = bleepwerk::theme::indexOf (p.getSettings().getValue ("theme"));
    lookAndFeel.applyTheme();
    setLookAndFeel (&lookAndFeel);

    setResizable (true, true);
    applyDesignSize (1.0f);
    // the sequencer panel makes the view taller: keep the scale, and follow the new height
    view.onDesignSizeChanged = [this] { applyDesignSize ((float) getWidth() / (float) MainView::kWidth); };
}

// The limits and the aspect ratio for the height the view has now, and the window at that
// height and the given scale.
void BleepWerkEditor::applyDesignSize (float scale)
{
    const int height = view.designHeight();
    setResizeLimits (juce::roundToInt (MainView::kWidth * kMinScale), juce::roundToInt ((float) height * kMinScale),
                     juce::roundToInt (MainView::kWidth * kMaxScale), juce::roundToInt ((float) height * kMaxScale));
    getConstrainer()->setFixedAspectRatio ((double) MainView::kWidth / (double) height);
    setSize (juce::roundToInt ((float) MainView::kWidth * scale), juce::roundToInt ((float) height * scale));
}

BleepWerkEditor::~BleepWerkEditor()
{
    setLookAndFeel (nullptr);
}

void BleepWerkEditor::paint (juce::Graphics& g)
{
    g.fillAll (bleepwerk::theme::background);
}

void BleepWerkEditor::resized()
{
    // vector drawing throughout, so a transform keeps everything sharp at any scale; a
    // host-set size with another aspect ratio leaves the view centred with margins
    const int height = view.designHeight();
    const float scale = juce::jmin ((float) getWidth() / (float) MainView::kWidth, (float) getHeight() / (float) height);
    view.setBounds (0, 0, MainView::kWidth, height);
    view.setTransform (juce::AffineTransform::scale (scale).translated (
        ((float) getWidth() - (float) MainView::kWidth * scale) * 0.5f,
        ((float) getHeight() - (float) height * scale) * 0.5f));
}

void BleepWerkEditor::parentHierarchyChanged()
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
   #else
    juce::ignoreUnused (processor);
   #endif
}

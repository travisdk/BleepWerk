// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#include "PluginEditor.h"

namespace
{
constexpr float kMinScale = 0.75f, kMaxScale = 2.0f;
} // namespace

BleepForgeEditor::BleepForgeEditor (BleepForgeProcessor& p)
    : AudioProcessorEditor (&p), processor (p), view (p)
{
    // the saved theme, before the look is set: setting it tells every component to re-read
    // its colours. The view goes in first, since only children present then hear of it (adding
    // one later doesn't): the knobs' value boxes kept JUCE's default white outline and the
    // toggles the default theme.
    addAndMakeVisible (view);
    bleepforge::theme::currentIndex() = bleepforge::theme::indexOf (p.getSettings().getValue ("theme"));
    lookAndFeel.applyTheme();
    setLookAndFeel (&lookAndFeel);

    setResizable (true, true);
    setResizeLimits (juce::roundToInt (MainView::kWidth * kMinScale), juce::roundToInt (MainView::kHeight * kMinScale),
                     juce::roundToInt (MainView::kWidth * kMaxScale), juce::roundToInt (MainView::kHeight * kMaxScale));
    getConstrainer()->setFixedAspectRatio ((double) MainView::kWidth / (double) MainView::kHeight);
    setSize (MainView::kWidth, MainView::kHeight);
}

BleepForgeEditor::~BleepForgeEditor()
{
    setLookAndFeel (nullptr);
}

void BleepForgeEditor::paint (juce::Graphics& g)
{
    g.fillAll (bleepforge::theme::background);
}

void BleepForgeEditor::resized()
{
    // vector drawing throughout, so a transform keeps everything sharp at any scale; a
    // host-set size with another aspect ratio leaves the view centred with margins
    const float scale = juce::jmin ((float) getWidth() / (float) MainView::kWidth, (float) getHeight() / (float) MainView::kHeight);
    view.setBounds (0, 0, MainView::kWidth, MainView::kHeight);
    view.setTransform (juce::AffineTransform::scale (scale).translated (
        ((float) getWidth() - (float) MainView::kWidth * scale) * 0.5f,
        ((float) getHeight() - (float) MainView::kHeight * scale) * 0.5f));
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
   #else
    juce::ignoreUnused (processor);
   #endif
}

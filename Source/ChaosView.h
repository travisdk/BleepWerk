// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "Controls.h"

// Chaos: how much each hit strays from the patch. Amount scales the depth of every target.
class ChaosView : public juce::Component
{
public:
    explicit ChaosView (BleepForgeProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    Knob amount, pitch, decay, tone, pan, velocity;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChaosView)
};

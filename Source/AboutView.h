// Copyright (C) 2026 Henrik Juhl. Part of BleepWerk, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// About BleepWerk: a panel inside the window (no dialog box) over a dimmed view. It is closed
// by its cross, by Esc or by a click anywhere outside it. Licences swaps the text for the
// third-party notices, which are built into the plug-in so they travel with the binary.
// (The idea is from Progrezzor.)
class AboutView : public juce::Component
{
public:
    explicit AboutView (std::function<void()> closeIt);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void showLicences (bool show);
    juce::Rectangle<int> panel() const;
    juce::Rectangle<int> crossArea() const;

    std::function<void()> onClose;
    juce::HyperlinkButton homepage, plugin;
    juce::TextButton licencesButton;
    juce::TextEditor licenceText;
    bool showingLicences = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AboutView)
};

// Copyright (C) 2026 Henrik Juhl. Part of BleepWerk, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// The controls the sound panels are built from, each tied to one parameter. Each one takes its
// hover text from Tooltips.h. The text goes on the parts the mouse can be over, as JUCE shows
// the tooltip of the component under the mouse only, not of its parents.

// A rotary control for one parameter: name above, value just under the knob; double-click
// resets it. Faded when it has no effect on the current sound (see setActive). The knob is
// as wide as the control, up to maxDiameter, and sits at the top, so a row of knobs with the
// same diameter has its names, knobs and values on the same lines.
struct Knob : juce::Component, juce::SettableTooltipClient
{
    static constexpr int kLabelHeight = 16, kValueHeight = 16;
    static constexpr int heightFor (int diameter) { return kLabelHeight + diameter + kValueHeight; }

    Knob (juce::AudioProcessorValueTreeState&, const juce::String& parameterId, const juce::String& name, bool bipolar = false);
    void resized() override;
    void setMaxDiameter (int diameter) { maxDiameter = diameter; resized(); }
    // Faded and not draggable when it has no effect on the sound. The knob itself stays enabled,
    // so its tooltip still shows.
    void setActive (bool active) { slider.setEnabled (active); setAlpha (active ? 1.0f : 0.35f); }
    void setTooltip (const juce::String& text) { SettableTooltipClient::setTooltip (text); slider.setTooltip (text); }

    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Label label;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    int maxDiameter = 1000;
};

// A choice parameter shown as a drop-down with its name above, centred on the rest of the
// height, so beside a row of knobs it lines up with their circles.
struct Choice : juce::Component, juce::SettableTooltipClient
{
    Choice (juce::AudioProcessorValueTreeState&, const juce::String& parameterId, const juce::String& name);
    void resized() override;
    void setTooltip (const juce::String& text) { SettableTooltipClient::setTooltip (text); box.setTooltip (text); }

    juce::ComboBox box;
    juce::Label label;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment attachment;
};

// One button per option, side by side, for a choice parameter with a few options. Stacked
// instead where there is height but no width; labels, if given, replace the option names
// on the buttons.
struct Toggle : juce::Component, juce::SettableTooltipClient
{
    Toggle (juce::AudioProcessorValueTreeState&, const juce::String& parameterId, const juce::String& name,
            bool stacked = false, const juce::StringArray& labels = {});
    void resized() override;
    void setTooltip (const juce::String& text);
    void update(); // follows the parameter
    void lookAndFeelChanged() override; // the theme changed

    juce::RangedAudioParameter& parameter;
    juce::Label label;
    juce::OwnedArray<juce::TextButton> buttons;
    bool stacked;
    int buttonHeight = 26; // of a side-by-side row
    std::function<void (int)> onSelect; // called with the index after a click on a button (not when the parameter changes otherwise)
};

// A small icon button drawn without fonts or images: a die (roll random values) or a back
// arrow (undo the last roll). Dimmed while not active, but still clickable, so its tooltip
// shows; the action has to check for itself that there is something to do.
struct IconButton : juce::Button
{
    enum class Icon { Dice, Back };
    IconButton (Icon, const juce::String& tooltip);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void setActive (bool on) { active = on; repaint(); }

    Icon icon;
    bool active = true;
};

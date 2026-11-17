// Copyright (C) 2026 Henrik Juhl. Part of BleepWerk, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#include "AboutView.h"
#include "BleepWerkVersion.h"
#include "Fonts.h"
#include "LicenceData.h"
#include "Theme.h"

namespace
{
constexpr int kAboutWidth = 460, kAboutHeight = 330, kLicencesWidth = 700, kLicencesHeight = 380;
constexpr int kPad = 28;
} // namespace

AboutView::AboutView (std::function<void()> closeIt) : onClose (std::move (closeIt))
{
    using namespace bleepwerk;
    setWantsKeyboardFocus (true);

    const auto link = [this] (juce::HyperlinkButton& button, const char* text, const char* url)
    {
        button.setButtonText (text);
        button.setURL (juce::URL (url));
        button.setColour (juce::HyperlinkButton::textColourId, theme::primary());
        button.setTooltip (url);
        button.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (button);
    };
    link (homepage, "music.henrikjuhl.dk", "https://music.henrikjuhl.dk");
    link (plugin, "music.henrikjuhl.dk/plugins/bleepwerk", "https://music.henrikjuhl.dk/plugins/bleepwerk/");

    licencesButton.setButtonText ("Licences");
    licencesButton.setTooltip ("Licences and third-party notices");
    licencesButton.onClick = [this] { showLicences (! showingLicences); };
    addAndMakeVisible (licencesButton);

    licenceText.setMultiLine (true, true);
    licenceText.setReadOnly (true);
    licenceText.setCaretVisible (false);
    licenceText.setScrollbarsShown (true);
    licenceText.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain)));
    licenceText.setColour (juce::TextEditor::backgroundColourId, theme::background);
    licenceText.setColour (juce::TextEditor::textColourId, theme::text);
    licenceText.setColour (juce::TextEditor::outlineColourId, theme::outline);
    licenceText.setColour (juce::TextEditor::focusedOutlineColourId, theme::outline);
    licenceText.setIndents (12, 10);
    licenceText.setText (juce::String::fromUTF8 (LicenceData::THIRDPARTYNOTICES_txt, LicenceData::THIRDPARTYNOTICES_txtSize), false);
    licenceText.moveCaretToTop (false);
    addChildComponent (licenceText);
}

juce::Rectangle<int> AboutView::panel() const
{
    return juce::Rectangle<int> (showingLicences ? kLicencesWidth : kAboutWidth, showingLicences ? kLicencesHeight : kAboutHeight)
        .withCentre (getLocalBounds().getCentre());
}

juce::Rectangle<int> AboutView::crossArea() const
{
    return juce::Rectangle<int> (36, 36).withRightX (panel().getRight() - 8).withY (panel().getY() + 8);
}

void AboutView::showLicences (bool show)
{
    showingLicences = show;
    licencesButton.setButtonText (show ? "Back" : "Licences");
    licenceText.setVisible (show);
    homepage.setVisible (! show);
    plugin.setVisible (! show);
    resized();
    if (show)
        licenceText.moveCaretToTop (false);
    repaint();
}

void AboutView::resized()
{
    const auto box = panel();
    if (showingLicences)
    {
        licencesButton.setBounds (box.getRight() - 44 - 80, box.getY() + 10, 80, 28); // Back, left of the cross
        licenceText.setBounds (box.withTrimmedTop (58).reduced (kPad, 0).withTrimmedBottom (22));
        return;
    }
    licencesButton.setBounds (box.getRight() - kPad - 96, box.getBottom() - 22 - 28, 96, 28);
    const int top = box.getY() + 22 + 38 + 22 + 24 + 6 + 22 + 22 + 8; // under the licence line
    homepage.setBounds (box.getX() + kPad, top, box.getWidth() - 2 * kPad, 24);
    plugin.setBounds (box.getX() + kPad, top + 24, box.getWidth() - 2 * kPad, 24);
}

void AboutView::paint (juce::Graphics& g)
{
    using namespace bleepwerk;
    g.fillAll (juce::Colours::black.withAlpha (0.55f)); // the view behind is dimmed

    const auto box = panel();
    const auto r = box.toFloat().reduced (1.5f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (r.translated (0.0f, 3.0f), 10.0f);
    g.setColour (theme::panel);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (theme::primary().withAlpha (0.8f));
    g.drawRoundedRectangle (r, 10.0f, 1.5f);

    // the cross
    g.setColour (theme::dimText.brighter (0.3f));
    const auto c = crossArea().toFloat().getCentre();
    g.drawLine (c.x - 6.0f, c.y - 6.0f, c.x + 6.0f, c.y + 6.0f, 2.2f);
    g.drawLine (c.x - 6.0f, c.y + 6.0f, c.x + 6.0f, c.y - 6.0f, 2.2f);

    auto area = box.reduced (kPad, 22);
    auto line = [&area] (int height) { return area.removeFromTop (height); };

    if (showingLicences)
    {
        g.setColour (theme::text);
        g.setFont (fonts::title (22.0f));
        g.drawText ("Licences", line (32), juce::Justification::centredLeft);
        return;
    }

    g.setColour (theme::primary());
    g.setFont (fonts::title (30.0f));
    g.drawText ("BleepWerk", line (38), juce::Justification::centredLeft);

    g.setColour (theme::dimText.brighter (0.2f));
    g.setFont (fonts::label (15.0f));
    g.drawText (juce::String ("Version ") + BLEEPWERK_VERSION_STRING, line (22), juce::Justification::centredLeft);
    g.setColour (theme::text);
    g.drawText ("Bleeps, blips and struck sounds.", line (24), juce::Justification::centredLeft);
    line (6);
    g.drawText ("by Henrik Juhl (travisdk)", line (22), juce::Justification::centredLeft);
    g.drawText ("Free software under the GNU AGPLv3", line (22), juce::Justification::centredLeft);
    line (8 + 2 * 24 + 8); // the two links lie here (see resized)

    g.setColour (theme::dimText.brighter (0.2f));
    g.setFont (fonts::label (14.0f));
    g.drawFittedText ("Made with JUCE and clap-juce-extensions. Fonts: Barlow Semi Condensed and IBM Plex Mono (SIL OFL).",
                      line (40), juce::Justification::topLeft, 2);
    line (4);
    g.drawText ("Click Licences for all third-party notices.", line (20).withTrimmedRight (110), juce::Justification::centredLeft);
}

void AboutView::mouseDown (const juce::MouseEvent& e)
{
    if ((crossArea().contains (e.getPosition()) || ! panel().contains (e.getPosition())) && onClose)
        onClose();
}

bool AboutView::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey && onClose)
    {
        onClose();
        return true;
    }
    return false;
}

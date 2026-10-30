// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#include "MainView.h"
#include "PresetFiles.h"
#include "Theme.h"
#include "BleepForgeVersion.h"

namespace
{
constexpr int kMargin = 16, kHeaderHeight = 40, kEngineHeight = 176, kChaosHeight = 128, kGap = 10, kChaosWidth = 420;
} // namespace

MainView::MainView (BleepForgeProcessor& p)
    : processor (p), engineView (p), chaosView (p), driveView (p), effectsView (p)
{
    setSize (kWidth, kHeight);
    using namespace bleepforge;

    // header: settings and the sound preset browser
    settingsButton.setTooltip ("Settings");
    settingsButton.onClick = [this] { showSettingsMenu(); };
    addAndMakeVisible (settingsButton);
    presetPrev.setConnectedEdges (juce::Button::ConnectedOnRight);
    presetButton.setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    presetNext.setConnectedEdges (juce::Button::ConnectedOnLeft);
    presetButton.setColour (juce::TextButton::textColourOffId, theme::text);
    presetPrev.onClick = [this] { stepPreset (-1); };
    presetNext.onClick = [this] { stepPreset (1); };
    presetButton.onClick = [this] { showPresetMenu(); };
    saveButton.onClick = [this]
    {
        auto& presets = processor.getSoundPresets();
        const auto suggestion = presets.isUser (presets.current()) ? presets.name() : "My " + presets.name();
        showNameDialog (suggestion, [this] (const juce::String& name) { savePreset (name); });
    };
    for (auto* button : { &presetPrev, &presetButton, &presetNext, &saveButton })
        addAndMakeVisible (*button);

    // the sound
    addAndMakeVisible (engineView);
    addAndMakeVisible (chaosView);
    addAndMakeVisible (driveView);
    addAndMakeVisible (effectsView);

    refreshPreset();
    startTimerHz (10);
}

MainView::~MainView()
{
    stopTimer();
}

// The preset can change from the host (programs) or a loaded project.
void MainView::timerCallback()
{
    if (processor.getSoundPresets().version() != shownPresetVersion)
        refreshPreset();
}

// For every open BleepForge editor of this process, and saved for the next ones.
void MainView::chooseTheme (int index)
{
    bleepforge::theme::currentIndex() = index;
    processor.getSettings().setValue ("theme", bleepforge::kThemes[(size_t) index].name);
    processor.getSettings().saveIfNeeded();
    if (auto* editor = findParentComponentOfClass<juce::AudioProcessorEditor>())
    {
        if (auto* look = dynamic_cast<bleepforge::theme::LookAndFeel*> (&editor->getLookAndFeel()))
            look->applyTheme();
        editor->sendLookAndFeelChange();
        editor->repaint();
    }
}

void MainView::showSettingsMenu()
{
    juce::PopupMenu menu;

    // each theme in its bright shade
    menu.addSectionHeader ("Colour");
    for (int i = 0; i < bleepforge::kNumThemes; ++i)
    {
        juce::PopupMenu::Item item (bleepforge::kThemes[i].name);
        item.colour = juce::Colour (0xff000000 | bleepforge::kThemes[i].primary);
        item.isTicked = i == bleepforge::theme::currentIndex();
        item.action = [this, i] { chooseTheme (i); };
        menu.addItem (item);
    }

    auto& presets = processor.getSoundPresets();
    const int entry = presets.current();
    const bool onUserPreset = presets.isUser (entry);
    menu.addSectionHeader ("Presets");
    menu.addItem ("Open preset folder", []
                  {
                      auto folder = SoundPresets::folder();
                      folder.createDirectory();
                      bleepforge::files::openFolder (folder);
                  });
    menu.addItem ("Rescan presets", [&presets] { presets.rescan(); });
    menu.addItem ("Delete \"" + (onUserPreset ? presets.entryName (entry) : juce::String ("user preset")) + "\"", onUserPreset, false,
                  [this, entry]
                  {
                      auto& p = processor.getSoundPresets();
                      confirm ("Delete preset?", "\"" + p.entryName (entry) + "\" will be moved to the trash.", "Delete",
                               [&p, entry] { p.remove (entry); });
                  });
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (settingsButton).withDeletionCheck (*this));
}

// One submenu per category, in the factory order, and the user's presets last.
void MainView::showPresetMenu()
{
    auto& presets = processor.getSoundPresets();
    juce::PopupMenu menu;
    juce::PopupMenu category;
    juce::String categoryName;
    bool categoryHasCurrent = false;

    auto flush = [&]
    {
        if (categoryName.isNotEmpty())
            menu.addSubMenu (categoryName, category, true, nullptr, categoryHasCurrent);
        category = {};
        categoryHasCurrent = false;
    };
    for (int i = 0; i < presets.numEntries(); ++i)
    {
        if (presets.entryCategory (i) != categoryName)
        {
            flush();
            categoryName = presets.entryCategory (i);
        }
        const bool isCurrent = i == presets.current();
        categoryHasCurrent = categoryHasCurrent || isCurrent;
        category.addItem (presets.entryName (i), true, isCurrent, [&presets, i] { presets.load (i); });
    }
    flush();
    if (presets.numEntries() == presets.numFactory())
        menu.addItem ("No user presets yet: Save stores one", false, false, {});

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetButton).withMinimumWidth (presetButton.getWidth())
                            .withDeletionCheck (*this));
}

void MainView::stepPreset (int delta)
{
    auto& presets = processor.getSoundPresets();
    const int n = presets.numEntries();
    if (presets.current() < 0) // a rolled sound: on to the first or back to the last
        presets.load (delta > 0 ? 0 : n - 1);
    else
        presets.load ((presets.current() + delta + n) % n);
}

void MainView::refreshPreset()
{
    auto& presets = processor.getSoundPresets();
    shownPresetVersion = presets.version();
    presetButton.setButtonText (presets.name());
    const auto category = presets.entryCategory (presets.current());
    presetButton.setTooltip (category.isEmpty() ? presets.name() : category + ": " + presets.name());
}

void MainView::savePreset (const juce::String& name)
{
    auto save = [this, name]
    {
        if (! processor.getSoundPresets().save (name))
            confirm ("Could not save", "Writing to " + SoundPresets::folder().getFullPathName() + " failed.", "OK", {});
    };

    if (processor.getSoundPresets().exists (name))
        confirm ("Overwrite preset?", "\"" + name + "\" already exists.", "Overwrite", save);
    else
        save();
}

// Shown inside the view rather than as a separate desktop window, which some hosts bury.
// (From SubForge.)
void MainView::showNameDialog (const juce::String& suggestion, std::function<void (const juce::String&)> onName)
{
    auto* window = new juce::AlertWindow ("Save preset", "Saved to " + SoundPresets::folder().getFullPathName(),
                                          juce::MessageBoxIconType::NoIcon);
    window->addTextEditor ("name", suggestion, "Name:");
    window->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    addAndMakeVisible (window);
    window->setCentrePosition (getLocalBounds().getCentre());
    window->enterModalState (true, juce::ModalCallbackFunction::create (
        [safeThis = juce::Component::SafePointer<MainView> (this), window, onName] (int result)
        {
            const auto name = window->getTextEditorContents ("name").trim();
            if (safeThis != nullptr && result == 1 && name.isNotEmpty())
                onName (name);
        }), true);

    if (auto* nameEditor = window->getTextEditor ("name"))
    {
        nameEditor->grabKeyboardFocus();
        nameEditor->selectAll();
    }
}

// A two-button message (one, without onOk) shown inside the view.
void MainView::confirm (const juce::String& title, const juce::String& message, const juce::String& okText, std::function<void()> onOk)
{
    auto* window = new juce::AlertWindow (title, message, juce::MessageBoxIconType::NoIcon);
    window->addButton (okText, 1, juce::KeyPress (juce::KeyPress::returnKey));
    if (onOk)
        window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    addAndMakeVisible (window);
    window->setCentrePosition (getLocalBounds().getCentre());
    window->enterModalState (true, juce::ModalCallbackFunction::create (
        [safeThis = juce::Component::SafePointer<MainView> (this), onOk] (int result)
        {
            if (safeThis != nullptr && result == 1 && onOk)
                onOk();
        }), true);
}

void MainView::GearButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    using namespace bleepforge;
    const auto area = getLocalBounds().toFloat().reduced (4.0f);
    const auto centre = area.getCentre();
    const float outer = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f, inner = outer * 0.72f;

    juce::Path gear;
    constexpr int teeth = 8;
    for (int i = 0; i < teeth * 2; ++i)
    {
        // each tooth is a flat top on the outer radius, the gaps sit on the inner one
        const float radius = (i % 2 == 0) ? outer : inner;
        for (float edge : { -0.5f, 0.5f })
        {
            const float angle = juce::MathConstants<float>::twoPi * ((float) i + edge * 0.8f) / (float) (teeth * 2);
            const auto p = centre.getPointOnCircumference (radius, angle);
            if (i == 0 && edge < 0.0f)
                gear.startNewSubPath (p);
            else
                gear.lineTo (p);
        }
    }
    gear.closeSubPath();
    gear.addEllipse (juce::Rectangle<float> (outer * 0.7f, outer * 0.7f).withCentre (centre));
    gear.setUsingNonZeroWinding (false);

    g.setColour (down ? theme::primary() : (highlighted ? theme::text : theme::dimText));
    g.fillPath (gear);
}

juce::Rectangle<int> MainView::headerArea() const
{
    return getLocalBounds().reduced (kMargin).removeFromTop (kHeaderHeight);
}

void MainView::paint (juce::Graphics& g)
{
    using namespace bleepforge;
    g.fillAll (theme::background);

    // the wordmark: a little square pulse, the bleep itself, then BLEEP in the theme colour and
    // FORGE in white
    auto header = headerArea().toFloat();
    const float midY = header.getCentreY();
    juce::Path pulse;
    const float x0 = header.getX() + 1.0f;
    pulse.startNewSubPath (x0, midY + 9.0f);
    for (auto [dx, y] : { std::pair { 6.0f, 9.0f }, { 6.0f, -9.0f }, { 14.0f, -9.0f }, { 14.0f, 9.0f }, { 20.0f, 9.0f },
                          { 20.0f, 1.0f }, { 26.0f, 1.0f }, { 26.0f, 9.0f }, { 32.0f, 9.0f } })
        pulse.lineTo (x0 + dx, midY + y);
    g.setColour (theme::primary());
    g.strokePath (pulse, juce::PathStrokeType (2.6f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));

    const auto wordmark = fonts::title (30.0f);
    auto widthOf = [&wordmark] (const juce::String& text)
    {
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText (wordmark, text, 0.0f, 0.0f);
        return glyphs.getBoundingBox (0, -1, true).getWidth() + 2.0f;
    };
    const float textX = x0 + 44.0f, bleepWidth = widthOf ("BLEEP"), forgeWidth = widthOf ("FORGE");
    g.setFont (wordmark);
    g.setColour (theme::primary());
    g.drawText ("BLEEP", juce::Rectangle<float> (textX, header.getY(), bleepWidth, header.getHeight()), juce::Justification::centredLeft);
    g.setColour (theme::text);
    g.drawText ("FORGE", juce::Rectangle<float> (textX + bleepWidth, header.getY(), forgeWidth, header.getHeight()), juce::Justification::centredLeft);

    g.setColour (theme::dimText);
    g.setFont (fonts::value (11.0f));
    g.drawText (BLEEPFORGE_VERSION_STRING, header.withTrimmedLeft (textX + bleepWidth + forgeWidth + 12.0f - header.getX()).withTrimmedTop (8.0f),
                juce::Justification::centredLeft);
}

void MainView::resized()
{
    auto area = getLocalBounds().reduced (kMargin);

    // header: the title, then the preset browser and settings on the right
    auto header = area.removeFromTop (kHeaderHeight);
    settingsButton.setBounds (header.removeFromRight (26).withSizeKeepingCentre (26, 26));
    header.removeFromRight (14);
    saveButton.setBounds (header.removeFromRight (52).withSizeKeepingCentre (52, 26));
    header.removeFromRight (8);
    for (auto [button, width] : { std::pair { &presetNext, 26 }, { &presetButton, 200 }, { &presetPrev, 26 } })
        button->setBounds (header.removeFromRight (width).withSizeKeepingCentre (width, 26));

    // the sound: the engine; Chaos and Drive side by side across the whole width; the effects
    area.removeFromTop (kGap);
    engineView.setBounds (area.removeFromTop (kEngineHeight));
    area.removeFromTop (kGap);
    auto middle = area.removeFromTop (kChaosHeight);
    chaosView.setBounds (middle.removeFromLeft (kChaosWidth));
    middle.removeFromLeft (kGap);
    driveView.setBounds (middle);
    area.removeFromTop (kGap);
    effectsView.setBounds (area.removeFromTop (EffectsView::kHeight));
}

// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#include "MainView.h"
#include "Theme.h"
#include "BleepForgeVersion.h"
#include "Randomizer.h"

namespace
{
constexpr int kMargin = 16, kHeaderHeight = 40, kBarHeight = 28, kStatusHeight = 26, kEngineHeight = 150, kBottomHeight = 120,
              kGap = 10, kChaosWidth = 350;

using EditMode = BleepForgeProcessor::EditMode;

juce::String helpText (EditMode mode)
{
    const juce::String value = mode == EditMode::Velocity ? "velocity" : mode == EditMode::Ratchet ? "ratchet (1-4 hits)" : "probability";
    return "Click: on/off    Drag up/down: " + value + "    Double-click: reset";
}

void styleBarLabel (juce::Label& label)
{
    label.setJustificationType (juce::Justification::centredRight);
    label.setFont (juce::FontOptions (13.0f));
}
} // namespace

MainView::MainView (BleepForgeProcessor& p)
    : processor (p), gridView (p), engineView (p), chaosView (p), effectsView (p)
{
    setSize (kWidth, kHeight);
    using namespace bleepforge;

    // header: edit mode and settings
    editLabel.setJustificationType (juce::Justification::centredRight);
    editLabel.setFont (juce::FontOptions (13.0f));
    addAndMakeVisible (editLabel);
    for (auto* button : { &probabilityButton, &velocityButton, &ratchetButton })
    {
        button->setClickingTogglesState (false);
        addAndMakeVisible (*button);
    }
    probabilityButton.setConnectedEdges (juce::Button::ConnectedOnRight);
    velocityButton.setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    ratchetButton.setConnectedEdges (juce::Button::ConnectedOnLeft);
    probabilityButton.onClick = [this] { setEditMode (EditMode::Probability); };
    velocityButton.onClick = [this] { setEditMode (EditMode::Velocity); };
    ratchetButton.onClick = [this] { setEditMode (EditMode::Ratchet); };
    settingsButton.setTooltip ("Settings");
    settingsButton.onClick = [this] { showSettingsMenu(); };
    addAndMakeVisible (settingsButton);

    // the sound preset browser
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

    // sequencer bar
    // grouped under headings; the item IDs stay the parameter's order
    for (auto* group : kScaleGroups)
    {
        scaleBox.addSectionHeading (group);
        for (int i = 0; i < kNumScales; ++i)
            if (std::strcmp (kScales[i].group, group) == 0)
                scaleBox.addItem (kScales[i].name, i + 1);
    }
    for (auto* name : kRootNames)
        rootBox.addItem (name, rootBox.getNumItems() + 1);
    for (auto* name : kSeqRateNames)
        rateBox.addItem (name, rateBox.getNumItems() + 1);
    scaleAttachment = std::make_unique<ComboAttachment> (processor.apvts, "seqScale", scaleBox);
    rootAttachment = std::make_unique<ComboAttachment> (processor.apvts, "seqRoot", rootBox);
    rateAttachment = std::make_unique<ComboAttachment> (processor.apvts, "seqRate", rateBox);

    stepsSlider.setRange (1.0, (double) kMaxSteps, 1.0);
    stepsSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 34, 24);
    stepsSlider.setValue (processor.getGrid().getLength(), juce::dontSendNotification);
    stepsSlider.onValueChange = [this]
    {
        processor.getGrid().setLength ((int) stepsSlider.getValue());
        processor.gridEdited();
    };

    swingSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 24);
    swingAttachment = std::make_unique<SliderAttachment> (processor.apvts, "seqSwing", swingSlider);
    if (auto* swing = processor.apvts.getParameter ("seqSwing"))
        swingSlider.setDoubleClickReturnValue (true, swing->convertFrom0to1 (swing->getDefaultValue()));

    for (auto* label : { &scaleLabel, &rootLabel, &stepsLabel, &rateLabel, &swingLabel })
    {
        styleBarLabel (*label);
        addAndMakeVisible (*label);
    }
    for (auto* c : std::initializer_list<juce::Component*> { &scaleBox, &rootBox, &stepsSlider, &rateBox, &swingSlider,
                                                             &patternBack, &patternDice })
        addAndMakeVisible (c);
    patternDice.onClick = [this] { rollPattern(); };
    patternBack.onClick = [this] { undoPatternRoll(); };
    patternBack.setEnabled (false);

    for (auto* name : kCycleNames)
        cycleBox.addItem (name, cycleBox.getNumItems() + 1);
    cycleBox.setTooltip ("Repeat the pattern's chances as a phrase of this length");
    cycleAttachment = std::make_unique<ComboAttachment> (processor.apvts, "seqCycle", cycleBox);
    seedSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 40, 22);
    seedSlider.setTooltip ("Which phrase the cycle repeats");
    seedAttachment = std::make_unique<SliderAttachment> (processor.apvts, "seqSeed", seedSlider);
    for (auto* label : { &cycleLabel, &seedLabel })
    {
        styleBarLabel (*label);
        addAndMakeVisible (*label);
    }
    addAndMakeVisible (cycleBox);
    addAndMakeVisible (seedSlider);

    // grid, status line and sound
    status.setFont (juce::FontOptions (12.0f));
    status.setColour (juce::Label::textColourId, theme::dimText);
    addAndMakeVisible (status);
    gridView.onStatus = [this] (const juce::String& text) { showStatus (text); };
    addAndMakeVisible (gridView);
    addAndMakeVisible (engineView);
    addAndMakeVisible (chaosView);
    addAndMakeVisible (effectsView);

    setEditMode (processor.editMode.load());
    refreshPreset();
    startTimerHz (10);
}

MainView::~MainView()
{
    stopTimer();
}

// The grid's length isn't a parameter, so a loaded project doesn't tell the slider; the
// preset can change from the host (programs) or a loaded project; Seed follows Cycle.
void MainView::timerCallback()
{
    if (processor.getSoundPresets().version() != shownPresetVersion)
        refreshPreset();

    // the seed only matters with a cycle
    const bool cycling = processor.apvts.getRawParameterValue ("seqCycle")->load() >= 0.5f;
    seedSlider.setEnabled (cycling);
    seedLabel.setEnabled (cycling);

    const int length = processor.getGrid().getLength();
    if ((int) stepsSlider.getValue() != length && ! stepsSlider.isMouseButtonDown())
        stepsSlider.setValue (length, juce::dontSendNotification);
}

void MainView::showStatus (const juce::String& text)
{
    status.setText (text.isEmpty() ? helpText (processor.editMode.load()) : text, juce::dontSendNotification);
}

void MainView::setEditMode (EditMode mode)
{
    using namespace bleepforge;
    processor.editMode = mode;
    probabilityButton.setToggleState (mode == EditMode::Probability, juce::dontSendNotification);
    velocityButton.setToggleState (mode == EditMode::Velocity, juce::dontSendNotification);
    ratchetButton.setToggleState (mode == EditMode::Ratchet, juce::dontSendNotification);
    probabilityButton.setColour (juce::TextButton::textColourOnId, theme::primary());
    velocityButton.setColour (juce::TextButton::textColourOnId, theme::secondary());
    ratchetButton.setColour (juce::TextButton::textColourOnId, theme::secondary());
    gridView.editModeChanged();
    showStatus ({});
}

// The edit mode buttons carry the theme's shades.
void MainView::lookAndFeelChanged()
{
    setEditMode (processor.editMode.load());
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
    auto* range = processor.apvts.getParameter ("seqRange");
    const int current = (int) processor.apvts.getRawParameterValue ("seqRange")->load();

    juce::PopupMenu rangeMenu;
    for (int octaves = 1; octaves <= BleepForgeProcessor::kMaxVisibleOctaves; ++octaves)
        rangeMenu.addItem (juce::String (octaves) + (octaves == 1 ? " octave" : " octaves"), true, octaves == current,
                           [range, octaves]
                           {
                               range->beginChangeGesture();
                               range->setValueNotifyingHost (range->convertTo0to1 ((float) octaves));
                               range->endChangeGesture();
                           });

    juce::PopupMenu menu;
    menu.addSectionHeader ("Grid");
    menu.addSubMenu ("Visible range: " + juce::String (current) + (current == 1 ? " octave" : " octaves"), rangeMenu);

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
                      folder.startAsProcess();
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

// Replaces the whole pattern; scale, key, steps and rate stay as they are.
void MainView::rollPattern()
{
    auto& grid = processor.getGrid();
    patternBeforeRoll = std::make_unique<bleepforge::Grid>();
    patternBeforeRoll->copyFrom (grid);

    bleepforge::DiceRng rng ((uint64_t) juce::Random::getSystemRandom().nextInt64());
    bleepforge::randomPattern (grid, processor.sequencerSettings().scale, gridView.firstRow(), gridView.visibleRows(), rng);
    processor.gridEdited();
    patternBack.setEnabled (true);
}

// One step back only, like the sound dice.
void MainView::undoPatternRoll()
{
    if (patternBeforeRoll == nullptr)
        return;
    processor.getGrid().copyFrom (*patternBeforeRoll);
    processor.gridEdited();
    patternBeforeRoll.reset();
    patternBack.setEnabled (false);
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

    auto header = headerArea().toFloat();
    g.setColour (theme::primary());
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("BleepForge", header, juce::Justification::centredLeft);
    g.setColour (theme::dimText);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (BLEEPFORGE_VERSION_STRING, header.withTrimmedLeft (128.0f).withTrimmedTop (6.0f),
                juce::Justification::centredLeft);
}

void MainView::resized()
{
    auto area = getLocalBounds().reduced (kMargin);

    // header: edit mode after the title, settings on the right
    auto header = area.removeFromTop (kHeaderHeight);
    settingsButton.setBounds (header.removeFromRight (26).withSizeKeepingCentre (26, 26));
    header.removeFromRight (14);
    saveButton.setBounds (header.removeFromRight (52).withSizeKeepingCentre (52, 26));
    header.removeFromRight (8);
    for (auto [button, width] : { std::pair { &presetNext, 26 }, { &presetButton, 200 }, { &presetPrev, 26 } })
        button->setBounds (header.removeFromRight (width).withSizeKeepingCentre (width, 26));
    header.removeFromLeft (176);
    editLabel.setBounds (header.removeFromLeft (36).withSizeKeepingCentre (36, 26));
    header.removeFromLeft (6);
    for (auto [button, width] : { std::pair { &probabilityButton, 86 }, { &velocityButton, 70 }, { &ratchetButton, 70 } })
        button->setBounds (header.removeFromLeft (width).withSizeKeepingCentre (width, 26));

    // sequencer bar
    area.removeFromTop (4);
    auto bar = area.removeFromTop (kBarHeight);
    auto place = [&bar] (juce::Component& label, int labelWidth, juce::Component& control, int width)
    {
        label.setBounds (bar.removeFromLeft (labelWidth));
        bar.removeFromLeft (6);
        control.setBounds (bar.removeFromLeft (width).withSizeKeepingCentre (width, 24));
        bar.removeFromLeft (18);
    };
    place (scaleLabel, 44, scaleBox, 160);
    place (rootLabel, 36, rootBox, 64);
    place (stepsLabel, 44, stepsSlider, 96);
    place (rateLabel, 36, rateBox, 78);
    place (swingLabel, 44, swingSlider, 130);
    patternDice.setBounds (bar.removeFromRight (22).withSizeKeepingCentre (22, 22));
    bar.removeFromRight (4);
    patternBack.setBounds (bar.removeFromRight (22).withSizeKeepingCentre (22, 22));

    // sound at the bottom: the engine, then Chaos and the effects side by side
    auto bottom = area.removeFromBottom (kBottomHeight);
    chaosView.setBounds (bottom.removeFromLeft (kChaosWidth));
    bottom.removeFromLeft (kGap);
    effectsView.setBounds (bottom);
    area.removeFromBottom (kGap);
    engineView.setBounds (area.removeFromBottom (kEngineHeight));
    area.removeFromBottom (kGap);

    area.removeFromTop (8);
    auto footer = area.removeFromBottom (kStatusHeight);
    footer.removeFromRight ((int) (GridView::kOverviewWidth + GridView::kOverviewGap));
    seedSlider.setBounds (footer.removeFromRight (92).withSizeKeepingCentre (92, 22));
    footer.removeFromRight (6);
    seedLabel.setBounds (footer.removeFromRight (34));
    footer.removeFromRight (16);
    cycleBox.setBounds (footer.removeFromRight (84).withSizeKeepingCentre (84, 22));
    footer.removeFromRight (6);
    cycleLabel.setBounds (footer.removeFromRight (40));
    status.setBounds (footer.withTrimmedLeft ((int) GridView::kLabelWidth - 4));
    gridView.setBounds (area);
}

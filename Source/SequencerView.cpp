// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#include "SequencerView.h"
#include "PresetFiles.h"
#include "Theme.h"
#include "Fonts.h"

using namespace bleepforge;

namespace
{
constexpr int kPadding = 10, kTopRow = 24, kGridTop = 54, kSlotWidth = 26, kKnobWidth = 56, kDiameter = 40;
constexpr int kLedHeight = 8, kNoteHeight = 14, kLevelHeight = 10, kGap = 3;
constexpr int kPixelsPerSemitone = 4;
} // namespace

//==============================================================================
// The sixteen steps.
class SequencerView::StepGrid : public juce::Component
{
public:
    explicit StepGrid (BleepForgeProcessor& p) : processor (p) { setWantsKeyboardFocus (false); }

    void setPlayhead (int step)
    {
        if (step != playhead)
        {
            playhead = step;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto& pattern = processor.getPatternBank().activePattern();
        const int length = pattern.groove().length;
        const int root = (int) processor.apvts.getRawParameterValue ("seqRoot")->load();
        constexpr float noteSpan = (float) (kMaxStepNote - kMinStepNote);

        for (int i = 0; i < kMaxSteps; ++i)
        {
            const auto step = pattern.get (i);
            const bool inPattern = i < length, playing = i == playhead;
            const float fade = inPattern ? 1.0f : 0.35f;

            // the playhead
            const auto led = juce::Rectangle<float> (5.0f, 5.0f).withCentre (cell (i, Row::Led).getCentre());
            g.setColour (playing ? theme::primary() : theme::outline.withMultipliedAlpha (fade));
            g.fillEllipse (led);

            // the bar box: a plain outline, brighter while the step plays
            const auto box = cell (i, Row::Bar);
            g.setColour (theme::background);
            g.fillRect (box);
            g.setColour (playing ? theme::primary() : theme::outline);
            g.drawRect (box, playing ? 2.0f : 1.0f);
            const auto inner = box.reduced (4.0f);
            if (step.gate)
            {
                // the higher the note, the taller the bar; the velocity sets its strength
                const float height = 0.08f + 0.92f * (float) (step.note - kMinStepNote) / noteSpan;
                const auto bar = inner.withTop (inner.getBottom() - juce::jmax (3.0f, inner.getHeight() * height));
                g.setColour (theme::primary().withAlpha ((0.45f + 0.275f * (float) step.level) * fade));
                g.fillRect (bar);
            }
            else
            {
                g.setColour (theme::outline);
                g.fillRect (inner.withTop (inner.getBottom() - 2.0f));
            }

            // the note name
            g.setFont (fonts::value (11.0f));
            g.setColour (step.gate ? theme::text.withMultipliedAlpha (fade) : theme::dimText.withMultipliedAlpha (fade));
            g.drawText (step.gate ? juce::MidiMessage::getMidiNoteName (juce::jlimit (0, 127, root + step.note), true, true, 3) : juce::String ("-"),
                        cell (i, Row::Note), juce::Justification::centred);

            // the velocity: one to three pips
            const auto levelCell = cell (i, Row::Level);
            const float pip = (levelCell.getWidth() - 2.0f * 2.0f) / 3.0f;
            for (int k = 0; k < kNumStepLevels; ++k)
            {
                const bool on = step.gate && k <= step.level;
                g.setColour (on ? theme::primary().withMultipliedAlpha (fade) : theme::outline.withMultipliedAlpha (fade));
                g.fillRect (levelCell.getX() + (float) k * (pip + 2.0f), levelCell.getCentreY() - 2.0f, pip, 4.0f);
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragStep = stepAt (e.x);
        dragRow = rowAt (e.y);
        dragged = false;
        auto& pattern = processor.getPatternBank().activePattern();
        dragStartNote = pattern.get (dragStep).note;
        if (dragRow == Row::Level)
        {
            auto step = pattern.get (dragStep);
            step.level = (step.level + 1) % kNumStepLevels;
            pattern.set (dragStep, step);
            edited();
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragRow != Row::Bar || dragStep < 0)
            return;
        auto& pattern = processor.getPatternBank().activePattern();
        auto step = pattern.get (dragStep);
        if (! step.gate && std::abs (e.getDistanceFromDragStartY()) < 3)
            return;
        const int note = juce::jlimit (kMinStepNote, kMaxStepNote, dragStartNote - e.getDistanceFromDragStartY() / kPixelsPerSemitone);
        if (note != step.note)
        {
            dragged = true;
            step.note = note;
            step.gate = true; // dragging a rest sets a note
            pattern.set (dragStep, step);
            edited();
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragRow == Row::Bar && dragStep >= 0 && ! dragged)
        {
            auto& pattern = processor.getPatternBank().activePattern();
            auto step = pattern.get (dragStep);
            step.gate = ! step.gate;
            pattern.set (dragStep, step);
            edited();
        }
        dragStep = -1;
        dragRow = Row::None;
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (rowAt (e.y) != Row::Bar)
            return;
        auto& pattern = processor.getPatternBank().activePattern();
        auto step = pattern.get (stepAt (e.x));
        step.note = 0;
        step.gate = true;
        pattern.set (stepAt (e.x), step);
        edited();
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        if (rowAt (e.y) != Row::Bar)
            return;
        auto& pattern = processor.getPatternBank().activePattern();
        const int index = stepAt (e.x);
        auto step = pattern.get (index);
        const int delta = wheel.deltaY > 0.0f ? 1 : wheel.deltaY < 0.0f ? -1 : 0;
        if (delta == 0)
            return;
        step.note = juce::jlimit (kMinStepNote, kMaxStepNote, step.note + delta);
        step.gate = true;
        pattern.set (index, step);
        edited();
    }

private:
    enum class Row { Led, Bar, Note, Level, None };

    juce::Rectangle<float> cell (int step, Row row) const
    {
        const float columnWidth = (float) getWidth() / (float) kMaxSteps;
        const float x = (float) step * columnWidth + 2.0f, w = columnWidth - 4.0f;
        const float h = (float) getHeight();
        const float levelTop = h - (float) kLevelHeight;
        const float noteTop = levelTop - (float) (kGap + kNoteHeight);
        const float barTop = (float) (kLedHeight + kGap);
        switch (row)
        {
            case Row::Led: return { x, 0.0f, w, (float) kLedHeight };
            case Row::Bar: return { x, barTop, w, noteTop - (float) kGap - barTop };
            case Row::Note: return { x, noteTop, w, (float) kNoteHeight };
            case Row::Level: return { x, levelTop, w, (float) kLevelHeight };
            case Row::None: break;
        }
        return {};
    }

    int stepAt (int x) const { return juce::jlimit (0, kMaxSteps - 1, x * kMaxSteps / juce::jmax (1, getWidth())); }

    Row rowAt (int y) const
    {
        for (auto row : { Row::Bar, Row::Note, Row::Level })
        {
            const auto r = cell (0, row);
            if ((float) y >= r.getY() - 1.5f && (float) y < r.getBottom() + 1.5f)
                return row == Row::Note ? Row::Bar : row; // the note name belongs to the bar
        }
        return Row::None;
    }

    void edited()
    {
        processor.getPatternPresets().markEdited();
        repaint();
    }

    BleepForgeProcessor& processor;
    int playhead = -1, dragStep = -1, dragStartNote = 0;
    Row dragRow = Row::None;
    bool dragged = false;
};

//==============================================================================
// A rotary control for one of the playing pattern's groove settings, which belong to the
// pattern and are stored with it, so they are no parameters.
struct SequencerView::GrooveKnob : juce::Component
{
    GrooveKnob (const juce::String& name, double min, double max, double step, std::function<double()> read,
                std::function<void (double)> write, std::function<juce::String (double)> text, double defaultValue)
        : readValue (std::move (read))
    {
        label.setText (name, juce::dontSendNotification);
        label.setFont (fonts::label (14.0f));
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, theme::dimText);
        label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (label);

        slider.setRange (min, max, step);
        slider.setDoubleClickReturnValue (true, defaultValue);
        slider.textFromValueFunction = std::move (text);
        slider.updateText();
        slider.onValueChange = [this, write = std::move (write)] { if (! updating) write (slider.getValue()); };
        addAndMakeVisible (slider);
        refresh();
    }

    void refresh()
    {
        const double value = readValue();
        if (std::abs (slider.getValue() - value) > 1.0e-9)
        {
            updating = true;
            slider.setValue (value, juce::dontSendNotification);
            updating = false;
        }
    }

    void resized() override
    {
        auto area = getLocalBounds();
        label.setBounds (area.removeFromTop (Knob::kLabelHeight));
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, juce::jmax (20, getWidth() - 8), Knob::kValueHeight);
        const int diameter = juce::jmax (0, juce::jmin (area.getWidth(), area.getHeight() - Knob::kValueHeight, kDiameter));
        slider.setBounds (area.removeFromTop (diameter + Knob::kValueHeight));
    }

    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Label label;
    std::function<double()> readValue;
    bool updating = false;
};

//==============================================================================
// The MIDI button: a click writes the file, and on Windows a drag takes it into the host as a
// clip (as the host takes a file from Explorer).
struct SequencerView::MidiButton : juce::TextButton
{
    MidiButton() : juce::TextButton ("MIDI") {}

    std::function<juce::File()> writeFile;

    void mouseDrag (const juce::MouseEvent& e) override
    {
       #if JUCE_WINDOWS
        if (! dragStarted && e.getDistanceFromDragStart() > 4 && writeFile)
        {
            dragStarted = true;
            if (const auto file = writeFile(); file != juce::File())
                juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false);
            return;
        }
       #endif
        juce::TextButton::mouseDrag (e);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        const bool wasDrag = dragStarted;
        dragStarted = false;
        if (wasDrag)
            return; // the drag has done its work; no click
        juce::TextButton::mouseUp (e);
    }

    bool dragStarted = false;
};

//==============================================================================
SequencerView::SequencerView (BleepForgeProcessor& p)
    : processor (p), root (p.apvts, "seqRoot", "Root")
{
    auto& bank = processor.getPatternBank();
    auto activeGroove = [&bank] { return bank.activePattern().groove(); };

    for (int i = 0; i < kNumPatternSlots; ++i)
    {
        auto* button = slotButtons.add (new juce::TextButton (juce::String::charToString ((juce::juce_wchar) ('A' + i))));
        button->setConnectedEdges ((i > 0 ? juce::Button::ConnectedOnLeft : 0) | (i < kNumPatternSlots - 1 ? juce::Button::ConnectedOnRight : 0));
        button->setTooltip ("Pattern " + button->getButtonText() + ". A pattern you choose while it plays starts when the one playing starts over.");
        button->onClick = [this, i] { processor.choosePatternSlot (i); refresh(); };
        addAndMakeVisible (button);
    }

    patternButton.setTooltip ("Load a pattern into this slot, save it, or copy it to another slot");
    patternButton.onClick = [this] { showPatternMenu(); };
    addAndMakeVisible (patternButton);

    dice.onClick = [this] { processor.getPatternPresets().randomize(); refresh(); };
    addAndMakeVisible (dice);

    midiButton = std::make_unique<MidiButton>();
   #if JUCE_WINDOWS
    midiButton->setTooltip ("Write this pattern as a MIDI file (one pass), or drag it into your host to get a clip.");
   #else
    midiButton->setTooltip ("Write this pattern as a MIDI file (one pass).");
   #endif
    midiButton->writeFile = [this] { return processor.getPatternPresets().exportMidi (processor.getPatternBank().active()); };
    midiButton->onClick = [this] { exportMidi(); };
    addAndMakeVisible (*midiButton);

    grid = std::make_unique<StepGrid> (processor);
    addAndMakeVisible (*grid);

    auto& presets = processor.getPatternPresets();
    length = std::make_unique<GrooveKnob> ("Length", 1, kMaxSteps, 1, [activeGroove] { return (double) activeGroove().length; },
                                           [&bank, &presets] (double v) { bank.activePattern().setLength ((int) std::lround (v)); presets.markEdited(); },
                                           [] (double v) { return juce::String ((int) std::lround (v)); }, kMaxSteps);
    rate = std::make_unique<GrooveKnob> ("Rate", 0, kNumSeqRates - 1, 1, [activeGroove] { return (double) activeGroove().rate; },
                                         [&bank, &presets] (double v) { bank.activePattern().setRate ((int) std::lround (v)); presets.markEdited(); },
                                         [] (double v) { return juce::String (kSeqRateNames[juce::jlimit (0, kNumSeqRates - 1, (int) std::lround (v))]); }, 1);
    swing = std::make_unique<GrooveKnob> ("Swing", 0.5, 0.75, 0.005, [activeGroove] { return (double) activeGroove().swing; },
                                          [&bank, &presets] (double v) { bank.activePattern().setSwing ((float) v); presets.markEdited(); },
                                          [] (double v) { return juce::String (juce::roundToInt ((v - 0.5) * 400.0)) + " %"; }, 0.5);
    gate = std::make_unique<GrooveKnob> ("Gate", 0.05, 1.0, 0.01, [activeGroove] { return (double) activeGroove().gate; },
                                         [&bank, &presets] (double v) { bank.activePattern().setGate ((float) v); presets.markEdited(); },
                                         [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + " %"; }, 0.5);
    for (auto* knob : { length.get(), rate.get(), swing.get(), gate.get() })
        addAndMakeVisible (knob);

    root.setMaxDiameter (kDiameter);
    root.slider.setTooltip ("The note the pattern is played from. Keys you play while it runs set the root instead.");
    addAndMakeVisible (root);

    refresh();
    startTimerHz (30);
}

SequencerView::~SequencerView()
{
    stopTimer();
}

void SequencerView::timerCallback()
{
    grid->setPlayhead (processor.isSequencerRunning() ? processor.getSequencerStep() : -1);
    refresh();
}

// Follows what changes under the view: the slot that plays, a pattern loaded, the knobs.
void SequencerView::refresh()
{
    auto& bank = processor.getPatternBank();
    for (int i = 0; i < slotButtons.size(); ++i)
    {
        slotButtons[i]->setToggleState (i == bank.active(), juce::dontSendNotification);
        slotButtons[i]->setColour (juce::TextButton::textColourOffId, i == bank.pending() ? theme::primary() : theme::text);
    }
    const auto name = processor.getPatternPresets().name();
    if (patternButton.getButtonText() != name)
        patternButton.setButtonText (name);
    for (auto* knob : { length.get(), rate.get(), swing.get(), gate.get() })
        knob->refresh();
    grid->repaint();
}

void SequencerView::paint (juce::Graphics& g)
{
    theme::paintPanel (g, getLocalBounds().toFloat());
    g.setColour (theme::primary().withAlpha (0.8f));
    g.setFont (fonts::title (14.0f));
    g.drawText ("SEQUENCER", juce::Rectangle<float> ((float) kPadding, 6.0f, 120.0f, 14.0f), juce::Justification::centredLeft);
    // the divider between the steps and the groove
    theme::paintDivider (g, (float) getWidth() - (float) (kPadding + 5 * kKnobWidth + 8), (float) kTopRow, (float) getHeight() - (float) kTopRow - (float) kPadding);
}

void SequencerView::resized()
{
    auto area = getLocalBounds().reduced (kPadding);

    // the knobs on the right, the steps filling the rest
    auto right = area.removeFromRight (5 * kKnobWidth);
    area.removeFromRight (16);
    right.removeFromTop (kGridTop - kPadding - 6);
    for (auto* component : std::initializer_list<juce::Component*> { length.get(), rate.get(), swing.get(), gate.get(), &root })
        component->setBounds (right.removeFromLeft (kKnobWidth));

    // the row above the steps: the slots, the pattern's name, the dice and the MIDI button
    auto top = juce::Rectangle<int> (kPadding, kTopRow, area.getWidth(), 22);
    for (auto* button : slotButtons)
        button->setBounds (top.removeFromLeft (kSlotWidth));
    top.removeFromLeft (10);
    midiButton->setBounds (top.removeFromRight (48));
    top.removeFromRight (6);
    dice.setBounds (top.removeFromRight (22).withSizeKeepingCentre (22, 22));
    top.removeFromRight (6);
    patternButton.setBounds (top);

    grid->setBounds (area.getX(), kGridTop, area.getWidth(), getHeight() - kGridTop - kPadding);
}

// The factory patterns, then the user's, with saving, copying and clearing below.
void SequencerView::showPatternMenu()
{
    auto& presets = processor.getPatternPresets();
    juce::PopupMenu menu;
    for (int i = 0; i < kNumFactoryPatterns; ++i)
        menu.addItem (presets.entryName (i), [this, i] { processor.getPatternPresets().load (i); refresh(); });

    if (presets.numEntries() > kNumFactoryPatterns)
    {
        juce::PopupMenu user;
        for (int i = kNumFactoryPatterns; i < presets.numEntries(); ++i)
            user.addItem (presets.entryName (i), [this, i] { processor.getPatternPresets().load (i); refresh(); });
        menu.addSubMenu ("My patterns", user);
    }

    menu.addSeparator();
    menu.addItem ("Save pattern...", [this] { if (onSave) onSave(); });
    juce::PopupMenu copy;
    const int active = processor.getPatternBank().active();
    for (int i = 0; i < kNumPatternSlots; ++i)
        if (i != active)
            copy.addItem ("To " + juce::String::charToString ((juce::juce_wchar) ('A' + i)), [this, active, i] { processor.getPatternPresets().copySlot (active, i); refresh(); });
    menu.addSubMenu ("Copy to slot", copy);
    menu.addItem ("Clear pattern", [this] { processor.getPatternPresets().clear(); refresh(); });
    const int entry = presets.current();
    if (presets.isUser (entry))
        menu.addItem ("Delete \"" + presets.entryName (entry) + "\"", [this, entry] { if (onDelete) onDelete (entry); });
    menu.addItem ("Open pattern folder", []
                  {
                      auto folder = BleepForgeProcessor::patternFolder();
                      folder.createDirectory();
                      files::openFolder (folder);
                  });
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (patternButton).withMinimumWidth (patternButton.getWidth()).withDeletionCheck (*this));
}

void SequencerView::exportMidi()
{
    const auto file = processor.getPatternPresets().exportMidi (processor.getPatternBank().active());
    if (file == juce::File())
    {
        juce::AlertWindow::showAsync (juce::MessageBoxOptions::makeOptionsOk (juce::MessageBoxIconType::WarningIcon, "Could not export",
                                                                              "Writing to " + processor.getPatternPresets().midiFolder().getFullPathName() + " failed."),
                                      nullptr);
        return;
    }
    juce::AlertWindow::showAsync (juce::MessageBoxOptions::makeOptionsOkCancel (juce::MessageBoxIconType::InfoIcon, "MIDI exported",
                                                                                "Saved " + file.getFileName() + " in " + file.getParentDirectory().getFullPathName() + ".",
                                                                                "Open folder", "OK"),
                                  juce::ModalCallbackFunction::create ([folder = file.getParentDirectory()] (int result)
                                                                       { if (result == 1) files::openFolder (folder); }));
}

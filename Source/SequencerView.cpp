// Copyright (C) 2026 Henrik Juhl. Part of BleepWerk, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#include "SequencerView.h"
#include "PresetFiles.h"
#include "Theme.h"
#include "Fonts.h"

using namespace bleepwerk;

namespace
{
constexpr int kPadding = 10, kTopRow = 24, kGridTop = 66, kSlotWidth = 26, kKnobWidth = 46, kDiameter = 34, kNumKnobs = 7, kRowGap = 6, kShapeButtonHeight = 20;
constexpr int kLedHeight = 10, kNoteHeight = 14, kOctaveHeight = 12, kGap = 3;
constexpr int kPixelsPerSemitone = 4;
} // namespace

//==============================================================================
// The sixteen steps.
class SequencerView::StepGrid : public juce::Component
{
public:
    explicit StepGrid (BleepWerkProcessor& p) : processor (p) { setWantsKeyboardFocus (false); }

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
        const int patternLength = pattern.groove().length; // not "length" and "root", the view's own members
        // the note a step plays: the shared root, this pattern's transpose and the step's own offset
        // (a key held while the sequencer plays takes over the shared root, and the notes shown follow it)
        const int heldRoot = processor.getExternalRoot();
        const int rootNote = (heldRoot >= 0 ? heldRoot : (int) processor.apvts.getRawParameterValue ("seqRoot")->load()) + pattern.groove().transpose;
        constexpr float noteSpan = (float) (kMaxStepNote - kMinStepNote);

        for (int i = 0; i < kMaxSteps; ++i)
        {
            const auto step = pattern.get (i);
            const bool inPattern = i < patternLength, playing = i == playhead;
            const float fade = inPattern ? 1.0f : 0.35f;

            // the accent: "ACC" in the bright colour, or the same text faint where there is none
            // (a circle is the pattern's play mark, so it means one thing only)
            const bool accented = step.level == kAccentLevel;
            g.setFont (fonts::title (9.0f));
            g.setColour (accented ? theme::primary().withMultipliedAlpha (fade) : theme::dimText.withMultipliedAlpha (fade * 0.45f));
            g.drawText ("ACC", cell (i, Row::Accent), juce::Justification::centred, false);

            // the bar box: a plain outline, brighter while the step plays
            const auto box = cell (i, Row::Bar);
            g.setColour (theme::background);
            g.fillRect (box);
            g.setColour (playing ? theme::primary() : theme::outline);
            g.drawRect (box, playing ? 2.0f : 1.0f);
            const auto inner = box.reduced (4.0f);
            if (step.gate)
            {
                // the higher the note, the taller the bar; an accent is brighter
                const float height = 0.08f + 0.92f * (float) (step.note - kMinStepNote) / noteSpan;
                const auto bar = inner.withTop (inner.getBottom() - juce::jmax (3.0f, inner.getHeight() * height));
                g.setColour (theme::primary().withAlpha ((accented ? 1.0f : 0.6f) * fade));
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
            g.drawText (step.gate ? juce::MidiMessage::getMidiNoteName (juce::jlimit (0, 127, rootNote + step.note), true, true, 3) : juce::String ("-"),
                        cell (i, Row::Note), juce::Justification::centred);

            // the octave arrows: up on the left, down on the right
            const auto octave = cell (i, Row::Octave);
            auto arrow = [&] (juce::Rectangle<float> r, bool up, bool possible)
            {
                juce::Path triangle;
                const auto c = r.getCentre();
                triangle.addTriangle (c.x - 3.5f, up ? c.y + 2.5f : c.y - 2.5f, c.x + 3.5f, up ? c.y + 2.5f : c.y - 2.5f, c.x, up ? c.y - 3.0f : c.y + 3.0f);
                g.setColour (theme::dimText.withMultipliedAlpha (fade * (possible ? 1.0f : 0.35f)));
                g.fillPath (triangle);
            };
            arrow (octave.withWidth (octave.getWidth() * 0.5f), true, step.note + 12 <= kMaxStepNote);
            arrow (octave.withTrimmedLeft (octave.getWidth() * 0.5f), false, step.note - 12 >= kMinStepNote);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragStep = stepAt (e.x);
        dragRow = rowAt (e.y);
        dragged = false;
        auto& pattern = processor.getPatternBank().activePattern();
        dragStartNote = pattern.get (dragStep).note;
        auto step = pattern.get (dragStep);
        if (dragRow == Row::Accent)
        {
            step.level = step.level == kAccentLevel ? kNormalLevel : kAccentLevel;
            pattern.set (dragStep, step);
            edited();
        }
        else if (dragRow == Row::Octave)
        {
            // the left half moves the note up an octave, the right half down; a rest comes on
            const bool up = e.x - dragStep * getWidth() / kMaxSteps < getWidth() / kMaxSteps / 2;
            step.note = juce::jlimit (kMinStepNote, kMaxStepNote, step.note + (up ? 12 : -12));
            step.gate = true;
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
    enum class Row { Accent, Bar, Note, Octave, None };

    juce::Rectangle<float> cell (int step, Row row) const
    {
        const float columnWidth = (float) getWidth() / (float) kMaxSteps;
        const float x = (float) step * columnWidth + 2.0f, w = columnWidth - 4.0f;
        const float h = (float) getHeight();
        const float octaveTop = h - (float) kOctaveHeight;
        const float noteTop = octaveTop - (float) kNoteHeight;
        const float barTop = (float) (kLedHeight + kGap);
        switch (row)
        {
            case Row::Accent: return { x, 0.0f, w, (float) kLedHeight };
            case Row::Bar: return { x, barTop, w, noteTop - (float) kGap - barTop };
            case Row::Note: return { x, noteTop, w, (float) kNoteHeight };
            case Row::Octave: return { x, octaveTop, w, (float) kOctaveHeight };
            case Row::None: break;
        }
        return {};
    }

    int stepAt (int x) const { return juce::jlimit (0, kMaxSteps - 1, x * kMaxSteps / juce::jmax (1, getWidth())); }

    Row rowAt (int y) const
    {
        for (auto row : { Row::Accent, Row::Bar, Row::Note, Row::Octave })
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

    BleepWerkProcessor& processor;
    int playhead = -1, dragStep = -1, dragStartNote = 0;
    Row dragRow = Row::None;
    bool dragged = false;
};

//==============================================================================
// A rotary control for one of the playing pattern's groove settings, which belong to the
// pattern and are stored with it, so they are no parameters.
struct SequencerView::GrooveKnob : juce::Component, juce::SettableTooltipClient
{
    // the slider is what the mouse is over, so it needs the text too
    void setTooltip (const juce::String& text) { SettableTooltipClient::setTooltip (text); slider.setTooltip (text); }

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
// The MIDI button: a click opens its menu, and on Windows a drag writes the file of the last
// choice and takes it into the host as a clip (as the host takes a file from Explorer).
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
// The tick under a pattern's letter: bright while the pattern plays as a layer, faint when not.
struct SequencerView::PlayDot : juce::Button
{
    PlayDot() : juce::Button ("Plays") {}
    void paintButton (juce::Graphics& g, bool highlighted, bool) override
    {
        const auto c = getLocalBounds().toFloat().getCentre();
        juce::Path tick;
        tick.startNewSubPath (c.x - 4.0f, c.y + 0.5f);
        tick.lineTo (c.x - 1.5f, c.y + 3.5f);
        tick.lineTo (c.x + 4.5f, c.y - 3.5f);
        const bool on = getToggleState();
        g.setColour (on ? theme::primary() : (highlighted ? theme::text : theme::dimText.withAlpha (0.5f)));
        g.strokePath (tick, juce::PathStrokeType (on ? 2.0f : 1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
};

//==============================================================================
SequencerView::SequencerView (BleepWerkProcessor& p)
    : processor (p), root (p.apvts, "seqRoot", "Root")
{
    auto& bank = processor.getPatternBank();
    auto activeGroove = [&bank] { return bank.activePattern().groove(); };

    for (int i = 0; i < kNumPatternSlots; ++i)
    {
        auto* button = slotButtons.add (new juce::TextButton (juce::String::charToString ((juce::juce_wchar) ('A' + i))));
        button->setConnectedEdges ((i > 0 ? juce::Button::ConnectedOnLeft : 0) | (i < kNumPatternSlots - 1 ? juce::Button::ConnectedOnRight : 0));
        button->setTooltip ("Show and edit pattern " + button->getButtonText());
        button->onClick = [this, i] { processor.editPatternSlot (i); refresh(); };
        addAndMakeVisible (button);

        auto* playDot = playDots.add (new PlayDot());
        playDot->setTooltip ("Pattern " + button->getButtonText() + " plays. Any number can play at once, each with its own length.");
        playDot->onClick = [this, i] { processor.getPatternBank().setPlaying (i, ! processor.getPatternBank().isPlaying (i)); refresh(); };
        addAndMakeVisible (playDot);
    }

    patternButton.setTooltip ("Load a pattern into this slot, save it, or copy it to another slot");
    patternButton.onClick = [this] { showPatternMenu(); };
    addAndMakeVisible (patternButton);

    dice.onClick = [this] { processor.getPatternPresets().randomize(); refresh(); };
    addAndMakeVisible (dice);

    clearButton.getProperties().set ("smallFont", true);
    clearButton.setTooltip ("Clear the notes of all eight patterns (and switch all layers off) or only of the shown one; you are asked which. Length, rate and the other settings stay, and so do Root and the sound.");
    clearButton.onClick = [this] { if (onClearAll) onClearAll(); };
    addAndMakeVisible (clearButton);

    midiButton = std::make_unique<MidiButton>();
    midiButton->getProperties().set ("smallFont", true);
    midiButton->writeFile = [this] { return writeMidi(); };
    midiButton->onClick = [this] { showMidiMenu(); };
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
    drift = std::make_unique<GrooveKnob> ("Drift", -kMaxDrift, kMaxDrift, 1, [activeGroove] { return (double) activeGroove().drift; },
                                          [&bank, &presets] (double v) { bank.activePattern().setDrift ((int) std::lround (v)); presets.markEdited(); },
                                          [] (double v) { const int d = (int) std::lround (v); return d > 0 ? "+" + juce::String (d) : juce::String (d); }, 0);
    hits = std::make_unique<GrooveKnob> ("Hits", 0, kMaxSteps, 1, [&bank] { return (double) bank.activePattern().hits(); },
                                         [&bank, &presets] (double v) { bank.activePattern().setHits ((int) std::lround (v)); presets.markEdited(); },
                                         [] (double v) { return juce::String ((int) std::lround (v)); }, 8);
    vary = std::make_unique<GrooveKnob> ("Vary", 0, kMaxSteps - 1, 1, [activeGroove] { return (double) activeGroove().vary; },
                                         [&bank, &presets] (double v) { bank.activePattern().setVary ((int) std::lround (v)); presets.markEdited(); },
                                         [] (double v) { const int n = (int) std::lround (v); return n == 0 ? juce::String ("Off") : "-" + juce::String (n); }, 0);
    transpose = std::make_unique<GrooveKnob> ("Transpose", -kMaxTranspose, kMaxTranspose, 1, [activeGroove] { return (double) activeGroove().transpose; },
                                              [&bank, &presets] (double v) { bank.activePattern().setTranspose ((int) std::lround (v)); presets.markEdited(); },
                                              [] (double v) { const int t = (int) std::lround (v); return t > 0 ? "+" + juce::String (t) : juce::String (t); }, 0);
    transpose->setTooltip ("Moves this pattern up or down from the shared Root, in semitones. Only this pattern: Root and the keys you hold move all layers together");
    length->setTooltip ("Number of steps in the pattern, up to 16");
    rate->setTooltip ("Length of each step: 1/8, 1/16 or 1/32 note, or the triplet 1/8T or 1/16T");
    swing->setTooltip ("Delays every second step: 0.5 is straight, higher swings more");
    gate->setTooltip ("How long each note sounds, as a share of its step");
    vary->setTooltip ("Lets the length change from pass to pass, by up to this many steps shorter, so the pattern never settles");
    shapeButton.setTooltip ("How the length changes: Walk steps down and up, Random follows a fixed series, Skip plays one short pass in three");
    shapeButton.onClick = [this, &bank, &presets]
    {
        auto& pattern = bank.activePattern();
        pattern.setShape ((pattern.groove().shape + 1) % kNumLengthShapes);
        presets.markEdited();
        refresh();
    };
    addAndMakeVisible (shapeButton);
    drift->setTooltip ("Every time the pattern has played through, it moves on this many steps");
    hits->setTooltip ("Spreads this many hits evenly over the pattern's length, keeping the notes you have");
    for (auto* knob : { length.get(), rate.get(), swing.get(), gate.get(), drift.get(), hits.get(), vary.get(), transpose.get() })
        addAndMakeVisible (knob);

    root.setMaxDiameter (kDiameter);
    root.label.setText (juce::String (juce::CharPointer_UTF8 ("Root \xc2\xb7 all")), juce::dontSendNotification); // unlike the knobs beside it, it moves every layer
    root.slider.setTooltip ("The note every layer is played from. While the sequencer plays, a key you hold plays from that note instead and is shown here as MIDI; "
                            "let go and this value counts again. Transpose moves a single pattern from it.");
    // Held keys are only shown: the knob keeps its own value, and under it stands the note that
    // plays while a key has taken over. The text function below reads externalRoot.
    {
        const auto fromValue = root.slider.textFromValueFunction;
        root.slider.textFromValueFunction = [this, fromValue] (double v)
        {
            return externalRoot >= 0 ? juce::MidiMessage::getMidiNoteName (juce::jlimit (0, 127, externalRoot), true, true, 3) : fromValue (v);
        };
    }
    addAndMakeVisible (root);

    refresh();
    showExternalRoot (processor.isSequencerRunning() ? processor.getExternalRoot() : -1);
    startTimerHz (30);
}

SequencerView::~SequencerView()
{
    stopTimer();
}

// Root's marking while a key from the host has taken over: its own name above the knob, the note
// that plays under it, both in the bright colour.
void SequencerView::showExternalRoot (int note)
{
    if (note == externalRoot)
        return;
    externalRoot = note;
    const bool external = note >= 0;
    root.label.setText (juce::String (juce::CharPointer_UTF8 (external ? "MIDI \xc2\xb7 Root" : "Root \xc2\xb7 all")), juce::dontSendNotification);
    root.label.setColour (juce::Label::textColourId, external ? theme::primary() : theme::dimText);
    root.slider.setColour (juce::Slider::textBoxTextColourId, external ? theme::primary() : theme::text);
    root.slider.updateText();
    grid->repaint();
}

void SequencerView::timerCallback()
{
    showExternalRoot (processor.isSequencerRunning() ? processor.getExternalRoot() : -1);
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
    }
    const auto name = processor.getPatternPresets().name();
    if (patternButton.getButtonText() != name)
        patternButton.setButtonText (name);
    for (auto* knob : { length.get(), rate.get(), swing.get(), gate.get(), drift.get(), hits.get(), vary.get(), transpose.get() })
        knob->refresh();
    const auto shapeName = juce::String (kLengthShapeNames[juce::jlimit (0, kNumLengthShapes - 1, bank.activePattern().groove().shape)]);
    if (shapeButton.getButtonText() != shapeName)
        shapeButton.setButtonText (shapeName);
    // dimmed rather than disabled: a disabled button gets no mouse events, so its tooltip never shows
    shapeButton.setAlpha (bank.activePattern().groove().vary > 0 ? 1.0f : 0.4f);
    for (int i = 0; i < playDots.size(); ++i)
        playDots[i]->setToggleState (bank.isPlaying (i), juce::dontSendNotification);
    // the MIDI button shows what a drag into the host (or a click on the menu) writes
    const auto midiText = exportBars == 0 ? juce::String ("MIDI") : "MIDI " + juce::String (exportBars);
    if (midiButton->getButtonText() != midiText)
        midiButton->setButtonText (midiText);
    const auto what = exportBars == 0 ? juce::String ("the shown pattern for one pass")
                                      : "everything the playing layers play over " + juce::String (exportBars) + " bars";
   #if JUCE_WINDOWS
    midiButton->setTooltip ("Write a MIDI file: click for the choices, or drag the button into your host to get a clip of " + what + ".");
   #else
    midiButton->setTooltip ("Write a MIDI file: click for the choices. Now: " + what + ".");
   #endif
    grid->repaint();
}

void SequencerView::paint (juce::Graphics& g)
{
    theme::paintPanel (g, getLocalBounds().toFloat());
    g.setColour (theme::primary().withAlpha (0.8f));
    g.setFont (fonts::title (14.0f));
    g.drawText ("SEQUENCER", juce::Rectangle<float> ((float) kPadding, 6.0f, 120.0f, 14.0f), juce::Justification::centredLeft);
    // the divider between the steps and the groove
    theme::paintDivider (g, (float) getWidth() - (float) (kPadding + kNumKnobs * kKnobWidth + 8), (float) kTopRow, (float) getHeight() - (float) kTopRow - (float) kPadding);
    // and the one that sets Root, which moves every layer, apart from the knobs of the shown pattern
    const auto rootArea = rootBounds();
    theme::paintDivider (g, (float) rootArea.getX() - 6.0f, (float) rootArea.getY(), (float) rootArea.getHeight());
}

// Root's place: the last two columns of the second row, with room for its label.
juce::Rectangle<int> SequencerView::rootBounds() const
{
    const int y = kGridTop - 6 + Knob::heightFor (kDiameter) + 4 + kShapeButtonHeight + kRowGap;
    return { getWidth() - kPadding - 2 * kKnobWidth, y, 2 * kKnobWidth, Knob::heightFor (kDiameter) };
}

void SequencerView::resized()
{
    auto area = getLocalBounds().reduced (kPadding);

    // the knobs on the right, the steps filling the rest: the shown pattern's settings in two rows,
    // Root on its own in the second, apart from them
    auto right = area.removeFromRight (kNumKnobs * kKnobWidth);
    area.removeFromRight (16);
    right.removeFromTop (kGridTop - kPadding - 6);
    auto firstRow = right.removeFromTop (Knob::heightFor (kDiameter) + 4 + kShapeButtonHeight);
    for (auto* component : std::initializer_list<juce::Component*> { length.get(), rate.get(), swing.get(), gate.get(), drift.get(), hits.get(), vary.get() })
    {
        auto column = firstRow.removeFromLeft (kKnobWidth);
        component->setBounds (column.removeFromTop (Knob::heightFor (kDiameter)));
        if (component == vary.get())
            shapeButton.setBounds (column.withTrimmedTop (4).withHeight (kShapeButtonHeight));
    }
    right.removeFromTop (kRowGap);
    transpose->setBounds (right.removeFromLeft (2 * kKnobWidth).removeFromTop (Knob::heightFor (kDiameter))); // two columns wide, for its name
    root.setBounds (rootBounds());

    // the row above the steps: the slots, the pattern's name, the dice and the MIDI button
    auto top = juce::Rectangle<int> (kPadding, kTopRow, area.getWidth(), 22);
    for (int i = 0; i < slotButtons.size(); ++i)
    {
        slotButtons[i]->setBounds (top.getX(), top.getY(), kSlotWidth, top.getHeight());
        playDots[i]->setBounds (top.getX(), top.getBottom() + 1, kSlotWidth, 10);
        top.removeFromLeft (kSlotWidth);
    }
    top.removeFromLeft (10);
    // MIDI and CLEAR: two identical small buttons one above the other, the lower ending above the steps
    constexpr int kSmallButtonHeight = 18;
    const auto midiBounds = top.removeFromRight (62);
    midiButton->setBounds (midiBounds.withHeight (kSmallButtonHeight));
    clearButton.setBounds (midiBounds.getX(), midiBounds.getY() + kSmallButtonHeight + 2, midiBounds.getWidth(), kSmallButtonHeight);
    top.removeFromRight (6);
    dice.setBounds (top.removeFromRight (22).withSizeKeepingCentre (22, 22));
    top.removeFromRight (6);
    patternButton.setBounds (top);

    grid->setBounds (area.getX(), kGridTop, area.getWidth(), getHeight() - kGridTop - kPadding);
}

// Loads a pattern or a set into the shown slot; a set that doesn't fit says why instead.
void SequencerView::loadEntry (int entry)
{
    const auto problem = processor.getPatternPresets().load (entry);
    refresh();
    if (problem.isNotEmpty())
        juce::AlertWindow::showAsync (juce::MessageBoxOptions::makeOptionsOk (juce::MessageBoxIconType::InfoIcon, "Not enough room for the set", problem), nullptr);
}

// The factory patterns, then the user's, with saving, copying and clearing below.
void SequencerView::showPatternMenu()
{
    auto& presets = processor.getPatternPresets();
    juce::PopupMenu menu;
    for (int i = 0; i < kFirstPolymeterPattern; ++i)
        menu.addItem (presets.entryName (i), [this, i] { loadEntry (i); });

    juce::PopupMenu solo;
    for (int i = kFirstPolymeterPattern; i < kNumFactoryPatterns; ++i)
        solo.addItem (presets.entryName (i), [this, i] { loadEntry (i); });
    menu.addSubMenu ("Polymeter, one layer", solo);

    juce::PopupMenu sets;
    for (int i = kNumFactoryPatterns; i < kNumFactoryEntries; ++i)
        sets.addItem (presets.entryName (i), [this, i] { loadEntry (i); });
    menu.addSubMenu ("Polymeter sets (layers)", sets);

    if (presets.numEntries() > kNumFactoryEntries)
    {
        juce::PopupMenu user;
        for (int i = kNumFactoryEntries; i < presets.numEntries(); ++i)
            user.addItem (presets.entryName (i), [this, i] { loadEntry (i); });
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
                      auto folder = BleepWerkProcessor::patternFolder();
                      folder.createDirectory();
                      files::openFolder (folder);
                  });
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (patternButton).withMinimumWidth (patternButton.getWidth()).withDeletionCheck (*this));
}

// What the MIDI button writes, for the drag into the host: the last choice from its menu.
juce::File SequencerView::writeMidi() const
{
    auto& presets = processor.getPatternPresets();
    return exportBars == 0 ? presets.exportMidi (processor.getPatternBank().active()) : presets.exportLayersMidi (exportBars);
}

void SequencerView::showMidiMenu()
{
    juce::PopupMenu menu;
    menu.addItem ("This pattern, one pass", true, exportBars == 0, [this] { exportMidi (0); });
    menu.addSeparator();
    menu.addSectionHeader ("All playing layers, as they play");
    for (const int bars : kExportBars)
        menu.addItem (juce::String (bars) + " bars", true, exportBars == bars, [this, bars] { exportMidi (bars); });
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (*midiButton).withMinimumWidth (160).withDeletionCheck (*this));
}

void SequencerView::exportMidi (int bars)
{
    exportBars = bars;
    refresh(); // the button shows the choice

    auto& presets = processor.getPatternPresets();
    if (bars == 0)
    {
        showExported (presets.exportMidi (processor.getPatternBank().active()), false);
        return;
    }

    const int cycle = presets.barsUntilRepeat();
    if (cycle == 0)
    {
        juce::AlertWindow::showAsync (juce::MessageBoxOptions::makeOptionsOk (juce::MessageBoxIconType::InfoIcon, "Nothing to export",
                                                                              "No pattern that plays has a note to play. Switch a layer on with the tick under its letter."),
                                      nullptr);
        return;
    }

    // the layers repeat exactly after fewer bars than asked for: say so, and offer the shorter clip
    if (cycle > 0 && cycle < bars)
    {
        const auto shorter = juce::String (cycle) + (cycle == 1 ? " bar" : " bars");
        const auto asked = juce::String (bars) + " bars";
        juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                          .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                          .withTitle ("The layers repeat")
                                          .withMessage ("Your layers play exactly the same again after " + shorter + ", so " + asked
                                                        + " would only be that over and over. Export " + shorter + " instead?")
                                          .withButton ("Export " + shorter)
                                          .withButton ("Export " + asked)
                                          .withButton ("Cancel"),
                                      juce::ModalCallbackFunction::create (
                                          [safe = juce::Component::SafePointer<SequencerView> (this), bars] (int result)
                                          {
                                              if (safe == nullptr || result == 0)
                                                  return; // cancelled, or the panel is gone
                                              auto& exporter = safe->processor.getPatternPresets();
                                              safe->showExported (exporter.exportLayersMidi (result == 1 ? exporter.barsUntilRepeat() : bars), true);
                                          }));
        return;
    }
    showExported (presets.exportLayersMidi (bars), true);
}

void SequencerView::showExported (const juce::File& file, bool layers)
{
    if (file == juce::File())
    {
        juce::AlertWindow::showAsync (juce::MessageBoxOptions::makeOptionsOk (juce::MessageBoxIconType::WarningIcon, "Could not export",
                                                                              "Writing to " + processor.getPatternPresets().midiFolder().getFullPathName() + " failed."),
                                      nullptr);
        return;
    }
    juce::String message = "Saved " + file.getFileName() + " in " + file.getParentDirectory().getFullPathName() + ".";
    if (layers)
        message << "\n\nThe notes are played from the Root knob, not from keys you hold.";
    message << "\n\nSwitch SEQ off when you play the file from the host, or the sequencer plays along with it.";
    juce::AlertWindow::showAsync (juce::MessageBoxOptions::makeOptionsOkCancel (juce::MessageBoxIconType::InfoIcon, "MIDI exported", message,
                                                                                "Open folder", "OK"),
                                  juce::ModalCallbackFunction::create ([folder = file.getParentDirectory()] (int result)
                                                                       { if (result == 1) files::openFolder (folder); }));
}

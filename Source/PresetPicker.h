// Copyright (C) 2026 Henrik Juhl. Part of BleepWerk, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SoundPresets.h"
#include "Theme.h"
#include "Fonts.h"

// The sound preset list: a search field over one flat list of every preset by name, the user's
// last. Typing narrows the list to the names that contain what was typed (every word of it, in
// any order); the arrow keys move in it, Return or a click loads the preset, Escape closes. Shown
// in a call-out under the preset button. Message thread only.
class PresetPicker : public juce::Component, private juce::ListBoxModel
{
public:
    explicit PresetPicker (SoundPresets& p) : presets (p)
    {
        search.setTextToShowWhenEmpty ("Search presets...", bleepwerk::theme::dimText);
        search.setFont (bleepwerk::fonts::label (15.0f));
        search.setIndents (8, 6);
        search.setColour (juce::TextEditor::backgroundColourId, bleepwerk::theme::background);
        search.setColour (juce::TextEditor::outlineColourId, bleepwerk::theme::outline);
        search.setColour (juce::TextEditor::focusedOutlineColourId, bleepwerk::theme::primary());
        search.setColour (juce::TextEditor::textColourId, bleepwerk::theme::text);
        search.onTextChange = [this] { rebuild(); };
        search.onEscapeKey = [this] { close(); };
        search.onReturnKey = [this] { chooseSelected(); };
        search.onArrow = [this] (int delta) { moveSelection (delta); };
        addAndMakeVisible (search);

        list.setModel (this);
        list.setRowHeight (24);
        list.setColour (juce::ListBox::backgroundColourId, bleepwerk::theme::background);
        list.setColour (juce::ListBox::outlineColourId, bleepwerk::theme::outline);
        list.setOutlineThickness (1);
        addAndMakeVisible (list);

        rebuild();
        if (const int row = rows.indexOf (presets.current()); row >= 0)
        {
            list.selectRow (row);
            list.scrollToEnsureRowIsOnscreen (row);
        }
        setSize (260, 360);
    }

    // the search field takes the keys from the moment the list is up
    void parentHierarchyChanged() override
    {
        if (getParentComponent() != nullptr)
            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<PresetPicker> (this)] { if (safe != nullptr) safe->search.grabKeyboardFocus(); });
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (6);
        search.setBounds (area.removeFromTop (28));
        area.removeFromTop (6);
        list.setBounds (area);
    }

    void setSearchText (const juce::String& text) { search.setText (text, false); rebuild(); }

    // the entries whose names hold every word of the text, in the list's order; all of them for no text
    static juce::Array<int> matching (const SoundPresets& presets, const juce::String& text)
    {
        const auto words = juce::StringArray::fromTokens (text.trim(), " ", "");
        juce::Array<int> found;
        for (int i = 0; i < presets.numEntries(); ++i)
        {
            const auto name = presets.entryName (i);
            bool all = true;
            for (const auto& word : words)
                all = all && name.containsIgnoreCase (word);
            if (all)
                found.add (i);
        }
        return found;
    }

private:
    // the search field, which also takes up and down for the list
    struct SearchField : juce::TextEditor
    {
        bool keyPressed (const juce::KeyPress& key) override
        {
            if (onArrow && (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey))
            {
                onArrow (key == juce::KeyPress::upKey ? -1 : 1);
                return true;
            }
            return juce::TextEditor::keyPressed (key);
        }
        std::function<void (int)> onArrow;
    };

    void rebuild()
    {
        rows = matching (presets, search.getText());
        list.updateContent();
        list.deselectAllRows();
        if (rows.isEmpty())
        {
            list.repaint();
            return;
        }
        const int current = rows.indexOf (presets.current());
        list.selectRow (search.isEmpty() && current >= 0 ? current : 0); // Return takes the first match
        list.scrollToEnsureRowIsOnscreen (list.getSelectedRow());
    }

    void moveSelection (int delta)
    {
        if (rows.isEmpty())
            return;
        const int row = juce::jlimit (0, rows.size() - 1, list.getSelectedRow() + delta);
        list.selectRow (row);
        list.scrollToEnsureRowIsOnscreen (row);
    }

    void chooseSelected()
    {
        if (juce::isPositiveAndBelow (list.getSelectedRow(), rows.size()))
            choose (rows[list.getSelectedRow()]);
    }

    void close()
    {
        if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
            box->dismiss();
    }

    void choose (int entry)
    {
        presets.load (entry);
        close();
    }

    int getNumRows() override { return rows.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (! juce::isPositiveAndBelow (row, rows.size()))
            return;
        const int entry = rows[row];
        if (selected)
        {
            g.setColour (bleepwerk::theme::outline);
            g.fillRect (0, 0, width, height);
        }
        g.setFont (bleepwerk::fonts::label (15.0f));
        g.setColour (entry == presets.current() ? bleepwerk::theme::primary() : bleepwerk::theme::text); // the loaded one in the bright colour
        g.drawText (presets.entryName (entry), 10, 0, width - 14, height, juce::Justification::centredLeft, true);
    }

    void listBoxItemClicked (int row, const juce::MouseEvent&) override
    {
        if (juce::isPositiveAndBelow (row, rows.size()))
            choose (rows[row]);
    }

    SoundPresets& presets;
    SearchField search;
    juce::ListBox list;
    juce::Array<int> rows; // the entry shown on each row
};

// Copyright (C) 2026 Henrik Juhl. Part of BleepWerk, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#include "PatternPresets.h"
#include "PresetFiles.h"

using namespace bleepwerk;

PatternPresets::PatternPresets (juce::AudioProcessorValueTreeState& s, PatternBank& b, std::function<juce::File()> folderProvider,
                                std::function<void()> callback)
    : state (s), bank (b), getFolder (std::move (folderProvider)), onChange (std::move (callback))
{
    rescan();
}

juce::String PatternPresets::entryName (int index) const
{
    if (isUser (index))
        return userFiles[index - kNumFactoryEntries].getFileNameWithoutExtension();
    if (juce::isPositiveAndBelow (index, kNumFactoryPatterns))
        return kFactoryPatterns[index].name;
    return isSet (index) ? kFactorySets[index - kNumFactoryPatterns].name : "";
}

int PatternPresets::rateIndexFor (const juce::String& rateName)
{
    for (int i = 0; i < kNumSeqRates; ++i)
        if (rateName == kSeqRateNames[i])
            return i;
    return -1;
}

// Into the slot that plays; the processor shows it in the sequencer parameters.
void PatternPresets::applyGroove (Pattern& target, const LoadedGroove& loaded)
{
    target.setLength (loaded.length);
    target.setSwing (loaded.swing);
    if (loaded.rateIndex >= 0)
        target.setRate (loaded.rateIndex);
    if (loaded.gate >= 0.0f)
        target.setGate (loaded.gate);
    target.setDrift (loaded.drift);
    target.setVary (loaded.vary);
    target.setShape (loaded.shape);
    target.setTranspose (loaded.transpose);
}

bool PatternPresets::exists (const juce::String& patternName) const
{
    return files::fileFor (getFolder(), patternName, kExtension).existsAsFile();
}

void PatternPresets::rescan()
{
    std::array<juce::File, kNumPatternSlots> loadedFiles;
    for (size_t i = 0; i < slots.size(); ++i)
        if (isUser (slots[i].entry))
            loadedFiles[i] = userFiles[slots[i].entry - kNumFactoryEntries];

    userFiles = files::listSorted (getFolder(), kExtension);

    for (size_t i = 0; i < slots.size(); ++i)
    {
        if (loadedFiles[i] != juce::File())
        {
            const int index = userFiles.indexOf (loadedFiles[i]);
            slots[i].entry = index >= 0 ? kNumFactoryEntries + index : -1;
        }
    }
    changed();
}

int PatternPresets::entryNamed (const juce::String& wanted) const
{
    int found = -1;
    for (int i = 0; i < numEntries(); ++i)
        if (entryName (i) == wanted)
            found = i;
    return found;
}

void PatternPresets::load (int index)
{
    if (isUser (index))
    {
        const auto xml = files::parseXmlFile (userFiles[index - kNumFactoryEntries]);
        if (xml == nullptr || ! xml->hasTagName ("BleepWerkPattern")
            || ! parseCompactPattern (xml->getStringAttribute ("steps").toRawUTF8(), pattern()))
            return;
        // files from before rate and gate were stored leave the current values
        applyGroove (pattern(), { xml->getIntAttribute ("length", kMaxSteps), (float) xml->getDoubleAttribute ("swing", 0.5),
                       rateIndexFor (xml->getStringAttribute ("rate")), (float) xml->getDoubleAttribute ("gate", -1.0),
                       xml->getIntAttribute ("drift", 0), xml->getIntAttribute ("vary", 0), xml->getIntAttribute ("shape", 0), xml->getIntAttribute ("transpose", 0) });
    }
    else if (juce::isPositiveAndBelow (index, kNumFactoryPatterns))
    {
        const auto& factory = kFactoryPatterns[index];
        parseCompactPattern (factory.steps, pattern());
        applyGroove (pattern(), { factory.length, factory.swing, factory.rate, factory.gate, factory.drift, factory.vary, factory.shape, 0 });
    }
    else if (isSet (index))
    {
        loadSet (index - kNumFactoryPatterns);
        changed();
        return;
    }
    else
    {
        return;
    }

    slot() = { entryName (index), index };
    changed();
}

// The layers go into slots A, B, C... and play together; the panel shows A. Other slots keep
// their steps but stop playing, so a set is heard as it was written.
void PatternPresets::loadSet (int setIndex)
{
    const auto& set = kFactorySets[setIndex];
    uint32_t mask = 0;
    for (int i = 0; i < set.numLayers; ++i)
    {
        const auto& layer = set.layers[i];
        auto& target = bank.slot (i);
        parseCompactPattern (layer.steps, target);
        applyGroove (target, { layer.length, 0.5f, layer.rate, layer.gate, 0, 0, 0, 0 });
        slots[(size_t) i] = { juce::String (set.name) + " " + juce::String (i + 1) + "/" + juce::String (set.numLayers),
                              kNumFactoryPatterns + setIndex };
        mask |= 1u << i;
    }
    bank.setPlayingMask (mask);
    bank.select (0);
}

bool PatternPresets::save (const juce::String& patternName)
{
    const auto file = files::fileFor (getFolder(), patternName, kExtension);
    if (file.getFileName() == kExtension) // nothing of the name is legal in a file name
        return false;
    if (! file.getParentDirectory().createDirectory())
        return false;

    juce::XmlElement xml ("BleepWerkPattern");
    xml.setAttribute ("version", 2);
    xml.setAttribute ("name", patternName.trim());
    const auto groove = pattern().groove();
    xml.setAttribute ("length", groove.length);
    xml.setAttribute ("swing", groove.swing);
    xml.setAttribute ("rate", kSeqRateNames[groove.rate]);
    xml.setAttribute ("gate", groove.gate);
    xml.setAttribute ("drift", groove.drift);
    xml.setAttribute ("vary", groove.vary);
    xml.setAttribute ("shape", groove.shape);
    xml.setAttribute ("transpose", groove.transpose);
    xml.setAttribute ("steps", juce::String (toCompactPattern (pattern())));
    if (! xml.writeTo (file))
        return false;

    slot() = { file.getFileNameWithoutExtension(), -1 };
    rescan();
    slot().entry = kNumFactoryEntries + userFiles.indexOf (file);
    changed();
    return true;
}

void PatternPresets::remove (int index)
{
    if (! isUser (index))
        return;

    files::moveToTrash (userFiles[index - kNumFactoryEntries]);
    for (auto& s : slots)
        if (s.entry == index)
            s.entry = -1;
    rescan();
}

void PatternPresets::randomize()
{
    pattern().randomize (rng);
    slot() = { "Random " + juce::String (++randomCount), -1 };
    changed();
}

void PatternPresets::clear()
{
    pattern().clear();
    slot() = { "Empty", -1 };
    changed();
}

void PatternPresets::markEdited()
{
    if (! slot().name.endsWith ("*"))
    {
        slot().name << " *";
        changed();
    }
}

void PatternPresets::copySlot (int from, int to)
{
    if (! juce::isPositiveAndBelow (from, kNumPatternSlots) || ! juce::isPositiveAndBelow (to, kNumPatternSlots) || from == to)
        return;
    bank.slot (to).copyFrom (bank.slot (from));
    slots[(size_t) to] = slots[(size_t) from];
    changed();
}

// A track as a standard MIDI file at the clip resolution. The file is replaced if it exists.
static bool writeMidiFile (const juce::File& file, const juce::MidiMessageSequence& sequence, int ticksPerQuarter)
{
    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote (ticksPerQuarter);
    midiFile.addTrack (sequence);

    juce::FileOutputStream stream (file);
    if (! stream.openedOk())
        return false;
    stream.setPosition (0);
    stream.truncate();
    return midiFile.writeTo (stream);
}

juce::File PatternPresets::exportMidi (int slotIndex)
{
    if (! juce::isPositiveAndBelow (slotIndex, kNumPatternSlots))
        return {};

    constexpr int ticksPerQuarter = 960;
    const auto clip = patternToClip (bank.slot (slotIndex), (int) state.getRawParameterValue ("seqRoot")->load());

    const auto name = "BleepWerk " + juce::String::charToString ((juce::juce_wchar) ('A' + slotIndex)) + " - "
                      + slots[(size_t) slotIndex].name.trimCharactersAtEnd (" *");
    juce::MidiMessageSequence sequence;
    sequence.addEvent (juce::MidiMessage::textMetaEvent (3, name)); // track name
    for (const auto& n : clip.notes)
    {
        sequence.addEvent (juce::MidiMessage::noteOn (1, n.note, (juce::uint8) n.velocity).withTimeStamp (n.start * ticksPerQuarter));
        sequence.addEvent (juce::MidiMessage::noteOff (1, n.note).withTimeStamp (n.end * ticksPerQuarter));
    }
    sequence.updateMatchedPairs();
    // the track ends with the pattern, so the clip is as long as one pass
    sequence.addEvent (juce::MidiMessage::endOfTrack().withTimeStamp (clip.lengthBeats * ticksPerQuarter));

    const auto file = files::fileFor (midiFolder(), name, ".mid");
    if (! midiFolder().createDirectory())
        return {};
    return writeMidiFile (file, sequence, ticksPerQuarter) ? file : juce::File();
}

juce::File PatternPresets::exportLayersMidi (int bars)
{
    const auto clip = bleepwerk::layersToClip (bank, (int) state.getRawParameterValue ("seqRoot")->load(), bars);
    if (clip.notes.empty())
        return {};

    const auto name = "BleepWerk layers " + juce::String (bars) + " bars";
    juce::MidiMessageSequence sequence;
    sequence.addEvent (juce::MidiMessage::textMetaEvent (3, name)); // track name
    // Every note-off goes in before any note-on, so a note that ends where the same pitch starts
    // again is let go first (events at the same time stay in the order they were added).
    for (const auto& n : clip.notes)
        sequence.addEvent (juce::MidiMessage::noteOff (1, n.note).withTimeStamp ((double) n.end));
    for (const auto& n : clip.notes)
        sequence.addEvent (juce::MidiMessage::noteOn (1, n.note, (juce::uint8) n.velocity).withTimeStamp ((double) n.start));
    sequence.addEvent (juce::MidiMessage::endOfTrack().withTimeStamp ((double) clip.lengthTicks));

    if (! midiFolder().createDirectory())
        return {};
    const auto file = files::uniqueFileFor (midiFolder(), name, ".mid");
    return writeMidiFile (file, sequence, bleepwerk::kClipTicksPerQuarter) ? file : juce::File();
}

static juce::String slotSuffix (int index)
{
    return juce::String::charToString ((juce::juce_wchar) ('A' + index));
}

void PatternPresets::writeState (juce::ValueTree& tree) const
{
    tree.setProperty ("patternPlaying", (int) bank.playing(), nullptr); // the layers that play
    tree.setProperty ("patternEdited", bank.active(), nullptr);         // the slot the panel shows
    for (int i = 0; i < kNumPatternSlots; ++i)
    {
        tree.setProperty ("pattern" + slotSuffix (i), juce::String (bank.slot (i).toString()), nullptr);
        tree.setProperty ("patternName" + slotSuffix (i), slots[(size_t) i].name, nullptr);
        tree.setProperty ("patternGroove" + slotSuffix (i), juce::String (bank.slot (i).groove().toString()), nullptr);
    }
}

// A slot missing from the project keeps what it has.
void PatternPresets::readState (const juce::ValueTree& tree)
{
    if (tree.hasProperty ("patternPlaying"))
        bank.setPlayingMask ((uint32_t) std::max (0, (int) tree.getProperty ("patternPlaying")));
    bank.select ((int) tree.getProperty ("patternEdited", 0));
    for (int i = 0; i < kNumPatternSlots; ++i)
    {
        const auto steps = "pattern" + slotSuffix (i), name = "patternName" + slotSuffix (i);
        auto& target = bank.slot (i);
        if (tree.hasProperty (steps))
            target.fromString (tree.getProperty (steps).toString().toStdString());
        Groove groove = target.groove();
        const auto saved = tree.getProperty ("patternGroove" + slotSuffix (i)).toString().toStdString();
        if (! saved.empty() && groove.fromString (saved))
            target.setGroove (groove);
        const auto slotName = tree.getProperty (name, slots[(size_t) i].name).toString();
        slots[(size_t) i] = { slotName, entryNamed (slotName) };
    }
    changed();
}

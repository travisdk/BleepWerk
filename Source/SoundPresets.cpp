// Copyright (C) 2026 Henrik Juhl. Part of BleepWerk, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#include "SoundPresets.h"
#include "WerkSynth.h"
#include "PresetFiles.h"

using namespace bleepwerk;

SoundPresets::SoundPresets (juce::AudioProcessorValueTreeState& s)
    : state (s), matchedGains (factoryPresets().size())
{
    rescan();
}

juce::String SoundPresets::entryName (int index) const
{
    if (isUser (index))
        return userFiles[index - numFactory()].getFileNameWithoutExtension();
    return juce::isPositiveAndBelow (index, numFactory()) ? factoryPresets()[(size_t) index].name : "";
}

juce::String SoundPresets::entryCategory (int index) const
{
    if (isUser (index))
        return "User";
    return juce::isPositiveAndBelow (index, numFactory()) ? factoryPresets()[(size_t) index].category : "";
}

juce::File SoundPresets::folder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("BleepWerk/Presets");
}

bool SoundPresets::exists (const juce::String& presetName) const
{
    return files::fileFor (folder(), presetName, kExtension).existsAsFile();
}

void SoundPresets::rescan()
{
    const auto currentFile = isUser (currentEntry) ? userFiles[currentEntry - numFactory()] : juce::File();
    userFiles = files::listSorted (folder(), kExtension);

    if (currentFile != juce::File())
    {
        const int index = userFiles.indexOf (currentFile);
        currentEntry = index >= 0 ? numFactory() + index : 0;
    }
    changed();
}

// Stores every sound parameter as a plain value, the output gain included: unlike a factory
// preset, a user preset comes back exactly as it was saved.
bool SoundPresets::save (const juce::String& presetName)
{
    const auto file = files::fileFor (folder(), presetName, kExtension);
    if (file.getFileName() == kExtension) // nothing of the name is legal in a file name
        return false;
    if (! file.getParentDirectory().createDirectory())
        return false;

    juce::XmlElement xml ("BleepWerkPreset");
    xml.setAttribute ("version", kFileVersion);
    xml.setAttribute ("name", presetName.trim());
    for (auto* p : state.processor.getParameters())
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (p); param != nullptr && isSoundParameter (param->paramID))
        {
            auto* element = xml.createNewChildElement ("Param");
            element->setAttribute ("id", param->paramID);
            element->setAttribute ("value", param->convertFrom0to1 (param->getValue()));
        }

    if (! xml.writeTo (file))
        return false;

    currentName = file.getFileNameWithoutExtension();
    currentEntry = -1;
    rescan();
    currentEntry = numFactory() + userFiles.indexOf (file);
    changed();
    return true;
}

// Sound parameters missing from older files, or not numbers in a damaged one, fall back to
// their defaults.
bool SoundPresets::loadFile (const juce::File& file)
{
    const auto xml = files::parseXmlFile (file);
    if (xml == nullptr || ! xml->hasTagName ("BleepWerkPreset"))
        return false;

    // files saved before 0.11 have one delay time list instead of a time and a feel; files
    // before version 2 have the Werk choices of before Crackle and Metal were removed
    const bool oldWerkChoices = xml->getIntAttribute ("version", 1) < 2;
    int oldDelayNote = -1, oldDelayFeel = -1;
    if (auto* old = xml->getChildByAttribute ("id", "delayTime"); old != nullptr && ! std::isnan (old->getDoubleAttribute ("value")))
        bleepwerk::splitOldDelayTime (juce::roundToInt (old->getDoubleAttribute ("value")), oldDelayNote, oldDelayFeel);

    for (auto* p : state.processor.getParameters())
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (p); param != nullptr && isSoundParameter (param->paramID))
        {
            float value = param->convertFrom0to1 (param->getDefaultValue());
            if (auto* element = xml->getChildByAttribute ("id", param->paramID))
            {
                if (const auto loaded = (float) element->getDoubleAttribute ("value", value); ! std::isnan (loaded))
                    value = loaded;
            }
            else if (oldDelayNote >= 0 && param->paramID == "delayNote")
                value = (float) oldDelayNote;
            else if (oldDelayFeel >= 0 && param->paramID == "delayFeel")
                value = (float) oldDelayFeel;
            if (oldWerkChoices && param->paramID == "strikeExciter")
                value = (float) bleepwerk::migrateWerkExciter (juce::roundToInt (value));
            files::setParameterValue (*param, value);
        }
    return true;
}

// Every sound parameter the preset leaves out goes back to its default, then the output gain
// is set to match the others' loudness.
void SoundPresets::loadFactory (int index)
{
    const auto& preset = factoryPresets()[(size_t) index];
    auto valueOf = [&preset] (const char* id)
    {
        for (const auto& v : preset.values)
            if (std::strcmp (v.id, id) == 0)
                return v.value;
        return soundDefault (id);
    };

    auto& measured = matchedGains[(size_t) index];
    if (! measured.has_value())
        measured = matchedOutputDb (readPatch (valueOf));
    const float gain = *measured;

    for (const auto& p : kSoundParams)
        if (auto* param = state.getParameter (p.id))
            files::setParameterValue (*param, std::strcmp (p.id, "outputGain") == 0 ? gain : valueOf (p.id));
}

void SoundPresets::remove (int index)
{
    if (! isUser (index))
        return;

    files::moveToTrash (userFiles[index - numFactory()]);
    if (currentEntry == index)
        currentEntry = 0;
    rescan();
}

void SoundPresets::load (int index)
{
    if (isUser (index))
    {
        if (! loadFile (userFiles[index - numFactory()]))
            return;
    }
    else if (juce::isPositiveAndBelow (index, numFactory()))
    {
        loadFactory (index);
    }
    else
    {
        return;
    }

    currentEntry = index;
    currentName = entryName (index);
    changed();
}

void SoundPresets::setCurrent (int entry, const juce::String& newName)
{
    currentEntry = juce::jlimit (-1, numEntries() - 1, entry);
    currentName = newName;
    changed();
}

void SoundPresets::writeState (juce::ValueTree& tree) const
{
    tree.setProperty ("presetName", currentName, nullptr);
    tree.setProperty ("presetIndex", isUser (currentEntry) ? 0 : currentEntry, nullptr);
}

void SoundPresets::readState (const juce::ValueTree& tree)
{
    currentName = tree.getProperty ("presetName", "Init").toString();
    currentEntry = juce::jlimit (-1, numFactory() - 1, (int) tree.getProperty ("presetIndex", 0)); // -1: a rolled sound
    for (int i = numFactory(); i < numEntries(); ++i)
        if (entryName (i) == currentName)
            currentEntry = i; // a user preset of that name is the better match
    changed();
}

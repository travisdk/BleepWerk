// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

// Damaged or hostile projects and preset files, and other input from outside, through the real
// processor (this test links JUCE, unlike the others). Each case runs on its own, named on the
// command line, so ctest shows one test per case. On Linux HOME points at a temporary folder,
// so presets and settings never touch the user's; elsewhere the cases with files are skipped.
#include "PluginProcessor.h"
#include "PresetFiles.h"
#include "TestUtils.h"

#include <cmath>
#include <cstdlib>
#include <map>

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 512;

// Two seconds with a note held; true if every sample is finite.
bool rendersFinite (BleepForgeProcessor& processor)
{
    processor.setRateAndBufferSizeDetails (kSampleRate, kBlockSize);
    processor.prepareToPlay (kSampleRate, kBlockSize);

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    bool finite = true;
    for (int block = 0; block * kBlockSize < 2 * kSampleRate; ++block)
    {
        juce::MidiBuffer midi;
        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.9f), 0);
        processor.processBlock (buffer, midi);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < kBlockSize; ++i)
                finite = finite && std::isfinite (buffer.getSample (ch, i));
    }
    return finite;
}

bool parametersFinite (BleepForgeProcessor& processor)
{
    for (auto* param : processor.getParameters())
        if (! std::isfinite (param->getValue()))
            return false;
    return true;
}

bool isDefault (BleepForgeProcessor& processor, const char* id)
{
    auto* param = processor.apvts.getParameter (id);
    return juce::approximatelyEqual (param->getValue(), param->getDefaultValue());
}

void setState (BleepForgeProcessor& processor, const juce::XmlElement& xml)
{
    juce::MemoryBlock data;
    juce::AudioProcessor::copyXmlToBinary (xml, data);
    processor.setStateInformation (data.getData(), (int) data.getSize());
}

std::unique_ptr<juce::XmlElement> savedState (BleepForgeProcessor& processor)
{
    juce::MemoryBlock data;
    processor.getStateInformation (data);
    return juce::AudioProcessor::getXmlFromBinary (data.getData(), (int) data.getSize());
}

// Loads the user preset of that name; false if there is none.
bool loadUserPreset (SoundPresets& presets, const juce::String& name)
{
    presets.rescan();
    for (int i = presets.numFactory(); i < presets.numEntries(); ++i)
        if (presets.entryName (i) == name)
        {
            presets.load (i);
            return true;
        }
    return false;
}

const char* const kNotNumbers[] = { "nan", "-nan", "NaN" };

// A project or preset file whose values are not numbers loads with the defaults and plays.
void notNumbers()
{
    for (auto* text : kNotNumbers)
    {
        BleepForgeProcessor processor;
        auto xml = savedState (processor);
        CHECK (xml != nullptr);
        for (auto* param : xml->getChildIterator())
            param->setAttribute ("value", text);
        auto* oldDelay = xml->createNewChildElement ("PARAM"); // the delay time before version 3
        oldDelay->setAttribute ("id", "delayTime");
        oldDelay->setAttribute ("value", text);
        setState (processor, *xml);
        CHECK (parametersFinite (processor));
        CHECK (isDefault (processor, "filterCutoff"));
        CHECK (rendersFinite (processor));
    }

   #if JUCE_LINUX
    for (auto* text : kNotNumbers)
    {
        BleepForgeProcessor processor;
        juce::XmlElement xml ("BleepForgePreset");
        for (auto* param : processor.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            {
                auto* element = xml.createNewChildElement ("Param");
                element->setAttribute ("id", ranged->paramID);
                element->setAttribute ("value", text);
            }
        auto* oldDelay = xml.createNewChildElement ("Param");
        oldDelay->setAttribute ("id", "delayTime");
        oldDelay->setAttribute ("value", text);
        CHECK (SoundPresets::folder().createDirectory());
        CHECK (xml.writeTo (SoundPresets::folder().getChildFile ("Broken.bfpreset")));

        auto& presets = processor.getSoundPresets();
        CHECK (loadUserPreset (presets, "Broken"));
        CHECK (presets.name() == "Broken");
        CHECK (parametersFinite (processor));
        CHECK (isDefault (processor, "filterCutoff"));
        CHECK (rendersFinite (processor));
    }
   #endif
}
// A folder name with "$(...)" is opened, not run as a command. A stand-in xdg-open records the
// folder it was given; PATH holds only it, so no real program can start.
void openFolder()
{
   #if JUCE_LINUX
    const auto root = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    const auto bin = root.getChildFile ("bin");
    const auto opened = root.getChildFile ("opened"), injected = root.getChildFile ("injected");
    CHECK (bin.createDirectory());
    const auto opener = bin.getChildFile ("xdg-open");
    CHECK (opener.replaceWithText ("#!/bin/sh\nprintf '%s' \"$1\" > \"" + opened.getFullPathName() + ".tmp\" && /bin/mv \""
                                       + opened.getFullPathName() + ".tmp\" \"" + opened.getFullPathName() + "\"\n",
                                   false, false, "\n"));
    CHECK (opener.setExecutePermission (true));
    setenv ("PATH", bin.getFullPathName().toRawUTF8(), 1);
    root.setAsCurrentWorkingDirectory();

    const auto folder = root.getChildFile ("Bleeps $(: >injected)");
    CHECK (folder.createDirectory());
    CHECK (bleepforge::files::openFolder (folder));
    for (int waited = 0; waited < 5000 && ! opened.existsAsFile(); waited += 10)
        juce::Thread::sleep (10);
    CHECK (opened.loadFileAsString() == folder.getFullPathName());
    CHECK (! injected.exists());
   #endif
}
// Elements nested far deeper than any real file would overflow the stack in JUCE's parser, and a
// huge file would stall the host: both are refused and change nothing.
void hugeXml()
{
    constexpr int deep = 100000;
    const auto nested = juce::String::repeatedString ("<a>", deep) + juce::String::repeatedString ("</a>", deep);
    const auto huge = juce::String::repeatedString ("x", 2 << 20);

    BleepForgeProcessor processor;
    auto& presets = processor.getSoundPresets();
    juce::MemoryOutputStream deepState;
    deepState.writeInt (0x21324356); // copyXmlToBinary's format: magic, size, text
    const auto text = "<PARAMS>" + nested + "</PARAMS>";
    deepState.writeInt ((int) text.getNumBytesAsUTF8() + 1);
    deepState.write (text.toRawUTF8(), text.getNumBytesAsUTF8() + 1);
    processor.setStateInformation (deepState.getData(), (int) deepState.getDataSize());

    auto xml = savedState (processor);
    xml->setAttribute ("presetName", huge);
    setState (processor, *xml);
    CHECK (presets.name() == "Init");

   #if JUCE_LINUX
    CHECK (SoundPresets::folder().createDirectory());
    CHECK (SoundPresets::folder().getChildFile ("Deep.bfpreset").replaceWithText ("<BleepForgePreset>" + nested + "</BleepForgePreset>"));
    CHECK (SoundPresets::folder().getChildFile ("Huge.bfpreset").replaceWithText ("<BleepForgePreset name=\"" + huge + "\"/>"));
    for (auto* name : { "Deep", "Huge" })
    {
        CHECK (loadUserPreset (presets, name));
        CHECK (presets.name() == "Init");
    }
   #endif
}
// A bend range typed in the host with an absurd number lands on the top of the range, not
// wherever the number wrapped around to.
void typedNumbers()
{
    BleepForgeProcessor processor;
    auto* bendRange = processor.apvts.getParameter ("bendRange");
    CHECK (juce::approximatelyEqual (bendRange->getValueForText ("4294967298"), 1.0f));
    CHECK (juce::approximatelyEqual (bendRange->getValueForText ("99999999999999999999999 st"), 1.0f));
    CHECK (juce::approximatelyEqual (bendRange->getValueForText ("12 st"), bendRange->convertTo0to1 (12.0f)));
    CHECK (juce::approximatelyEqual (bendRange->getValueForText ("Off"), 0.0f));
}
// A name of only characters a file name can't hold is not saved as a hidden, nameless file.
void namelessPreset()
{
   #if JUCE_LINUX
    BleepForgeProcessor processor;
    auto& presets = processor.getSoundPresets();
    CHECK (! presets.save ("///"));
    CHECK (! SoundPresets::folder().getChildFile (SoundPresets::kExtension).exists());
    CHECK (presets.save ("Blip/2"));
    CHECK (SoundPresets::folder().getChildFile ("Blip2.bfpreset").existsAsFile());
   #endif
}
} // namespace

int main (int argc, char* argv[])
{
    const std::map<juce::String, void (*)()> cases {
        { "not-numbers", notNumbers },
        { "open-folder", openFolder },
        { "huge-xml", hugeXml },
        { "typed-numbers", typedNumbers },
        { "nameless-preset", namelessPreset },
    };
    const juce::String name = argc > 1 ? argv[1] : "";
    const auto testCase = cases.find (name);
    if (testCase == cases.end())
    {
        std::printf ("unknown case \"%s\"\n", name.toRawUTF8());
        return 1;
    }

    const juce::ScopedJuceInitialiser_GUI juceInit;
    const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getNonexistentChildFile ("BleepForgeTest", {}, false);
    CHECK (root.createDirectory());
   #if JUCE_LINUX
    setenv ("HOME", root.getFullPathName().toRawUTF8(), 1);
   #endif

    testCase->second();

    root.deleteRecursively();
    return finish (("hardening " + name).toRawUTF8());
}

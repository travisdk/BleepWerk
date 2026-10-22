// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

// Damaged or hostile projects and preset files, and other input from outside, through the real
// processor (this test links JUCE, unlike the others). Each case runs on its own, named on the
// command line, so ctest shows one test per case. On Linux HOME points at a temporary folder,
// so presets and settings never touch the user's; elsewhere the cases with files are skipped.
#include "PluginProcessor.h"
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
} // namespace

int main (int argc, char* argv[])
{
    const std::map<juce::String, void (*)()> cases {
        { "not-numbers", notNumbers },
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

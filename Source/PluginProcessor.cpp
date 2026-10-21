// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
juce::NormalisableRange<float> skewedRange (float lo, float hi, float centre)
{
    juce::NormalisableRange<float> range (lo, hi);
    range.setSkewForCentre (centre);
    return range;
}

// The same range run backwards: the knob's left end is the range's top. For controls whose
// "off" sits at the top of their values (the crusher's bits and rate), so off is fully left.
juce::NormalisableRange<float> reversedRange (const juce::NormalisableRange<float>& range)
{
    return { range.start, range.end,
             [range] (float, float, float proportion) { return range.convertFrom0to1 (1.0f - proportion); },
             [range] (float, float, float value) { return 1.0f - range.convertTo0to1 (value); },
             [range] (float, float, float value) { return range.snapToLegalValue (value); } };
}

// Parameters without a unit are 0..1 amounts and display as percent. (From SubForge.)
std::unique_ptr<juce::AudioParameterFloat> floatParam (const char* id, const char* name,
                                                       juce::NormalisableRange<float> range,
                                                       float defaultValue, const char* unit = "")
{
    const juce::String unitText (unit);
    const bool isPercent = unitText.isEmpty();

    auto toText = [unitText, isPercent] (float value, int)
    {
        if (isPercent)
            return juce::String (juce::roundToInt (value * 100.0f)) + " %";
        // decided on the rounded value, so 9.97 shows as "10" like 10 does and the text reads back the same
        const float tenths = std::round (value * 10.0f) / 10.0f;
        const auto number = std::abs (tenths) < 10.0f ? juce::String (tenths, 1) : juce::String (juce::roundToInt (value));
        return number + " " + unitText;
    };
    auto fromText = [isPercent] (const juce::String& text)
    {
        const float value = text.retainCharacters ("0123456789.-").getFloatValue();
        return isPercent ? value / 100.0f : value;
    };

    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, defaultValue,
                                                        juce::AudioParameterFloatAttributes()
                                                            .withStringFromValueFunction (toText)
                                                            .withValueFromStringFunction (fromText));
}
// Per-user preferences, not per project (the colour theme): ~/.config/BleepForge/BleepForge.settings
// on Linux. (From SubForge.)
juce::PropertiesFile::Options settingsOptions()
{
    juce::PropertiesFile::Options options;
    options.applicationName = "BleepForge";
   #if JUCE_LINUX
    options.folderName = ".config/BleepForge"; // JUCE puts Linux settings straight under ~ otherwise
   #else
    options.folderName = "BleepForge";
   #endif
    options.filenameSuffix = "settings";
    options.osxLibrarySubFolder = "Application Support";
    return options;
}
} // namespace

BleepForgeProcessor::BleepForgeProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout()),
      userSettings (settingsOptions())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout BleepForgeProcessor::createParameterLayout()
{
    using namespace juce;
    using bleepforge::soundDefault;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    // Sound parameters take their range and default from bleepforge::kSoundParams, which the
    // factory presets are checked against; only the skew or step is set here.
    auto info = [] (const char* id)
    {
        const auto* p = bleepforge::findSoundParam (id);
        jassert (p != nullptr);
        return *p;
    };
    auto linear = [info] (const char* id, float step = 0.0f) { return NormalisableRange<float> (info (id).min, info (id).max, step); };
    auto skewed = [info] (const char* id, float centre) { return skewedRange (info (id).min, info (id).max, centre); };
    auto sound = [&params, info] (const char* id, const char* name, NormalisableRange<float> range, const char* unit = "")
    { params.push_back (floatParam (id, name, range, info (id).defaultValue, unit)); };
    auto choice = [&params] (const char* id, const char* name, const StringArray& names)
    { params.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, names, (int) soundDefault (id))); };
    auto namesOf = [] (const auto& list)
    {
        StringArray names;
        for (auto* name : list)
            names.add (name);
        return names;
    };

    choice ("engine", "Engine", namesOf (bleepforge::kEngineNames));

    // Bleep engine
    choice ("bleepWave", "Wave", namesOf (bleepforge::kBleepWaveNames));
    sound ("bleepPulseWidth", "Pulse Width", linear ("bleepPulseWidth"));
    sound ("bleepFmRatio", "FM Ratio", linear ("bleepFmRatio", 0.5f), "x");
    sound ("bleepFmAmount", "FM Amount", linear ("bleepFmAmount"));
    sound ("bleepPitchAmount", "Pitch Env", linear ("bleepPitchAmount", 0.1f), "st");
    sound ("bleepPitchDecay", "Pitch Decay", skewed ("bleepPitchDecay", 40.0f), "ms");
    sound ("bleepAttack", "Attack", skewed ("bleepAttack", 5.0f), "ms");
    sound ("bleepDecay", "Decay", skewed ("bleepDecay", 200.0f), "ms");
    sound ("bleepRelease", "Release", skewed ("bleepRelease", 50.0f), "ms");
    sound ("bleepNoise", "Noise", linear ("bleepNoise"));

    // Foley engine
    choice ("foleyExciter", "Exciter", namesOf (bleepforge::kFoleyExciterNames));
    sound ("foleyLength", "Exciter Length", skewed ("foleyLength", 5.0f), "ms");
    // Material shows the material it sounds like rather than a percentage
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "foleyMaterial", 1 }, "Material", linear ("foleyMaterial"), soundDefault ("foleyMaterial"),
        AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int)
                                          {
                                              return String (v < 0.15f ? "Wood" : v < 0.4f ? "Wood/Metal" : v < 0.6f ? "Metal"
                                                                         : v < 0.85f ? "Metal/Glass" : "Glass");
                                          })
            .withValueFromStringFunction ([] (const String& text)
                                          {
                                              const auto t = text.trim().toLowerCase();
                                              // each name reads back inside its own band, so text round-trips
                                              if (t == "wood") return 0.0f;
                                              if (t == "wood/metal") return 0.275f;
                                              if (t == "metal") return 0.5f;
                                              if (t == "metal/glass") return 0.725f;
                                              if (t == "glass") return 1.0f;
                                              return t.retainCharacters ("0123456789.").getFloatValue() / 100.0f;
                                          })));
    sound ("foleySize", "Size", linear ("foleySize"));
    sound ("foleyDamping", "Damping", linear ("foleyDamping"));
    sound ("foleyBrightness", "Brightness", linear ("foleyBrightness"));
    sound ("foleySweep", "Sweep", linear ("foleySweep", 0.1f), "st");

    // Machine engine
    choice ("machineType", "Machine Type", namesOf (bleepforge::kMachineTypeNames));
    sound ("machineLength", "Machine Length", skewed ("machineLength", 600.0f), "ms");
    sound ("machineTone", "Machine Tone", linear ("machineTone"));
    sound ("machineMotion", "Motion", linear ("machineMotion"));
    sound ("machineFocus", "Focus", linear ("machineFocus"));
    sound ("machineMetal", "Metal", linear ("machineMetal"));

    // Chaos: Amount scales every target's depth
    sound ("chaosAmount", "Chaos", linear ("chaosAmount"));
    sound ("chaosPitch", "Chaos Pitch", linear ("chaosPitch"));
    sound ("chaosDecay", "Chaos Decay", linear ("chaosDecay"));
    sound ("chaosTone", "Chaos Tone", linear ("chaosTone"));
    sound ("chaosPan", "Chaos Pan", linear ("chaosPan"));
    sound ("chaosVelocity", "Chaos Velocity", linear ("chaosVelocity"));

    // Voices
    choice ("voiceMode", "Voice Mode", { "Mono", "Poly" });
    sound ("glide", "Glide", skewed ("glide", 60.0f), "ms");
    // How far a full pitch bend goes, up and down. A setting of the player, not of the sound,
    // so it isn't in kSoundParams and presets leave it alone.
    params.push_back (std::make_unique<AudioParameterInt> (
        ParameterID { "bendRange", 1 }, "Bend Range", 0, 24, 2,
        AudioParameterIntAttributes()
            .withStringFromValueFunction ([] (int v, int) { return v == 0 ? String ("Off") : String::charToString (0xb1) + String (v) + " st"; })
            .withValueFromStringFunction ([] (const String& t) { return t.retainCharacters ("0123456789").getIntValue(); })));

    // Effects, in signal order. Bits and Rate read "Off" at the top of their values, where they
    // do nothing; their knobs run backwards so that is fully left.
    choice ("filterType", "Filter", namesOf (bleepforge::kFilterTypeNames));
    sound ("filterCutoff", "Cutoff", skewed ("filterCutoff", 1000.0f), "Hz");
    sound ("filterResonance", "Resonance", linear ("filterResonance"));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "crushBits", 1 }, "Bits", reversedRange (linear ("crushBits")), soundDefault ("crushBits"),
        AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) { return v >= bleepforge::kCrushBitsOff - 0.01f ? String ("Off") : String (v, 1) + " bit"; })
            .withValueFromStringFunction ([] (const String& t)
                                          {
                                              return t.trim().equalsIgnoreCase ("off") ? bleepforge::kCrushBitsOff
                                                                                      : t.retainCharacters ("0123456789.").getFloatValue();
                                          })));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "crushRate", 1 }, "Crush Rate", reversedRange (skewed ("crushRate", 6000.0f)), soundDefault ("crushRate"),
        AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int)
                                          {
                                              if (v >= bleepforge::kCrushRateOff - 1.0f)
                                                  return String ("Off");
                                              return v < 1000.0f ? String (roundToInt (v)) + " Hz" : String (v / 1000.0f, 1) + " kHz";
                                          })
            .withValueFromStringFunction ([] (const String& t)
                                          {
                                              const auto text = t.trim().toLowerCase();
                                              if (text == "off")
                                                  return bleepforge::kCrushRateOff;
                                              const float value = text.retainCharacters ("0123456789.").getFloatValue();
                                              return text.contains ("k") ? value * 1000.0f : value;
                                          })));
    choice ("delayTime", "Delay Time", namesOf (bleepforge::kDelayTimeNames));
    sound ("delayFeedback", "Delay Feedback", linear ("delayFeedback"));
    sound ("delayMix", "Delay Mix", linear ("delayMix"));
    sound ("roomSize", "Reverb Size", linear ("roomSize"));
    sound ("roomDecay", "Reverb Decay", skewed ("roomDecay", 1.5f), "s");
    sound ("roomTone", "Reverb Tone", linear ("roomTone"));
    sound ("roomMix", "Reverb Mix", linear ("roomMix"));
    sound ("roomDuck", "Reverb Duck", linear ("roomDuck"));

    sound ("outputGain", "Output Gain", linear ("outputGain", 0.1f), "dB");

    return { params.begin(), params.end() };
}

void BleepForgeProcessor::prepareToPlay (double sampleRate, int)
{
    bleep.prepare (sampleRate);
    foley.prepare (sampleRate);
    machine.prepare (sampleRate); // sizes the comb delays here, off the audio thread
    effects.prepare (sampleRate);
    noteInput.reset(); // prepare() has silenced the voices, so no key is held any more
    readParams();
    effects.setParams (patch.effects, hostBpm);
    outputGain.reset (sampleRate, 0.02);
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("outputGain")->load()));
}

bool BleepForgeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void BleepForgeProcessor::readParams()
{
    patch = bleepforge::readPatch ([this] (const char* id) { return apvts.getRawParameterValue (id)->load(); });

    bleep.setParams (patch.bleep);
    foley.setParams (patch.foleyParams);
    machine.setParams (patch.machine);
    bleep.setMode (patch.mono ? bleepforge::BleepSynth::Mode::Mono : bleepforge::BleepSynth::Mode::Poly);
    foley.setMode (patch.mono ? bleepforge::FoleySynth::Mode::Mono : bleepforge::FoleySynth::Mode::Poly);
    machine.setMode (patch.mono ? bleepforge::MachineSynth::Mode::Mono : bleepforge::MachineSynth::Mode::Poly);
    bleep.setGlideMs (patch.glideMs);
    foley.setGlideMs (patch.glideMs);
    machine.setGlideMs (patch.glideMs);
    bleep.setChaos (patch.chaos);
    foley.setChaos (patch.chaos);
    machine.setChaos (patch.chaos);
    // the range may change while a bend is held
    Engines { *this }.pitchBend (noteInput.currentBend());
}

void BleepForgeProcessor::Engines::noteOn (int note, float velocity)
{
    switch (p.patch.engine)
    {
        case bleepforge::Engine::Bleep: p.bleep.noteOn (note, velocity); break;
        case bleepforge::Engine::Foley: p.foley.noteOn (note, velocity); break;
        case bleepforge::Engine::Machine: p.machine.noteOn (note, velocity); break;
    }
}

void BleepForgeProcessor::Engines::noteOff (int note)
{
    p.bleep.noteOff (note);
    p.foley.noteOff (note);
    p.machine.noteOff (note);
}

void BleepForgeProcessor::Engines::pitchBend (float amount)
{
    const float semis = amount * p.apvts.getRawParameterValue ("bendRange")->load();
    p.bleep.setPitchBend (semis);
    p.foley.setPitchBend (semis);
    p.machine.setPitchBend (semis);
}

void BleepForgeProcessor::allNotesOff()
{
    bleep.allNotesOff();
    foley.allNotesOff();
    machine.allNotesOff();
}

void BleepForgeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    readParams();
    readTempo();

    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    // Render between the host's MIDI events so each note starts on its exact sample. Chaos
    // pans each note; a mono output gets them unpanned.
    float* left = buffer.getWritePointer (0);
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    auto renderRange = [&] (int from, int to)
    {
        if (to <= from)
            return;
        bleep.renderAdding (left + from, right != nullptr ? right + from : nullptr, to - from);
        foley.renderAdding (left + from, right != nullptr ? right + from : nullptr, to - from);
        machine.renderAdding (left + from, right != nullptr ? right + from : nullptr, to - from);
    };

    Engines engines { *this };
    int position = 0;
    for (const auto metadata : midi)
    {
        const int eventPos = juce::jlimit (0, numSamples, metadata.samplePosition);
        renderRange (position, eventPos);
        position = juce::jmax (position, eventPos);
        noteInput.handle (metadata.data, metadata.numBytes, engines);
    }
    renderRange (position, numSamples);

    effects.setParams (patch.effects, hostBpm);
    effects.process (left, right, numSamples);

    outputGain.setTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("outputGain")->load()));
    outputGain.applyGain (buffer, numSamples);
}

// While bypassed the host's note-offs don't reach the engines, so every note is let go and
// nothing hangs once the plug-in plays again. The effects' tails are cut, as a bypass expects.
void BleepForgeProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    buffer.clear();
    noteInput.reset();
    allNotesOff();
}

// The delay's synced time follows the host's tempo, kept from the last block that had one.
void BleepForgeProcessor::readTempo()
{
    if (auto* hostPlayHead = getPlayHead())
        if (auto position = hostPlayHead->getPosition())
            if (auto bpm = position->getBpm())
                hostBpm = *bpm;
}

juce::AudioProcessorEditor* BleepForgeProcessor::createEditor()
{
    return new BleepForgeEditor (*this);
}

// The preset name and other non-parameter data are stored with the APVTS state, so one XML
// blob holds the whole project.
void BleepForgeProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("stateVersion", kStateVersion, nullptr);
    soundPresets.writeState (state);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void BleepForgeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            soundPresets.readState (state);
            // Only the parameters stay in the tree: getStateInformation adds the rest again, so
            // a reloaded state saves byte for byte the same.
            for (auto id : { "stateVersion", "presetName", "presetIndex" })
                state.removeProperty (id, nullptr);
            for (int i = state.getNumChildren(); --i >= 0;)
                if (apvts.getParameter (state.getChild (i).getProperty ("id").toString()) == nullptr)
                    state.removeChild (i, nullptr);
            apvts.replaceState (state);
            // Tells the host every parameter may have changed, so it reads them all again
            // (CLAP: params rescan of the values, which the wrapper sends for a program change).
            updateHostDisplay (ChangeDetails().withProgramChanged (true));
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BleepForgeProcessor();
}

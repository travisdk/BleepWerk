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
    // the grid starts empty: notes come from the user or the pattern dice
    previewOn = userSettings.getBoolValue ("previewNotes", true);
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

    choice ("engine", "Engine", { "Bleep", "Foley" });

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

    // Sequencer
    StringArray scales;
    for (const auto& scale : bleepforge::kScales)
        scales.add (scale.name);
    params.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "seqScale", 1 }, "Scale", scales,
                                                              bleepforge::kDefaultScale));
    params.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "seqRoot", 1 }, "Root", namesOf (bleepforge::kRootNames), 0));
    params.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "seqRate", 1 }, "Rate", namesOf (bleepforge::kSeqRateNames),
                                                              bleepforge::kDefaultSeqRate));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "seqSwing", 1 }, "Swing", NormalisableRange<float> (0.5f, 0.75f, 0.005f), 0.5f,
        AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })
            .withValueFromStringFunction ([] (const String& t) { return t.retainCharacters ("0123456789.").getFloatValue() / 100.0f; })));
    // Cycle locks the pattern's chances into a phrase that repeats; Seed picks the phrase, and
    // automating it changes the phrase on cue.
    params.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "seqCycle", 1 }, "Cycle", namesOf (bleepforge::kCycleNames), 0));
    params.push_back (std::make_unique<AudioParameterInt> (ParameterID { "seqSeed", 1 }, "Seed", 1, bleepforge::kMaxSeed, 1));

    // Chaos: Amount scales every target's depth
    sound ("chaosAmount", "Chaos", linear ("chaosAmount"));
    sound ("chaosPitch", "Chaos Pitch", linear ("chaosPitch"));
    sound ("chaosDecay", "Chaos Decay", linear ("chaosDecay"));
    sound ("chaosTone", "Chaos Tone", linear ("chaosTone"));
    sound ("chaosPan", "Chaos Pan", linear ("chaosPan"));
    sound ("chaosVelocity", "Chaos Velocity", linear ("chaosVelocity"));

    // how many octaves the grid shows at a time: a view setting, saved with the project
    params.push_back (std::make_unique<AudioParameterInt> (ParameterID { "seqRange", 1 }, "Visible Range", 1, kMaxVisibleOctaves,
                                                           2, AudioParameterIntAttributes().withAutomatable (false)));

    // Voices
    choice ("voiceMode", "Voice Mode", { "Mono", "Poly" });
    sound ("glide", "Glide", skewed ("glide", 60.0f), "ms");

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
    sound ("roomSize", "Room Size", linear ("roomSize"));
    sound ("roomMix", "Room Mix", linear ("roomMix"));

    sound ("outputGain", "Output Gain", linear ("outputGain", 0.1f), "dB");

    return { params.begin(), params.end() };
}

void BleepForgeProcessor::prepareToPlay (double sampleRate, int)
{
    seqMidi.ensureSize (4096);
    bleep.prepare (sampleRate);
    foley.prepare (sampleRate);
    effects.prepare (sampleRate);
    readParams();
    effects.setParams (patch.effects, hostBpm);
    outputGain.reset (sampleRate, 0.02);
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("outputGain")->load()));
    previewPlaying = -1; // prepare() has silenced the voices
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
    bleep.setMode (patch.mono ? bleepforge::BleepSynth::Mode::Mono : bleepforge::BleepSynth::Mode::Poly);
    foley.setMode (patch.mono ? bleepforge::FoleySynth::Mode::Mono : bleepforge::FoleySynth::Mode::Poly);
    bleep.setGlideMs (patch.glideMs);
    foley.setGlideMs (patch.glideMs);
    bleep.setChaos (patch.chaos);
    foley.setChaos (patch.chaos);
}

void BleepForgeProcessor::handleNote (const juce::MidiMessage& message)
{
    // note-offs go to both, so a note that started before an engine switch still ends
    if (message.isNoteOn())
    {
        if (patch.foley)
            foley.noteOn (message.getNoteNumber(), message.getFloatVelocity());
        else
            bleep.noteOn (message.getNoteNumber(), message.getFloatVelocity());
    }
    else if (message.isNoteOff())
    {
        bleep.noteOff (message.getNoteNumber());
        foley.noteOff (message.getNoteNumber());
    }
}

void BleepForgeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    readParams();

    const int numSamples = buffer.getNumSamples();
    buffer.clear();
    runPreview (numSamples);
    runSequencer (numSamples);

    // Render between the sequencer's notes so they start on their exact sample. Chaos pans
    // each note; a mono output gets them unpanned.
    float* left = buffer.getWritePointer (0);
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    auto renderRange = [&] (int from, int to)
    {
        if (to <= from)
            return;
        bleep.renderAdding (left + from, right != nullptr ? right + from : nullptr, to - from);
        foley.renderAdding (left + from, right != nullptr ? right + from : nullptr, to - from);
    };

    int position = 0;
    for (const auto metadata : seqMidi)
    {
        const int eventPos = juce::jlimit (0, numSamples, metadata.samplePosition);
        renderRange (position, eventPos);
        position = juce::jmax (position, eventPos);
        handleNote (metadata.getMessage());
    }
    renderRange (position, numSamples);

    // after runSequencer, which has read this block's tempo
    effects.setParams (patch.effects, hostBpm);
    effects.process (left, right, numSamples);

    // BleepForge plays only its sequencer. With producesMidi() whatever is left in this buffer
    // goes to the host, so it is cleared; the MIDI output mode will fill it with seqMidi.
    midi.clear();

    outputGain.setTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("outputGain")->load()));
    outputGain.applyGain (buffer, numSamples);
}

// Steps follow the host's position, so the sequencer runs while the host plays.
void BleepForgeProcessor::runSequencer (int numSamples)
{
    double bpm = 120.0, ppq = 0.0;
    bool playing = false;
    if (auto* hostPlayHead = getPlayHead())
    {
        if (auto position = hostPlayHead->getPosition())
        {
            if (auto positionBpm = position->getBpm())
                bpm = *positionBpm;
            if (auto hostPpq = position->getPpqPosition(); hostPpq && position->getIsPlaying())
            {
                ppq = *hostPpq;
                playing = true;
            }
        }
    }

    hostBpm = bpm;
    const auto settings = sequencerSettings();
    seqMidi.clear();
    sequencer.process (playing, ppq, bpm, getSampleRate(), numSamples, settings, grid,
                       [this] (int sample, bool on, int note, float velocity)
                       {
                           seqMidi.addEvent (on ? juce::MidiMessage::noteOn (1, note, velocity)
                                                  : juce::MidiMessage::noteOff (1, note), sample);
                       });
}

bleepforge::SequencerSettings BleepForgeProcessor::sequencerSettings() const
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    bleepforge::SequencerSettings settings;
    settings.scale = (int) get ("seqScale");
    settings.root = (int) get ("seqRoot");
    settings.stepBeats = bleepforge::kSeqRateBeats[(size_t) juce::jlimit (0, bleepforge::kNumSeqRates - 1, (int) get ("seqRate"))];
    settings.swing = get ("seqSwing");
    settings.cycleSteps = bleepforge::cycleSteps ((int) get ("seqCycle"), settings.stepBeats);
    settings.seed = (int) get ("seqSeed");
    settings.velocityChaos = get ("chaosAmount") * get ("chaosVelocity");
    return settings;
}

void BleepForgeProcessor::gridEdited()
{
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
}

void BleepForgeProcessor::previewNote (int note, float velocity)
{
    if (! previewOn.load() || note < 0 || note > 127)
        return;
    const auto vel = (uint32_t) juce::jlimit (1, 127, juce::roundToInt (velocity * 127.0f));
    previewRequest = (1u << 16) | (vel << 8) | (uint32_t) note;
}

void BleepForgeProcessor::setPreviewEnabled (bool on)
{
    previewOn = on;
    userSettings.setValue ("previewNotes", on);
    userSettings.saveIfNeeded();
}

// Preview notes start and end on a block boundary: they don't follow the steps, so the few
// milliseconds don't matter. A preview is short, about a 16th at 100 bpm, so it shows the pitch
// and velocity without smearing into the pattern.
void BleepForgeProcessor::runPreview (int numSamples)
{
    constexpr double kPreviewSeconds = 0.15;
    if (previewPlaying >= 0)
    {
        previewSamplesLeft -= numSamples;
        if (previewSamplesLeft <= 0)
        {
            handleNote (juce::MidiMessage::noteOff (1, previewPlaying));
            previewPlaying = -1;
        }
    }

    const auto request = previewRequest.exchange (0);
    if ((request & (1u << 16)) == 0)
        return;
    if (previewPlaying >= 0)
        handleNote (juce::MidiMessage::noteOff (1, previewPlaying)); // one preview at a time
    previewPlaying = (int) (request & 0x7fu);
    previewSamplesLeft = (int) (kPreviewSeconds * getSampleRate());
    handleNote (juce::MidiMessage::noteOn (1, previewPlaying, (juce::uint8) ((request >> 8) & 0x7fu)));
}

juce::AudioProcessorEditor* BleepForgeProcessor::createEditor()
{
    return new BleepForgeEditor (*this);
}

// The grid and other non-parameter data are stored with the APVTS state, so one XML blob
// holds the whole project.
void BleepForgeProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("stateVersion", kStateVersion, nullptr);
    state.setProperty ("grid", juce::String (grid.toString()), nullptr);
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
            const int version = state.getProperty ("stateVersion", kStateVersion);
            juce::ignoreUnused (version); // migrations from older versions go here
            if (! grid.fromString (state.getProperty ("grid").toString().toStdString()))
                grid.clear();
            soundPresets.readState (state);
            // Only the parameters stay in the tree: getStateInformation adds the rest again, so
            // a reloaded state saves byte for byte the same.
            for (auto id : { "stateVersion", "grid", "presetName", "presetIndex" })
                state.removeProperty (id, nullptr);
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

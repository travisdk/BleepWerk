// Copyright (C) 2026 Henrik Juhl. Part of BleepWerk, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PresetFiles.h"

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
// Per-user preferences (the colour theme): ~/.config/BleepWerk/BleepWerk.settings on Linux. The
// first run after the rename brings the folders of the old name along (see PresetFiles.h).
juce::PropertiesFile::Options settingsOptions()
{
    bleepwerk::files::migrateLegacyUserData ("BleepWerk");
    return bleepwerk::files::settingsOptions ("BleepWerk");
}
} // namespace

BleepWerkProcessor::BleepWerkProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout()),
      userSettings (settingsOptions())
{
    // What a new instance starts as: the Log Drum sound (the defaults of the parameters
    // themselves, see starterValue; loading the preset also names it), and the set Five Over Seven
    // in A and B with both playing. The sequencer is off until it is switched on. A project or the
    // host's state replaces all of this on loading, so older projects keep theirs.
    for (int i = 0; i < soundPresets.numFactory(); ++i)
        if (soundPresets.entryName (i) == bleepwerk::kStarterPreset)
            soundPresets.load (i);
    patternPresets.loadStarterSet();
}

juce::AudioProcessorValueTreeState::ParameterLayout BleepWerkProcessor::createParameterLayout()
{
    using namespace juce;
    using bleepwerk::soundDefault;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    // Sound parameters take their range and default from bleepwerk::kSoundParams, which the
    // factory presets are checked against; only the skew or step is set here.
    auto info = [] (const char* id)
    {
        const auto* p = bleepwerk::findSoundParam (id);
        jassert (p != nullptr);
        return *p;
    };
    auto linear = [info] (const char* id, float step = 0.0f) { return NormalisableRange<float> (info (id).min, info (id).max, step); };
    auto skewed = [info] (const char* id, float centre) { return skewedRange (info (id).min, info (id).max, centre); };
    auto sound = [&params, info] (const char* id, const char* name, NormalisableRange<float> range, const char* unit = "")
    { params.push_back (floatParam (id, name, range, bleepwerk::starterValue (id), unit)); };
    auto choice = [&params] (const char* id, const char* name, const StringArray& names)
    { params.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, names, (int) bleepwerk::starterValue (id))); };
    auto namesOf = [] (const auto& list)
    {
        StringArray names;
        for (auto* name : list)
            names.add (name);
        return names;
    };

    choice ("engine", "Engine", namesOf (bleepwerk::kEngineNames));

    // Bleep engine
    choice ("bleepWave", "Wave", namesOf (bleepwerk::kBleepWaveNames));
    sound ("bleepPulseWidth", "Pulse Width", linear ("bleepPulseWidth"));
    sound ("bleepFmRatio", "FM Ratio", linear ("bleepFmRatio", 0.5f), "x");
    sound ("bleepFmAmount", "FM Amount", linear ("bleepFmAmount"));
    sound ("bleepPitchAmount", "Pitch Env", linear ("bleepPitchAmount", 0.1f), "st");
    sound ("bleepPitchDecay", "Pitch Decay", skewed ("bleepPitchDecay", 40.0f), "ms");
    sound ("bleepAttack", "Attack", skewed ("bleepAttack", 5.0f), "ms");
    sound ("bleepDecay", "Decay", skewed ("bleepDecay", 200.0f), "ms");
    sound ("bleepRelease", "Release", skewed ("bleepRelease", 50.0f), "ms");
    sound ("bleepNoise", "Noise", linear ("bleepNoise"));

    // Werk engine
    choice ("strikeExciter", "Play", namesOf (bleepwerk::kWerkExciterNames));
    sound ("strikeLength", "Werk Length", skewed ("strikeLength", 30.0f), "ms");
    sound ("strikeRing", "Ring", linear ("strikeRing"));
    sound ("strikeTone", "Werk Tone", linear ("strikeTone"));
    sound ("strikeMotion", "Motion", linear ("strikeMotion"));
    sound ("strikeSweep", "Sweep", linear ("strikeSweep", 0.1f), "st");

    // The step sequencer: switched on it plays against the host's transport. The patterns
    // themselves are stored with the project, not as parameters. The list of slots must never
    // change length: hosts store automation as 0..1 and would land on other slots.
    params.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "seqEnabled", 1 }, "Sequencer", StringArray { "Off", "On" }, 0));
    {
        StringArray slotNames;
        for (int i = 0; i < bleepwerk::kNumPatternSlots; ++i)
            slotNames.add (String::charToString ((juce_wchar) ('A' + i)));
        params.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "seqPattern", 1 }, "Sequencer Pattern", slotNames, 0));
    }
    // shown as a note name (the pattern's root), not a number. The default is C3 (MIDI 60, middle C):
    // bleeps sit high, so a root an octave lower would put most of a pattern below them
    params.push_back (std::make_unique<AudioParameterInt> (
        ParameterID { "seqRoot", 1 }, "Sequencer Root", 24, 84, 60,
        AudioParameterIntAttributes()
            .withStringFromValueFunction ([] (int v, int) { return MidiMessage::getMidiNoteName (jlimit (0, 127, v), true, true, 3); })
            .withValueFromStringFunction ([] (const String& text)
                                          {
                                              const auto t = text.trim();
                                              for (int n = 24; n <= 84; ++n) // a note name reads back as its note
                                                  if (t.equalsIgnoreCase (MidiMessage::getMidiNoteName (n, true, true, 3)))
                                                      return n;
                                              return jlimit (24, 84, t.getIntValue());
                                          })));

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
            // read as a double and limited first: an int would wrap around on an absurd number
            .withValueFromStringFunction ([] (const String& t)
                                          { return roundToInt (jlimit (0.0, 24.0, t.retainCharacters ("0123456789").getDoubleValue())); })));

    // Effects, in signal order. Bits and Rate read "Off" at the top of their values, where they
    // do nothing; their knobs run backwards so that is fully left.
    choice ("filterType", "Filter", namesOf (bleepwerk::kFilterTypeNames));
    sound ("filterCutoff", "Cutoff", skewed ("filterCutoff", 1000.0f), "Hz");
    sound ("filterResonance", "Resonance", linear ("filterResonance"));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "crushBits", 1 }, "Bits", reversedRange (linear ("crushBits")), soundDefault ("crushBits"),
        AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) { return v >= bleepwerk::kCrushBitsOff - 0.01f ? String ("Off") : String (v, 1) + " bit"; })
            .withValueFromStringFunction ([] (const String& t)
                                          {
                                              return t.trim().equalsIgnoreCase ("off") ? bleepwerk::kCrushBitsOff
                                                                                      : t.retainCharacters ("0123456789.").getFloatValue();
                                          })));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "crushRate", 1 }, "Crush Rate", reversedRange (skewed ("crushRate", 6000.0f)), soundDefault ("crushRate"),
        AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int)
                                          {
                                              if (v >= bleepwerk::kCrushRateOff - 1.0f)
                                                  return String ("Off");
                                              return v < 1000.0f ? String (roundToInt (v)) + " Hz" : String (v / 1000.0f, 1) + " kHz";
                                          })
            .withValueFromStringFunction ([] (const String& t)
                                          {
                                              const auto text = t.trim().toLowerCase();
                                              if (text == "off")
                                                  return bleepwerk::kCrushRateOff;
                                              const float value = text.retainCharacters ("0123456789.").getFloatValue();
                                              return text.contains ("k") ? value * 1000.0f : value;
                                          })));
    choice ("delayNote", "Delay Time", namesOf (bleepwerk::kDelayNoteNames));
    choice ("delayFeel", "Delay Feel", namesOf (bleepwerk::kDelayFeelNames));
    sound ("delayFeedback", "Delay Feedback", linear ("delayFeedback"));
    sound ("delayMix", "Delay Mix", linear ("delayMix"));
    sound ("roomSize", "Reverb Size", linear ("roomSize"));
    sound ("roomDecay", "Reverb Decay", skewed ("roomDecay", 1.5f), "s");
    sound ("roomTone", "Reverb Tone", linear ("roomTone"));
    sound ("roomMix", "Reverb Mix", linear ("roomMix"));
    sound ("roomDuck", "Reverb Duck", linear ("roomDuck"));
    // Drive sits first or last in the chain, so it is the one effect not in signal order here
    choice ("driveType", "Drive Type", namesOf (bleepwerk::kDriveTypeNames));
    choice ("drivePost", "Drive Place", namesOf (bleepwerk::kDrivePlaceNames));
    sound ("driveAmount", "Drive", linear ("driveAmount"));
    sound ("driveMix", "Drive Mix", linear ("driveMix"));

    sound ("outputGain", "Output Gain", linear ("outputGain", 0.1f), "dB");

    return { params.begin(), params.end() };
}

void BleepWerkProcessor::prepareToPlay (double sampleRate, int)
{
    bleep.prepare (sampleRate);
    werk.prepare (sampleRate);
    effects.prepare (sampleRate);
    noteInput.reset(); // prepare() has silenced the voices, so no key is held any more
    for (auto& layer : layers)
        layer.reset();
    numHeldKeys = 0;
    sequencerRunning = false;
    readParams();
    effects.setParams (patch.effects, hostBpm);
    outputGain.reset (sampleRate, 0.02);
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("outputGain")->load()));
}

bool BleepWerkProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void BleepWerkProcessor::readParams()
{
    patch = bleepwerk::readPatch ([this] (const char* id) { return apvts.getRawParameterValue (id)->load(); });

    bleep.setParams (patch.bleep);
    werk.setParams (patch.werk);
    bleep.setMode (patch.mono ? bleepwerk::BleepSynth::Mode::Mono : bleepwerk::BleepSynth::Mode::Poly);
    werk.setMode (patch.mono ? bleepwerk::WerkSynth::Mode::Mono : bleepwerk::WerkSynth::Mode::Poly);
    bleep.setGlideMs (patch.glideMs);
    werk.setGlideMs (patch.glideMs);
    bleep.setChaos (patch.chaos);
    werk.setChaos (patch.chaos);
    // the range may change while a bend is held
    Engines { *this }.pitchBend (noteInput.currentBend());
}

void BleepWerkProcessor::Engines::noteOn (int note, float velocity)
{
    switch (p.patch.engine)
    {
        case bleepwerk::Engine::Bleep: p.bleep.noteOn (note, velocity); break;
        case bleepwerk::Engine::Werk: p.werk.noteOn (note, velocity); break;
    }
}

void BleepWerkProcessor::Engines::noteOff (int note)
{
    p.bleep.noteOff (note);
    p.werk.noteOff (note);
}

void BleepWerkProcessor::Engines::pitchBend (float amount)
{
    const float semis = amount * p.apvts.getRawParameterValue ("bendRange")->load();
    p.bleep.setPitchBend (semis);
    p.werk.setPitchBend (semis);
}

void BleepWerkProcessor::allNotesOff()
{
    bleep.allNotesOff();
    werk.allNotesOff();
}

void BleepWerkProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
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
        werk.renderAdding (left + from, right != nullptr ? right + from : nullptr, to - from);
    };

    // The sequencer plays while it is switched on and the host's transport runs. Then the
    // keys played don't sound but set the root of the pattern (the last one held), and its
    // notes go to the engines on their exact samples, mixed in with the host's events.
    struct SeqEvent
    {
        int sample;
        bool on;
        int note;
        float velocity;
    };
    constexpr int kMaxSeqEvents = 256; // eight layers
    std::array<SeqEvent, kMaxSeqEvents> seqEvents;
    int numSeqEvents = 0;

    double ppq = 0.0;
    bool playing = false;
    const bool hasTransport = readTransport (ppq, playing);
    const bool wanted = apvts.getRawParameterValue ("seqEnabled")->load() > 0.5f && hasTransport && playing;
    if (wanted != sequencerRunning.load())
    {
        // changing between playing the keys and the pattern lets go of everything held
        noteInput.reset();
        allNotesOff();
        numHeldKeys = 0;
        externalRoot = -1;
        sequencerRunning = wanted;
    }
    // the Pattern parameter (automation) makes one slot the only one that plays
    if (const int slot = (int) apvts.getRawParameterValue ("seqPattern")->load(); slot != lastPatternParam.exchange (slot))
    {
        patternBank.playOnly (slot);
        patternBank.select (slot);
    }

    bleepwerk::SequencerSettings settings;
    settings.enabled = wanted;
    settings.rootNote = numHeldKeys > 0 ? heldKeys[(size_t) numHeldKeys - 1] : (int) apvts.getRawParameterValue ("seqRoot")->load();
    for (int layer = 0; layer < bleepwerk::kNumPatternSlots; ++layer)
    {
        // a layer that doesn't play is run as stopped, which lets go of its note
        bleepwerk::SequencerSettings layerSettings = settings;
        layerSettings.enabled = wanted && patternBank.isPlaying (layer);
        if (layerSettings.enabled || layers[(size_t) layer].getCurrentStep() >= 0)
            layers[(size_t) layer].process (layerSettings.enabled, ppq, hostBpm, getSampleRate(), numSamples, layerSettings,
                                            patternBank.slot (layer),
                                            [&] (int sample, bool on, int note, float velocity)
                                            {
                                                if (numSeqEvents < kMaxSeqEvents)
                                                    seqEvents[(size_t) numSeqEvents++] = { sample, on, note, velocity };
                                            });
    }
    // the layers' events in time order (a few, so an insertion sort; no allocation)
    for (int k = 1; k < numSeqEvents; ++k)
        for (int m = k; m > 0 && seqEvents[(size_t) m].sample < seqEvents[(size_t) m - 1].sample; --m)
            std::swap (seqEvents[(size_t) m], seqEvents[(size_t) m - 1]);

    Engines engines { *this };
    int position = 0;
    int nextSeq = 0;
    auto playSequencerUpTo = [&] (int sample)
    {
        for (; nextSeq < numSeqEvents && seqEvents[(size_t) nextSeq].sample <= sample; ++nextSeq)
        {
            const auto& e = seqEvents[(size_t) nextSeq];
            const int at = juce::jlimit (position, numSamples, e.sample);
            renderRange (position, at);
            position = at;
            if (e.on)
                engines.noteOn (e.note, e.velocity);
            else
                engines.noteOff (e.note);
        }
    };
    for (const auto metadata : midi)
    {
        const int eventPos = juce::jlimit (0, numSamples, metadata.samplePosition);
        playSequencerUpTo (eventPos);
        renderRange (position, eventPos);
        position = juce::jmax (position, eventPos);

        const auto* data = metadata.data;
        const bool isNote = metadata.numBytes >= 3 && ((data[0] & 0xf0) == 0x90 || (data[0] & 0xf0) == 0x80);
        if (sequencerRunning.load() && isNote)
        {
            const int key = data[1] & 0x7f;
            const bool down = (data[0] & 0xf0) == 0x90 && (data[2] & 0x7f) != 0;
            int* const keys = heldKeys.data(); // a pointer: an array's iterator is a class in MSVC
            numHeldKeys = (int) (std::remove (keys, keys + numHeldKeys, key) - keys);
            if (down && numHeldKeys < (int) heldKeys.size())
                heldKeys[(size_t) numHeldKeys++] = key;
            continue;
        }
        noteInput.handle (metadata.data, metadata.numBytes, engines);
    }
    playSequencerUpTo (numSamples);
    renderRange (position, numSamples);
    // for the panel only: the Root parameter is never written from MIDI
    externalRoot = numHeldKeys > 0 ? heldKeys[(size_t) numHeldKeys - 1] : -1;

    effects.setParams (patch.effects, hostBpm);
    effects.process (left, right, numSamples);

    outputGain.setTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("outputGain")->load()));
    outputGain.applyGain (buffer, numSamples);
}

// While bypassed the host's note-offs don't reach the engines, so every note is let go and
// nothing hangs once the plug-in plays again. The effects' tails are cut, as a bypass expects.
void BleepWerkProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    buffer.clear();
    noteInput.reset();
    allNotesOff();
    for (auto& layer : layers)
        layer.reset();
    numHeldKeys = 0;
    externalRoot = -1;
    sequencerRunning = false;
}

// The delay's synced time follows the host's tempo, kept from the last block that had one.
void BleepWerkProcessor::readTempo()
{
    if (auto* hostPlayHead = getPlayHead())
        if (auto position = hostPlayHead->getPosition())
            if (auto bpm = position->getBpm())
                hostBpm = *bpm;
}

// The host's position in quarter notes and whether it plays; false when it has none (a plain
// player), where the sequencer stays silent.
bool BleepWerkProcessor::readTransport (double& ppq, bool& playing)
{
    if (auto* hostPlayHead = getPlayHead())
        if (auto position = hostPlayHead->getPosition())
            if (auto pos = position->getPpqPosition())
            {
                ppq = *pos;
                playing = position->getIsPlaying();
                return true;
            }
    return false;
}

juce::File BleepWerkProcessor::patternFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("BleepWerk/Patterns");
}

juce::AudioProcessorEditor* BleepWerkProcessor::createEditor()
{
    return new BleepWerkEditor (*this);
}

// The preset name and other non-parameter data are stored with the APVTS state, so one XML
// blob holds the whole project.
void BleepWerkProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("stateVersion", kStateVersion, nullptr);
    soundPresets.writeState (state);
    patternPresets.writeState (state);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void BleepWerkProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (! bleepwerk::files::isPlausibleXml (data, (size_t) juce::jmax (0, sizeInBytes)))
        return;

    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {

            auto state = juce::ValueTree::fromXml (*xml);
            const int savedVersion = (int) state.getProperty ("stateVersion", 0);
            soundPresets.readState (state);
            patternPresets.readState (state);
            // Only the parameters stay in the tree: getStateInformation adds the rest again, so
            // a reloaded state saves byte for byte the same.
            // the patterns are saved as properties of the tree, and added again on saving
            for (auto id : { "stateVersion", "presetName", "presetIndex", "patternPlaying", "patternEdited" })
                state.removeProperty (id, nullptr);
            for (int i = 0; i < bleepwerk::kNumPatternSlots; ++i)
                for (auto prefix : { "pattern", "patternName", "patternGroove" })
                    state.removeProperty (juce::String (prefix) + juce::String::charToString ((juce::juce_wchar) ('A' + i)), nullptr);
            // A value that is not a number (a damaged project) loads as the parameter's default,
            // as its range only clamps numbers; an old delay time like that is dropped.
            for (int i = state.getNumChildren(); --i >= 0;)
            {
                auto child = state.getChild (i);
                if (! std::isnan ((double) child.getProperty ("value")))
                    continue;
                if (auto* param = apvts.getParameter (child.getProperty ("id").toString()))
                    child.setProperty ("value", param->convertFrom0to1 (param->getDefaultValue()), nullptr);
                else
                    state.removeChild (i, nullptr);
            }
            // before version 5 the Werk exciter choices were longer (see migrateWerkExciter)
            if (savedVersion < 5)
                for (int i = 0; i < state.getNumChildren(); ++i)
                {
                    auto child = state.getChild (i);
                    const auto id = child.getProperty ("id").toString();
                    if (id == "strikeExciter")
                        child.setProperty ("value", (float) bleepwerk::migrateWerkExciter (juce::roundToInt ((float) child.getProperty ("value"))), nullptr);
                }
            // before version 3 the delay time was one list; it becomes a time and a feel
            if (auto old = state.getChildWithProperty ("id", "delayTime"); old.isValid())
            {
                int note = 0, feel = 0;
                bleepwerk::splitOldDelayTime (juce::roundToInt ((float) old.getProperty ("value")), note, feel);
                for (auto [id, value] : { std::pair { "delayNote", note }, std::pair { "delayFeel", feel } })
                {
                    juce::ValueTree param ("PARAM");
                    param.setProperty ("id", id, nullptr);
                    param.setProperty ("value", value, nullptr);
                    state.appendChild (param, nullptr);
                }
            }
            // a sound parameter an older project doesn't have loads as its old default, not as the
            // starting sound that the parameters declare as theirs now
            for (const auto& sound : bleepwerk::kSoundParams)
                if (! state.getChildWithProperty ("id", sound.id).isValid())
                {
                    juce::ValueTree param ("PARAM");
                    param.setProperty ("id", sound.id, nullptr);
                    param.setProperty ("value", sound.defaultValue, nullptr);
                    state.appendChild (param, nullptr);
                }
            for (int i = state.getNumChildren(); --i >= 0;)
                if (apvts.getParameter (state.getChild (i).getProperty ("id").toString()) == nullptr)
                    state.removeChild (i, nullptr);
            apvts.replaceState (state);
            lastPatternParam = (int) apvts.getRawParameterValue ("seqPattern")->load(); // no solo from loading
            // Tells the host every parameter may have changed, so it reads them all again
            // (CLAP: params rescan of the values, which the wrapper sends for a program change).
            updateHostDisplay (ChangeDetails().withProgramChanged (true));
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BleepWerkProcessor();
}

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
        const auto number = std::abs (value) < 10.0f ? juce::String (value, 1) : juce::String (juce::roundToInt (value));
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
} // namespace

BleepForgeProcessor::BleepForgeProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    bleepforge::loadStarterPattern (grid);
}

juce::AudioProcessorValueTreeState::ParameterLayout BleepForgeProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    StringArray waves;
    for (auto* name : bleepforge::kBleepWaveNames)
        waves.add (name);

    // Bleep engine
    params.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "bleepWave", 1 }, "Wave", waves, 0));
    params.push_back (floatParam ("bleepPulseWidth", "Pulse Width", { 0.05f, 0.95f }, 0.5f));
    params.push_back (floatParam ("bleepFmRatio", "FM Ratio", { 0.5f, 8.0f, 0.5f }, 2.0f, "x"));
    params.push_back (floatParam ("bleepFmAmount", "FM Amount", { 0.0f, 1.0f }, 0.3f));
    params.push_back (floatParam ("bleepPitchAmount", "Pitch Env", { -24.0f, 48.0f, 0.1f }, 12.0f, "st"));
    params.push_back (floatParam ("bleepPitchDecay", "Pitch Decay", skewedRange (1.0f, 500.0f, 40.0f), 25.0f, "ms"));
    params.push_back (floatParam ("bleepAttack", "Attack", skewedRange (0.0f, 50.0f, 5.0f), 0.5f, "ms"));
    params.push_back (floatParam ("bleepDecay", "Decay", skewedRange (5.0f, 2000.0f, 200.0f), 180.0f, "ms"));
    params.push_back (floatParam ("bleepRelease", "Release", skewedRange (1.0f, 500.0f, 50.0f), 30.0f, "ms"));
    params.push_back (floatParam ("bleepNoise", "Noise", { 0.0f, 1.0f }, 0.0f));

    // Sequencer
    StringArray scales, roots;
    for (const auto& scale : bleepforge::kScales)
        scales.add (scale.name);
    for (auto* name : bleepforge::kRootNames)
        roots.add (name);
    params.push_back (std::make_unique<AudioParameterBool> (ParameterID { "seqOn", 1 }, "Sequencer", true));
    params.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "seqScale", 1 }, "Scale", scales,
                                                              bleepforge::kDefaultScale));
    params.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "seqRoot", 1 }, "Root", roots, 0));
    params.push_back (std::make_unique<AudioParameterInt> (ParameterID { "seqOctave", 1 }, "Octave", -3, 3, 0));

    // Voices
    params.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "voiceMode", 1 }, "Voice Mode",
                                                              StringArray { "Mono", "Poly" }, 1));
    params.push_back (floatParam ("glide", "Glide", skewedRange (0.0f, 500.0f, 60.0f), 0.0f, "ms"));

    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "outputGain", 1 }, "Output Gain",
                                                             NormalisableRange<float> (-48.0f, 6.0f, 0.1f), 0.0f,
                                                             AudioParameterFloatAttributes().withLabel ("dB")));

    return { params.begin(), params.end() };
}

void BleepForgeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    mono.assign ((size_t) juce::jmax (1, samplesPerBlock), 0.0f);
    voiceMidi.ensureSize (4096);
    bleep.prepare (sampleRate);
    readParams();
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
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    bleepforge::BleepParams p;
    p.wave = (bleepforge::BleepWave) juce::jlimit (0, bleepforge::kNumBleepWaves - 1, (int) get ("bleepWave"));
    p.pulseWidth = get ("bleepPulseWidth");
    p.fmRatio = get ("bleepFmRatio");
    p.fmAmount = get ("bleepFmAmount");
    p.pitchEnvSemis = get ("bleepPitchAmount");
    p.pitchEnvDecayMs = get ("bleepPitchDecay");
    p.attackMs = get ("bleepAttack");
    p.decayMs = get ("bleepDecay");
    p.releaseMs = get ("bleepRelease");
    p.noise = get ("bleepNoise");
    bleep.setParams (p);

    bleep.setMode ((int) get ("voiceMode") == 0 ? bleepforge::BleepSynth::Mode::Mono : bleepforge::BleepSynth::Mode::Poly);
    bleep.setGlideMs (get ("glide"));
}

void BleepForgeProcessor::handleMidi (const juce::MidiMessage& message)
{
    if (message.isNoteOn())
        bleep.noteOn (message.getNoteNumber(), message.getFloatVelocity());
    else if (message.isNoteOff())
        bleep.noteOff (message.getNoteNumber());
    else if (message.isAllNotesOff() || message.isAllSoundOff())
        bleep.allNotesOff();
    else if (message.isPitchWheel())
        bleep.setPitchBend ((float) (message.getPitchWheelValue() - 8192) / 8192.0f * 2.0f);
}

void BleepForgeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    readParams();

    const int numSamples = buffer.getNumSamples();
    buffer.clear();
    buildVoiceMidi (midi, numSamples);

    // Render in chunks between MIDI events so notes start on their exact sample. The scratch
    // buffer is sized in prepareToPlay; a host block larger than promised is done in pieces.
    const int chunkMax = (int) mono.size();
    auto renderRange = [&] (int from, int to)
    {
        while (from < to)
        {
            const int length = juce::jmin (chunkMax, to - from);
            bleep.render (mono.data(), length);
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.copyFrom (ch, from, mono.data(), length);
            from += length;
        }
    };

    int position = 0;
    for (const auto metadata : voiceMidi)
    {
        const int eventPos = juce::jlimit (0, numSamples, metadata.samplePosition);
        renderRange (position, eventPos);
        position = juce::jmax (position, eventPos);
        handleMidi (metadata.getMessage());
    }
    renderRange (position, numSamples);

    // Incoming MIDI is consumed here: with producesMidi() whatever is left in the buffer goes
    // to the host, and MIDI thru will be an explicit option later.
    midi.clear();

    outputGain.setTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("outputGain")->load()));
    outputGain.applyGain (buffer, numSamples);
}

// Incoming MIDI plays the engine whether the sequencer runs or not; the sequencer's notes are
// merged in. Steps follow the host's position, so the sequencer only runs while it plays.
void BleepForgeProcessor::buildVoiceMidi (const juce::MidiBuffer& midi, int numSamples)
{
    double bpm = 120.0, ppq = 0.0;
    bool playing = false;
    if (auto* hostPlayHead = getPlayHead())
    {
        if (auto position = hostPlayHead->getPosition())
        {
            if (auto hostBpm = position->getBpm())
                bpm = *hostBpm;
            if (auto hostPpq = position->getPpqPosition(); hostPpq && position->getIsPlaying())
            {
                ppq = *hostPpq;
                playing = true;
            }
        }
    }

    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    bleepforge::SequencerSettings settings;
    settings.enabled = get ("seqOn") >= 0.5f;
    settings.scale = (int) get ("seqScale");
    settings.rootNote = 60 + (int) get ("seqRoot") + 12 * (int) get ("seqOctave");

    voiceMidi.clear();
    voiceMidi.addEvents (midi, 0, numSamples, 0);
    sequencer.process (playing, ppq, bpm, getSampleRate(), numSamples, settings, grid,
                       [this] (int sample, bool on, int note, float velocity)
                       {
                           voiceMidi.addEvent (on ? juce::MidiMessage::noteOn (1, note, velocity)
                                                  : juce::MidiMessage::noteOff (1, note), sample);
                       });
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
                bleepforge::loadStarterPattern (grid);
            state.removeProperty ("grid", nullptr);
            apvts.replaceState (state);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BleepForgeProcessor();
}

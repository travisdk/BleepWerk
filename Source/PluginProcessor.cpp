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

    params.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "engine", 1 }, "Engine",
                                                              StringArray { "Bleep", "Foley" }, 0));

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

    // Foley engine
    StringArray exciters;
    for (auto* name : bleepforge::kFoleyExciterNames)
        exciters.add (name);
    params.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "foleyExciter", 1 }, "Exciter", exciters,
                                                              (int) bleepforge::FoleyExciter::Mallet));
    params.push_back (floatParam ("foleyLength", "Exciter Length", skewedRange (0.05f, 200.0f, 5.0f), 0.3f, "ms"));
    // Material shows the material it sounds like rather than a percentage
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "foleyMaterial", 1 }, "Material", NormalisableRange<float> (0.0f, 1.0f), 0.0f,
        AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int)
                                          {
                                              return String (v < 0.15f ? "Wood" : v < 0.4f ? "Wood/Metal" : v < 0.6f ? "Metal"
                                                                         : v < 0.85f ? "Metal/Glass" : "Glass");
                                          })
            .withValueFromStringFunction ([] (const String& text)
                                          {
                                              const auto t = text.trim().toLowerCase();
                                              if (t == "wood") return 0.0f;
                                              if (t == "metal") return 0.5f;
                                              if (t == "glass") return 1.0f;
                                              return t.retainCharacters ("0123456789.").getFloatValue() / 100.0f;
                                          })));
    params.push_back (floatParam ("foleySize", "Size", { 0.0f, 1.0f }, 0.5f));
    params.push_back (floatParam ("foleyDamping", "Damping", { 0.0f, 1.0f }, 0.5f));
    params.push_back (floatParam ("foleyBrightness", "Brightness", { 0.0f, 1.0f }, 0.5f));
    params.push_back (floatParam ("foleySweep", "Sweep", { -24.0f, 24.0f, 0.1f }, 0.0f, "st"));

    // Sequencer
    StringArray scales, roots;
    for (const auto& scale : bleepforge::kScales)
        scales.add (scale.name);
    for (auto* name : bleepforge::kRootNames)
        roots.add (name);
    params.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "seqScale", 1 }, "Scale", scales,
                                                              bleepforge::kDefaultScale));
    params.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "seqRoot", 1 }, "Root", roots, 0));
    StringArray rates;
    for (auto* name : bleepforge::kSeqRateNames)
        rates.add (name);
    params.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "seqRate", 1 }, "Rate", rates, bleepforge::kDefaultSeqRate));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "seqSwing", 1 }, "Swing", NormalisableRange<float> (0.5f, 0.75f, 0.005f), 0.5f,
        AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })
            .withValueFromStringFunction ([] (const String& t) { return t.retainCharacters ("0123456789.").getFloatValue() / 100.0f; })));

    // Chaos: Amount scales every target's depth
    params.push_back (floatParam ("chaosAmount", "Chaos", { 0.0f, 1.0f }, 0.25f));
    params.push_back (floatParam ("chaosPitch", "Chaos Pitch", { 0.0f, 1.0f }, 0.2f));
    params.push_back (floatParam ("chaosDecay", "Chaos Decay", { 0.0f, 1.0f }, 0.5f));
    params.push_back (floatParam ("chaosTone", "Chaos Tone", { 0.0f, 1.0f }, 0.5f));
    params.push_back (floatParam ("chaosPan", "Chaos Pan", { 0.0f, 1.0f }, 0.5f));
    params.push_back (floatParam ("chaosVelocity", "Chaos Velocity", { 0.0f, 1.0f }, 0.3f));

    // how many octaves the grid shows at a time: a view setting, saved with the project
    params.push_back (std::make_unique<AudioParameterInt> (ParameterID { "seqRange", 1 }, "Visible Range", 1, kMaxVisibleOctaves,
                                                           2, AudioParameterIntAttributes().withAutomatable (false)));

    // Voices
    params.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "voiceMode", 1 }, "Voice Mode",
                                                              StringArray { "Mono", "Poly" }, 1));
    params.push_back (floatParam ("glide", "Glide", skewedRange (0.0f, 500.0f, 60.0f), 0.0f, "ms"));

    // Effects, in signal order. Bits and Rate read "Off" at the top, where they do nothing.
    StringArray filterTypes, delayTimes;
    for (auto* name : bleepforge::kFilterTypeNames)
        filterTypes.add (name);
    for (auto* name : bleepforge::kDelayTimeNames)
        delayTimes.add (name);
    params.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "filterType", 1 }, "Filter", filterTypes, 0));
    params.push_back (floatParam ("filterCutoff", "Cutoff", skewedRange (20.0f, 20000.0f, 1000.0f), 2000.0f, "Hz"));
    params.push_back (floatParam ("filterResonance", "Resonance", { 0.0f, 1.0f }, 0.2f));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "crushBits", 1 }, "Bits", NormalisableRange<float> (1.0f, bleepforge::kCrushBitsOff), bleepforge::kCrushBitsOff,
        AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) { return v >= bleepforge::kCrushBitsOff - 0.01f ? String ("Off") : String (v, 1) + " bit"; })
            .withValueFromStringFunction ([] (const String& t)
                                          {
                                              return t.trim().equalsIgnoreCase ("off") ? bleepforge::kCrushBitsOff
                                                                                      : t.retainCharacters ("0123456789.").getFloatValue();
                                          })));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "crushRate", 1 }, "Crush Rate", skewedRange (200.0f, bleepforge::kCrushRateOff, 6000.0f), bleepforge::kCrushRateOff,
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
    params.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "delayTime", 1 }, "Delay Time", delayTimes,
                                                              bleepforge::kDefaultDelayTime));
    params.push_back (floatParam ("delayFeedback", "Delay Feedback", { 0.0f, 0.95f }, 0.35f));
    params.push_back (floatParam ("delayMix", "Delay Mix", { 0.0f, 1.0f }, 0.0f));
    params.push_back (floatParam ("roomSize", "Room Size", { 0.0f, 1.0f }, 0.4f));
    params.push_back (floatParam ("roomMix", "Room Mix", { 0.0f, 1.0f }, 0.0f));

    params.push_back (floatParam ("outputGain", "Output Gain", { -48.0f, 6.0f, 0.1f }, 0.0f, "dB"));

    return { params.begin(), params.end() };
}

void BleepForgeProcessor::prepareToPlay (double sampleRate, int)
{
    seqMidi.ensureSize (4096);
    bleep.prepare (sampleRate);
    foley.prepare (sampleRate);
    effects.prepare (sampleRate);
    readParams();
    readEffectParams();
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

    bleepforge::FoleyParams f;
    f.exciter = (bleepforge::FoleyExciter) juce::jlimit (0, bleepforge::kNumFoleyExciters - 1, (int) get ("foleyExciter"));
    f.exciterMs = get ("foleyLength");
    f.material = get ("foleyMaterial");
    f.size = get ("foleySize");
    f.damping = get ("foleyDamping");
    f.brightness = get ("foleyBrightness");
    f.sweepSemis = get ("foleySweep");
    foley.setParams (f);

    const bool monoMode = (int) get ("voiceMode") == 0;
    bleep.setMode (monoMode ? bleepforge::BleepSynth::Mode::Mono : bleepforge::BleepSynth::Mode::Poly);
    foley.setMode (monoMode ? bleepforge::FoleySynth::Mode::Mono : bleepforge::FoleySynth::Mode::Poly);
    bleep.setGlideMs (get ("glide"));
    foley.setGlideMs (get ("glide"));

    const float amount = get ("chaosAmount");
    bleepforge::ChaosDepths chaos;
    chaos.pitch = amount * get ("chaosPitch");
    chaos.decay = amount * get ("chaosDecay");
    chaos.tone = amount * get ("chaosTone");
    chaos.pan = amount * get ("chaosPan");
    bleep.setChaos (chaos);
    foley.setChaos (chaos);
    foleySelected = (int) get ("engine") == 1;
}

void BleepForgeProcessor::readEffectParams()
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    bleepforge::EffectParams e;
    e.filterType = (bleepforge::FilterType) juce::jlimit (0, bleepforge::kNumFilterTypes - 1, (int) get ("filterType"));
    e.cutoffHz = get ("filterCutoff");
    e.resonance = get ("filterResonance");
    e.crushBits = get ("crushBits");
    e.crushRateHz = get ("crushRate");
    e.delayTime = (int) get ("delayTime");
    e.delayFeedback = get ("delayFeedback");
    e.delayMix = get ("delayMix");
    e.roomSize = get ("roomSize");
    e.roomMix = get ("roomMix");
    effects.setParams (e, hostBpm);
}

void BleepForgeProcessor::handleNote (const juce::MidiMessage& message)
{
    // note-offs go to both, so a note that started before an engine switch still ends
    if (message.isNoteOn())
    {
        if (foleySelected)
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
    readEffectParams();
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
    settings.velocityChaos = get ("chaosAmount") * get ("chaosVelocity");
    return settings;
}

void BleepForgeProcessor::gridEdited()
{
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
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

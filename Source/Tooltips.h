// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <cstring>

// The hover text of every control that is tied to a parameter, by parameter id. The knobs,
// drop-downs and choice buttons look their text up here, so a new parameter needs an entry
// here or it has no tooltip (test_tooltips checks the sound parameters have one). JUCE-free.
namespace bleepforge::tooltips
{
struct Entry
{
    const char* id;
    const char* text;
};

inline constexpr Entry kEntries[] = {
    // the whole instrument
    { "engine", "Which engine plays: Bleep (a tone with a pitch sweep) or Strike (something hits something)" },
    { "voiceMode", "Mono plays one note at a time, the last one held wins; Poly plays chords" },
    { "glide", "How long the pitch takes to slide to a new note in Mono; 0 is no glide" },
    { "bendRange", "How far the pitch wheel bends the note, in semitones" },
    { "outputGain", "Output level of the whole instrument" },

    // Bleep
    { "bleepWave", "The oscillator: Sine, Saw, Pulse (its width is the Pulse Width knob) or FM (two operators, set by FM Ratio and FM Amount)" },
    { "bleepPulseWidth", "Width of the pulse wave; 50% is a square wave" },
    { "bleepFmRatio", "Frequency of the FM modulator, as a multiple of the note" },
    { "bleepFmAmount", "How strongly the modulator bends the wave; 0 is a plain sine" },
    { "bleepPitchAmount", "How many semitones the pitch starts away from the note, and falls back to it" },
    { "bleepPitchDecay", "How fast the pitch falls back to the note" },
    { "bleepAttack", "How long the sound takes to reach full level" },
    { "bleepDecay", "How long the sound takes to fade out after the attack" },
    { "bleepRelease", "How long the sound fades after the key is let go, if it is still sounding" },
    { "bleepNoise", "How much noise is mixed into the bleep" },

    // Strike
    { "strikeExciter", "What hits: Knock (a short click), Noise (a burst of noise), Steam (a steady hiss with a puff) or Friction (a bright tone that slides into the note)" },
    { "strikeBody", "What is hit: Wood (a free bar) or Membrane (a drum skin)" },
    { "strikeLength", "How long the burst or the steady sound lasts; not used by Knock" },
    { "strikeRing", "How long the body rings after the hit. For Steam and Friction it is also how much of the body (Wood or Membrane) you hear: turn it up to hear the difference" },
    { "strikeTone", "Dark to bright: how hard the hit is and how loud the high parts are" },
    { "strikeMotion", "Movement: a rattle for Knock, a flutter for Steam, a waver for Friction; not used by Noise" },
    { "strikeSweep", "The body starts this many semitones from the note and slides in; negative starts above" },

    // Chaos: every hit is varied at random, by Amount times each setting
    { "chaosAmount", "How much every hit is varied at random; 0 is none, so a sound plays the same each time" },
    { "chaosPitch", "How much the pitch of each hit varies" },
    { "chaosDecay", "How much the decay of each hit varies" },
    { "chaosTone", "How much the tone of each hit varies" },
    { "chaosPan", "How much the stereo position of each hit varies" },
    { "chaosVelocity", "How much the velocity of each hit varies" },

    // Effects, in chain order: filter, crusher, delay, reverb, with Drive first or last
    { "filterType", "Filter: Off, LP (low-pass), BP (band-pass) or HP (high-pass)" },
    { "filterCutoff", "The frequency the filter cuts at" },
    { "filterResonance", "Emphasis at the cutoff frequency; high values ring" },
    { "crushBits", "Bit depth: fewer bits sound grittier; at the top the crusher is off" },
    { "crushRate", "Sample rate of the crusher: lower holds samples for a rougher sound; at the top it is off" },
    { "delayNote", "Delay time as a note value, synced to the host tempo" },
    { "delayFeel", "Straight, triplet or dotted: makes the delay time a triplet or dotted note" },
    { "delayFeedback", "How much of each repeat is fed back into the delay" },
    { "delayMix", "How much of the delay is heard" },
    { "roomSize", "Size of the reverb room" },
    { "roomDecay", "How long the reverb tail lasts" },
    { "roomTone", "Dark to bright: how much the reverb tail is damped" },
    { "roomMix", "How much of the reverb is heard" },
    { "roomDuck", "How much the reverb is turned down while the sound plays, so the tail shows in the gaps" },
    { "driveType", "Distortion: Warm (soft saturation), Hard (fuzz clipping) or Fold (a wavefolder)" },
    { "drivePost", "Where the distortion sits in the chain: Pre is first, Post is last" },
    { "driveAmount", "How hard the signal is pushed into the distortion" },
    { "driveMix", "How much of the distortion is heard against the clean signal" },

    // the sequencer's root note, a parameter that the sequencer panel shows
    { "seqRoot", "The note the pattern is played from; keys you play while it runs set it instead" },
};

// The tooltip for a parameter id, or nullptr if it has none.
inline const char* forParameter (const char* id)
{
    for (const auto& entry : kEntries)
        if (std::strcmp (entry.id, id) == 0)
            return entry.text;
    return nullptr;
}
} // namespace bleepforge::tooltips

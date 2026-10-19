# BleepForge: Project Brief (working name)

## 1. Concept

BleepForge is a melodic instrument plugin for hypnotic and minimal techno. It produces short, percussive, organic sounds: classic bleeps and blips plus foley-like tones (drips, metallic tinks, wooden knocks, crackles). It is played from MIDI notes: sequencing is done in the DAW. (Until 2026-10-19 it had a built-in probability grid sequencer; that version is kept on the `with-sequencer` branch and tag `v0.9-with-sequencer`.)

Hypnotic techno (target since 2026-10-18) leans on long loops that drift out of phase, slow change over many bars, rolling polyrhythms and space (dub delay, room). Features are weighed against that.

Everything is synthesized. No samples, no third-party audio content.

## 2. Technical frame

- Framework: JUCE 8, C++
- Formats: CLAP and VST3
- Platforms: Linux and Windows
- License: open source, free (same approach as SubForge)
- Build: CMake
- Parameters via AudioProcessorValueTreeState, full state save/restore (projects saved with the sequencer version load, their grid is ignored)

## 3. Hard constraints

- **No samples or wavetables from any external source.** All sound comes from DSP code. Factory presets are parameter sets only.
- **No copied code from other plugins.** Do not copy source code, graphics or presets from any other project.
- **Real-time safety:** no allocations, locks or file I/O on the audio thread.
- MIDI input is sample-accurate: each note starts on the sample the host puts it on.
- No hanging notes: note-offs, sustain release, all-notes-off and bypass always end what plays.

## 4. Sound engine

### 4.1 Voice types

Each note is rendered by one of three engines, selectable per preset:

**Bleep engine**
- Oscillator: sine, triangle, square/pulse (PWM), simple 2-operator FM
- Fast pitch envelope (amount, decay) for the classic "blip"
- Short amp envelope (attack, decay, optional short release)
- Optional noise blend

**Foley engine**
- Exciter: noise burst, click or impulse (shape and length adjustable)
- Modal resonator bank (a small number of tuned resonators)
- High-level controls: Material (wood ↔ metal ↔ glass), Size, Damping, Brightness
- Goal: drips, tinks, knocks, crackles by turning a few knobs

**Machine engine** (added 2026-10-18)
- Industrial sounds tuned to the note: Hit (a click and noise striking the part, with a deep
  rumble and a short hiss), Hiss (noise through a bandpass, steam), Hum (a driven sine with a
  rasp, motors and transformers), Grind (noise into a comb filter)
- Every type runs into a tuned metal resonator (8 inharmonic modes that follow the note) and a
  light saturation
- Five controls shared by all four types: Length (ends by itself, up to 6 s), Tone, Motion
  (0 still, then a slow wave to a fast rattle), Focus (airy noise to a clear tone; on Hit, how
  much rumble and hiss), Metal (dry source to about a second of ringing metal)

### 4.2 Voice mode

- Selectable: **Mono** (one note at a time, classic bleep line, with optional glide; the last key held wins, and letting go of it returns to the key still held) or **Poly** (up to 8 notes at once)

### 4.3 Per-hit variation ("Chaos")

- Global Chaos amount plus per-target depth: pitch, decay, tone/brightness, pan, velocity
- Randomization is per triggered note, so repeated notes never sound identical

### 4.4 Effects (simple, in this order)

- Filter (LP/BP/HP, resonance)
- Bitcrush / sample rate reduction
- Short tempo-synced delay
- Plate reverb (Dattorro)

## 5. MIDI input

- Notes come from the host on any channel, velocity included; sequencing, probability and
  pattern variation are done in the DAW (Bitwig's note probability and randomisation, for one)
- Note-offs (also note-on at velocity 0), the sustain pedal (CC 64), all-notes-off and
  all-sound-off (CC 123, 120, which also let go of the pedal) and reset-all-controllers
  (CC 121: pedal up, bend centred)
- Pitch bend on all three engines, with a Bend Range of 0 to 24 semitones (default 2). It is a
  setting of the player, so sound presets leave it alone
- No MIDI output
- **Dice**: rolls a random sound for the selected engine (curated ranges, output gain
  matched), with a back arrow that undoes the last roll

## 6. UI

- Top: title, preset browser, settings
- Then the engine section (Bleep / Foley / Machine, with Voices, Glide, Bend and Output), and
  below it Chaos and the effects
- Resizable window
- Own visual identity, not imitating any other plugin

## 7. Presets

- Factory presets defined in code or as bundled parameter files (no audio content)
- A large factory set (over 100), grouped by category: bleeps, pulses, FM, noise, lo-fi, dub, wood, metal, machine, glass, drips, textures
- Sound presets never change the Bend Range

## 8. Quality and testing

- JUCE-free unit tests for the engines, effects, presets, dice and MIDI input (sustain,
  all-notes-off, mono last-note priority, pitch bend)
- Validate builds with pluginval
- Test in at least one host on each platform (Bitwig on Linux and Windows is the primary host)

## 9. Milestones

1. **Skeleton:** JUCE 8 project with CMake, CLAP + VST3 build on Linux and Windows, empty UI, APVTS in place
2. **Bleep engine**, mono/poly
3. **Foley engine**
4. **Chaos**
5. **Effects**
6. **Presets, state save/restore, polish**
7. **MIDI input instead of the built-in sequencer** (2026-10-19)
8. **Testing and release** (README, license, build artifacts)

## 10. Open for later

- Final product name
- Hypnotic techno, considered 2026-10-18: slow built-in drift (LFOs over 4 to 64 bars on cutoff, FM amount, brightness), wetter and longer sounds from the dice and a "Hypnotic" preset category

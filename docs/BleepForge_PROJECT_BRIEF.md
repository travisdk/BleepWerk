# BleepForge: Project Brief (working name)

## 1. Concept

BleepForge is a melodic instrument plugin for hypnotic and minimal techno. It produces short, percussive, organic sounds: classic bleeps and blips plus foley-like tones (drips, metallic tinks, wooden knocks, crackles). It is played from MIDI notes: sequencing is done in the DAW.

Hypnotic techno (target since 2026-10-18) leans on long loops that drift out of phase, slow change over many bars, rolling polyrhythms and space (dub delay, room). Features are weighed against that.

Everything is synthesized. No samples, no third-party audio content.

## 2. Technical frame

- Framework: JUCE 8, C++
- Formats: CLAP and VST3
- Platforms: Linux and Windows
- License: open source, free (same approach as SubForge)
- Build: CMake
- Parameters via AudioProcessorValueTreeState, full state save/restore

## 3. Hard constraints

- **No samples or wavetables from any external source.** All sound comes from DSP code. Factory presets are parameter sets only.
- **No copied code from other plugins.** Do not copy source code, graphics or presets from any other project.
- **Real-time safety:** no allocations, locks or file I/O on the audio thread.
- MIDI input is sample-accurate: each note starts on the sample the host puts it on.
- No hanging notes: note-offs, sustain release, all-notes-off and bypass always end what plays.

## 4. Sound engine

### 4.1 Voice types

Each note is rendered by one of two engines, selectable per preset:

**Bleep engine**
- Oscillator: sine, saw, square/pulse (PWM), simple 2-operator FM
- Fast pitch envelope (amount, decay) for the classic "blip"
- Short amp envelope (attack, decay, optional short release)
- Optional noise blend

**Strike engine** (replaced the Foley and Machine engines, 2026-10-31)
Something hits something: two choices and five knobs.
- Hit with (the exciter): Knock (a short pulse with a little noise bite; Tone is how hard),
  Noise (a decaying burst), Crackle (sparse clicks), Steam (steady noise through a bandpass on
  the note, opening with a puff) and Friction (metal rubbing on metal: a bright tone two octaves
  up that slides into the note, with friction hiss; Motion makes it waver, chatter and jump)
- Hit on (the body): Wood (a free bar), Metal (a hard part, between a bar and a plate) or
  Membrane (a drum skin: the Bessel zeros of a circular membrane). Eight tuned two-pole
  resonators that follow the note
- Length (a burst, or how long Steam and Friction last), Ring (how long the body rings), Tone
  (dark to bright), Motion (a rattle, a flutter, a waver) and Sweep (the body slides into the
  note: drips, a drum's fall)
- Heavy industrial sounds come from a hard-hit metal body with Drive and reverb, not from a
  special noise engine

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
- Then the engine section (Bleep / Strike, with Voices, Glide, Bend and Output), and
  below it Chaos, Drive and the effects
- A SEQ button in the header folds a step sequencer out below the effects (see 6.1); folded in,
  the sequencer is off and the plug-in is as without one
- Resizable window
- Own visual identity, not imitating any other plugin

### 6.1 Step sequencer (added 2026-10-31)

Adapted from SubForge's: sixteen steps (on or off, a note from the root, an accent), eight
patterns A-H, locked to the host's transport (sample-accurate, loops and jumps handled).
- While it is on and the host plays, played keys set the root of the pattern instead of
  sounding; otherwise the keys play as usual. Switching lets go of everything held
- Any pattern can play as a layer, any number at once, each with its own length, counted from
  beat 0 so they stay in step (a polymeter: 16 against 12 meets again after 48)
- Per pattern: Length, Rate, Swing, Gate, Drift (moves on a few steps every pass), Hits (spreads
  that many hits evenly: 3 of 8 is the tresillo), and Vary with a shape (Walk, Random, Skip),
  which lets the length change from pass to pass
- A factory set of original patterns, user pattern files (.bfpattern), a dice, copy to slot
- MIDI export of the shown pattern as a one-pass clip; on Windows it can be dragged into the host
- No slide (a step is a short strike) and no MIDI slot notes: the Pattern parameter, which the
  host can automate, makes one pattern the only one that plays

## 7. Presets

- Factory presets defined in code or as bundled parameter files (no audio content)
- A large factory set (over 100), grouped by category: bleeps, pulses, FM, lo-fi, dub, wood, drums, industrial, air and friction, drips, textures, drive
- Sound presets never change the Bend Range

## 8. Quality and testing

- Linux is the only target for now (decided 2026-10-31). The Windows code paths (the package-windows
  target, the install notes, the MIDI drag-out) stay but are not built or tested, and the CI
  matrix has no Windows job; the last Windows build error found, the std::array iterator, is fixed

- JUCE-free unit tests for the engines, effects, presets, dice and MIDI input (sustain,
  all-notes-off, mono last-note priority, pitch bend)
- Validate builds with pluginval
- Test in at least one host on each platform (Bitwig on Linux and Windows is the primary host)

## 9. Milestones

1. **Skeleton:** JUCE 8 project with CMake, CLAP + VST3 build on Linux and Windows, empty UI, APVTS in place
2. **Bleep engine**, mono/poly
3. **Foley engine** (later joined with Machine as the Strike engine)
4. **Chaos**
5. **Effects**
6. **Presets, state save/restore, polish**
7. **MIDI input** (2026-10-19)
8. **Testing and release** (README, license, build artifacts)
9. **Strike engine** and **step sequencer** (2026-10-31)

## 10. Open for later

- Final product name
- Hypnotic techno, considered 2026-10-18: slow built-in drift (LFOs over 4 to 64 bars on cutoff, FM amount, brightness), wetter and longer sounds from the dice and a "Hypnotic" preset category

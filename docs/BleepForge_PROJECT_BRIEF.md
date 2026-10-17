# BleepForge: Project Brief (working name)

## 1. Concept

BleepForge is a melodic instrument plugin for minimal techno. It produces short, percussive, organic sounds: classic bleeps and blips plus foley-like tones (drips, metallic tinks, wooden knocks, crackles). It has a built-in probability grid sequencer inspired by the UX of Stochas, so patterns vary subtly on every pass.

Everything is synthesized. No samples, no third-party audio content.

## 2. Technical frame

- Framework: JUCE 8, C++
- Formats: CLAP and VST3
- Platforms: Linux and Windows
- License: open source, free (same approach as SubForge)
- Build: CMake
- Parameters via AudioProcessorValueTreeState, full state save/restore (including sequencer grid)

## 3. Hard constraints

- **No samples or wavetables from any external source.** All sound comes from DSP code. Factory presets are parameter sets only.
- **No copied code from other plugins.** Stochas (and other open source sequencers) may inspire the UX concept, but the implementation must be our own. Do not copy source code, graphics or presets from any other project.
- **Real-time safety:** no allocations, locks or file I/O on the audio thread.
- Sequencer must be sample-accurate and follow host tempo, transport and position.

## 4. Sound engine

### 4.1 Voice types

Each note is rendered by one of two engines, selectable per preset (later perhaps per row):

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

### 4.2 Voice mode

- Selectable: **Mono** (one note at a time, classic bleep line, with optional glide) or **Poly** (several rows may sound at once)

### 4.3 Per-hit variation ("Chaos")

- Global Chaos amount plus per-target depth: pitch, decay, tone/brightness, pan
- Randomization is per triggered note, so repeated notes never sound identical

### 4.4 Effects (simple, in this order)

- Filter (LP/BP/HP, resonance)
- Bitcrush / sample rate reduction
- Short tempo-synced delay
- Small room reverb

## 5. Sequencer (probability grid)

UX inspired by Stochas, own implementation.

- **Rows = notes**: follow the selected scale within one octave, Stochas style (rows show the scale degrees, not all 12 chromatic notes)
- **Scale selector**: e.g. chromatic, minor, major, Phrygian, Dorian, minor pentatonic
- **Root note** selector and **octave shift**
- **Columns = steps**: 16 by default, selectable length (e.g. 1 to 32 steps) to allow polymeter against the DAW
- **Step rate**: 1/8, 1/16, 1/32, triplets
- **Per cell**:
  - On/off
  - Probability (0 to 100 %)
  - Velocity
  - Ratchet (1 to 4 repeats within the step)
- Global swing
- Sequencer can be bypassed so the plugin also plays from incoming MIDI

### 5.1 MIDI output (optional mode)

- The plugin declares MIDI output (JUCE `producesMidi()`), so the sequencer's generated notes (after probability, ratchets, velocity and Chaos on velocity) are sent to the host
- **Output mode** parameter: Audio only, Audio + MIDI, MIDI only
- In **MIDI only** mode the sound engine is disabled entirely (no voices rendered, no CPU cost), so BleepForge works purely as a probability sequencer driving another synth
- **MIDI thru** toggle: incoming MIDI notes are passed through to the output as well
- Selectable output MIDI channel
- Note-offs must always be sent correctly (also on transport stop, mode change and bypass), so no hanging notes
- Note: host support for MIDI output from instrument plugins varies. CLAP note output is the primary target (Bitwig handles it well); VST3 MIDI output should be implemented but tested per host

## 6. UI

- Top: grid (main focus, clearly readable probability per cell, e.g. fill level or opacity)
- Bottom: engine section (Bleep / Foley), Chaos, effects
- Simple cell editing: click to toggle, drag or modifier+drag to set probability/velocity
- Resizable window
- Own visual identity, not imitating any other plugin

## 7. Presets

- Factory presets defined in code or as bundled parameter files (no audio content)
- Aim for a starter set of around 10 to 15 presets: bleep lines, drips, metal tinks, wood knocks, crackle textures

## 8. Quality and testing

- Unit tests for sequencer logic (step timing, probability distribution, ratchets, scale mapping)
- Validate builds with pluginval
- Test in at least one host on each platform (Bitwig on Linux and Windows is the primary host)

## 9. Milestones

1. **Skeleton:** JUCE 8 project with CMake, CLAP + VST3 build on Linux and Windows, empty UI, APVTS in place
2. **Bleep engine** playable from MIDI, mono/poly
3. **Sequencer core:** grid model, host sync, scale/root/octave, probability, velocity
4. **Grid UI**
5. **Foley engine**
6. **Ratchets, Chaos, swing, step length/rate**
7. **Effects**
7b. **MIDI output mode** (Audio / Audio + MIDI / MIDI only, MIDI thru, channel)
8. **Presets, state save/restore, polish**
9. **Testing and release** (README, license, build artifacts)

## 10. Open for later

- Engine selection per row instead of per preset
- A separate MIDI-effect build of the sequencer (for hosts that prefer MIDI effects over instruments with MIDI out)
- Pattern slots / pattern switching
- Final product name

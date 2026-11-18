# BleepWerk

BleepWerk is a melodic percussion instrument for hypnotic and minimal techno,
for Linux and Windows. It makes short, organic sounds: classic bleeps and
blips, and struck, brushed, blown and squealing tones (drips, metallic tinks,
wooden knocks, steam, squeals). You play it from MIDI, or with the built-in
step sequencer, whose layers drift in and out of phase against each other. It
comes as a CLAP and a VST®3 plug-in. Free and open source.

**Download the latest version:**
[**Linux** (x86-64, .tar.gz)](https://github.com/travisdk/BleepWerk/releases/latest/download/BleepWerk-linux-x86_64.tar.gz)
·
[**Windows** (64-bit, .zip)](https://github.com/travisdk/BleepWerk/releases/latest/download/BleepWerk-windows-x64.zip)

> **Developed in my spare time.** I work on BleepWerk when I have the time
> and share it as it is. There is no support and no fixed schedule for
> updates. Pull requests with fixes are welcome, but may go unanswered. You
> are free to fork it and carry it on.

Every sound is synthesised in code. There are no samples and no wavetables,
and the presets are parameter sets only.

## Features

- **Two engines**, Bleep and Werk, one per sound (see below).
- **Mono or Poly** (up to 8 notes). Mono plays the last key held, with
  optional glide, and returns to the key still held when you let go of it.
- **Chaos:** every note is a little different. One amount plus a depth for
  pitch, decay, tone, pan and velocity, rolled per note, so a repeated note
  never sounds the same twice.
- **Drive:** Warm (soft saturation), Hard (fuzz) or Fold (wavefolder), placed
  before the effects (Pre) or after them (Post).
- **Effects**, in this order: filter (LP, BP, HP), bit crusher, tempo-synced
  delay (1/32 to 1/2, straight, triplet or dotted) and a plate reverb with a
  duck from the dry signal. Each is transparent at its default.
- **Step sequencer** with eight patterns, layers and MIDI export (see below).
- **Dice:** rolls a random sound for the selected engine, with a back arrow
  that undoes the last roll.
- Over 100 factory sound presets in one searchable list, plus your own
  presets. Pitch bend with a Bend Range of 0 to 24 semitones.
- Resizable interface with eleven colour themes, and a tooltip on every
  control.

### The two engines

**Bleep** is the classic blip: an oscillator (sine, saw, square or pulse with
PWM, or simple two-operator FM) with a fast pitch envelope, a short amplitude
envelope and an optional noise blend.

**Werk** plays something on an object. The object is always the same free bar
of wood or metal, which rings at the note you play. You choose how it is
played, with five knobs:

| Play | What it does |
|---|---|
| **Knock** | A short pulse with a little noise bite. Tone is how hard. |
| **Brush** | A decaying burst of noise. |
| **Blow** | Steady noise through a bandpass on the note, opening with a puff. |
| **Squeal** | Metal rubbed on the bar: a bright tone that slides into the note, with a hiss. Motion makes it waver, chatter and jump. |

| Knob | What it does |
|---|---|
| **Length** | The length of a burst, or how long Blow and Squeal last. |
| **Ring** | How long the bar rings. Brush and Blow need it turned up to be heard as a body. |
| **Tone** | Dark to bright. |
| **Motion** | A rattle, a flutter or a waver. |
| **Sweep** | The bar slides into the note: drips, a drum's fall. |

Heavy industrial sounds come from a hard-hit bar with Drive and reverb.

## The step sequencer

The **SEQ** button in the header folds a sequencer out under the effects.
Folded in, the sequencer is off and BleepWerk is a plain instrument. It is
locked to your DAW's transport and sample-accurate.

A pattern is up to sixteen steps. Each step is on or off, has a note counted
from the root, and can carry an accent. The sequencer holds eight patterns,
A-H, and **any number of them can play at once as layers**, each with its own
length. Layers all start together from beat 0, so a 16-step layer against a
12-step layer meets again after 48 steps. That is a polymeter, and it is the
core of the hypnotic sound.

Each pattern has its own settings, saved with it:

- **Length** and **Rate**: how many steps, and how long each lasts.
- **Swing** and **Gate**: the feel and how long the notes are held.
- **Drift:** moves the pattern on a few steps every pass, so it slowly
  slides out of phase with itself.
- **Hits:** spreads that many hits evenly over the steps (3 of 8 is the
  tresillo).
- **Vary** with a shape (Walk, Random or Skip): lets the length change from
  pass to pass.
- **Transpose:** -24 to +24 semitones from the shared Root, so one layer can
  sit in another octave or on another interval.

**Root** is shared by all layers. While the sequencer runs and your DAW plays,
the keys you hold don't sound: they set the root, so holding F2 plays the
patterns from F2. Release them and the **Root** knob applies again. While the
DAW is stopped, or with the sequencer off, the keys play BleepWerk like any
instrument.

A new instance starts as the factory preset Log Drum, with the sequencer off
and the set *Five Over Seven* waiting in A and B.

### Pattern sets, files and buttons

- Factory patterns and **polymeter sets**. A set fills as many slots as it has
  layers, from the shown slot on, and names them "Set name · A", "Set name · B"
  and so on. If a set doesn't fit from the shown slot, you get a message saying
  how many slots are missing. Afterwards the slots are ordinary patterns.
- Your own pattern files (`.bfpattern`), a dice for a random pattern, and
  copy to another slot.
- **CLEAR** asks whether to empty all eight patterns (and switch every layer
  off) or only the shown one. The pattern's settings go back to the plain
  ones. Root and the sound stay.
- A tick under a letter means that pattern plays. "ACC" above a step marks an
  accent.
- The **Pattern** parameter can be automated: it makes one pattern the only
  one that plays.

### Exporting MIDI

The **MIDI** button saves the shown pattern as a one-pass MIDI clip, or
everything the playing layers play, exactly as they play it (length, rate,
swing, gate, accents, Drift and Vary included), over 4 to 64 bars. On
Windows you can drag the button straight into your DAW.

Play the clip with BleepWerk's sequencer off, or with another instrument:
while the sequencer runs, incoming notes set the root instead of playing.

## MIDI

BleepWerk takes notes on any channel, with velocity. It handles note-off (and
note-on at velocity 0), the sustain pedal (CC 64), all-notes-off and
all-sound-off (CC 123 and 120), reset-all-controllers (CC 121) and pitch bend.
Notes start on the exact sample your DAW puts them on. Nothing hangs: changing
mode, all-notes-off and bypass end everything that plays. There is no MIDI
output.

## Download and install

Download the ready-built plug-ins for your system (always the latest
release):

- **Linux** (x86-64):
  [BleepWerk-linux-x86_64.tar.gz](https://github.com/travisdk/BleepWerk/releases/latest/download/BleepWerk-linux-x86_64.tar.gz)
- **Windows** (64-bit):
  [BleepWerk-windows-x64.zip](https://github.com/travisdk/BleepWerk/releases/latest/download/BleepWerk-windows-x64.zip)

Each archive contains an `INSTALL` text with the steps. In short, copy
`BleepWerk.clap` and/or the `BleepWerk.vst3` folder to your plug-in folder:

| | CLAP | VST3 |
|---|---|---|
| Linux | `~/.clap/` | `~/.vst3/` |
| Windows | `%LOCALAPPDATA%\Programs\Common\CLAP\` | `%LOCALAPPDATA%\Programs\Common\VST3\` |

Then rescan plug-ins in your DAW. BleepWerk appears as an instrument.

**Tested with:** Bitwig Studio on Linux and Windows. It also passes
[pluginval](https://github.com/Tracktion/pluginval) at strictness level 10 and
the [clap-validator](https://github.com/free-audio/clap-validator).

**No macOS version.** I have no Mac to build and test on. The code is plain
JUCE and may well build on macOS, but you are on your own there.

## Building from source

You need CMake 3.22 or newer, a C++17 compiler (GCC or Clang on Linux,
Visual Studio 2022 on Windows) and two SDKs:

- [JUCE](https://github.com/juce-framework/JUCE) **8.0.15**
- [clap-juce-extensions](https://github.com/free-audio/clap-juce-extensions),
  commit `7adee3a`, with its submodules

```sh
mkdir -p ~/SDKs
git clone --depth 1 --branch 8.0.15 https://github.com/juce-framework/JUCE.git ~/SDKs/JUCE-8.0.15
git clone https://github.com/free-audio/clap-juce-extensions.git ~/SDKs/clap-juce-extensions-main
git -C ~/SDKs/clap-juce-extensions-main checkout 7adee3a1bd4684d4caa5601100e364abccff4b4d
git -C ~/SDKs/clap-juce-extensions-main submodule update --init --recursive
```

On Linux, also install the JUCE build dependencies (Debian/Ubuntu names):

```sh
sudo apt install ninja-build libasound2-dev libfreetype-dev libfontconfig1-dev \
  libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev \
  libxrandr-dev libxrender-dev
```

Then build (on Windows, leave out `-G Ninja` and add `--config Release` to the
build and test commands):

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build                        # unit tests
```

The SDKs are looked for in `~/SDKs` (`%USERPROFILE%\SDKs` on Windows). Use
`-DJUCE_DIR=…` and `-DCLAP_JUCE_EXTENSIONS_DIR=…` to point elsewhere. By
default each build copies the plug-ins into your user plug-in folders; turn
that off with `-DBLEEPWERK_INSTALL_AFTER_BUILD=OFF`. The release archives
(`package-linux`, `package-windows`) need a build with `-DBLEEPWERK_LTO=ON`.

[`docs/BleepWerk_PROJECT_BRIEF.md`](docs/BleepWerk_PROJECT_BRIEF.md) describes
what the plug-in does and why.

## Forks and contributions

You are welcome to fork BleepWerk and change it in your own repository. Only
I can change this repository: pull requests are welcome, but I decide whether
to merge them, and they may go unanswered.

If you share a modified version, the licence requires that you keep the
copyright and licence notices, mark your changes as yours, and publish your
source code under the same licence.

## Licence

Copyright © 2026 Henrik Juhl

BleepWerk is free software: you can redistribute it and/or modify it under
the terms of the [GNU Affero General Public License, version 3](LICENSE)
(SPDX: `AGPL-3.0-only`). It comes with **no warranty**.

BleepWerk is built with [JUCE](https://juce.com), used under the AGPLv3. The
other bundled components (CLAP, the VST3 SDK, fonts and others) are under
permissive licences, listed with their full texts in
[THIRD-PARTY-NOTICES.txt](THIRD-PARTY-NOTICES.txt).

VST is a registered trademark of Steinberg Media Technologies GmbH.

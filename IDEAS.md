# Ideas

Ideas for later, not yet planned into a milestone.

## Machine type "Screech" (proposed 2026-10-19)

A fifth Machine type: metal rubbing on metal, the screech of a heavy machine. Today Grind or
Hiss with a high Tone and lots of Metal comes close to a squeal, but not the gliding, chattering
scream of friction.

What makes a screech, from the sketches in `private/sound-tests/make_screech.cpp`:

- a bright tone (about 250 Hz to 3 kHz, odd and even partials) that glides in pitch and wavers
- chatter: the level stutters as the metal sticks and slips
- now and then a jump to another vibration mode
- friction hiss around the tone, and the machine's own resonances swelling as the pitch
  passes them (here: the Machine resonator, which already follows the note and Metal)

It would use the five Machine controls as they are (Length, Tone, Motion, Focus, Metal), so it
is one more entry in the Type menu, with no new page or control. A few factory presets would
come with it.

Composite sounds (a heavy rumble that then hits something screeching) stay a job for several
BleepWerk instances on their own tracks; building layers into one sound would need an extra
layer in the UI.

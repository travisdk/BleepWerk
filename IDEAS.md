# Ideas

Ideas for later, not yet planned into a milestone.

## Richer ratchets

Ratchet has its own edit mode but only plays 1-4 even hits at the same velocity, so it gives
little for the space it takes. Keep the mode and make it worth it:

1. **More hits: 1-8 instead of 1-4.** At 1/16 steps, 8 hits make fast rolls. Still set by
   dragging up/down. The cell's packed word has free bits for it.
2. **A Ratchet feel choice in the sequencer bar**, for all ratcheted cells:
   - **Even**: as now
   - **Fade**: each hit softer than the one before (echo, bouncing ball)
   - **Swell**: each hit louder (a build into the next step)
   - **Speed up**: the hits get closer together through the step (a tightening roll)
3. **Maybe later: feel per cell** instead of one global choice. More expressive, but it needs
   a way to set it per cell (a second drag direction or click-to-cycle), which runs against
   the no-hidden-gestures preference, so start with the global choice.

Rejected: dropping the Ratchet mode and folding it into Velocity mode (e.g. a sideways drag
for the count). It frees a header button but hides ratchets behind a gesture.

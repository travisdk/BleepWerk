// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

// What the picture of the sound draws: the Bleep's curves match the envelope the engine uses,
// and the resonator tones come out in the right order, strongest first and longest ringing low.
#include "SoundShape.h"
#include "TestUtils.h"

using namespace bleepforge;

static void testBleep()
{
    BleepParams p;
    p.attackMs = 10.0f;
    p.decayMs = 200.0f;
    p.pitchEnvSemis = 24.0f;
    p.pitchEnvDecayMs = 50.0f;
    CHECK (bleepAmplitudeAt (p, 0.0f) == 0.0f);
    CHECK (std::abs (bleepAmplitudeAt (p, 5.0f) - 0.5f) < 1.0e-6f); // halfway up the attack
    CHECK (std::abs (bleepAmplitudeAt (p, 10.0f) - 1.0f) < 1.0e-6f);
    CHECK (std::abs (bleepAmplitudeAt (p, 210.0f) - 1.0e-4f) < 2.0e-5f); // -80 dB at the end of the decay
    CHECK (std::abs (bleepPitchAt (p, 0.0f) - 24.0f) < 1.0e-5f);
    CHECK (std::abs (bleepPitchAt (p, 50.0f) - 24.0f * 0.01f) < 5.0e-3f); // -40 dB after P Decay
    CHECK (bleepShapeEndMs (p) == 210.0f);
    p.decayMs = 5.0f;
    p.attackMs = 0.5f;
    CHECK (bleepShapeEndMs (p) >= 20.0f);

    // the same envelope the engine runs: render it and compare
    Envelope env;
    constexpr float fs = 48000.0f;
    p.attackMs = 10.0f;
    p.decayMs = 200.0f;
    env.set (fs, p.attackMs, p.decayMs, 0.0f, 30.0f);
    env.noteOn();
    float worst = 0.0f;
    for (int i = 0; i < (int) (0.25f * fs); ++i)
    {
        const float v = env.next();
        worst = std::max (worst, std::abs (v - bleepAmplitudeAt (p, 1000.0f * (float) (i + 1) / fs)));
    }
    CHECK (worst < 0.02f);
}

static void testModes()
{
    StrikeParams p;
    p.body = StrikeBody::Wood;
    const auto wood = strikeShape (p);
    CHECK (wood.size() == (size_t) kNumStrikeModes);
    CHECK (wood[0].ratio == 1.0f && wood[1].ratio > 2.0f);
    p.body = StrikeBody::Membrane;
    const auto drum = strikeShape (p);
    CHECK (drum[5].ratio < 3.2f); // a skin's modes crowd together
    CHECK (drum[0].ringSeconds > wood[0].ringSeconds); // and ring longer
    for (const auto& m : wood)
        CHECK (m.ringSeconds > 0.0f && m.strength > 0.0f);
    p.body = StrikeBody::Wood;
    p.ring = 0.0f;
    const auto damped = strikeShape (p);
    CHECK (damped[0].ringSeconds < wood[0].ringSeconds);

    // the ring-out curve starts at 1 and falls; a damped sound ends sooner
    CHECK (std::abs (ringLevelAt (wood, 0.0f) - 1.0f) < 1.0e-5f);
    CHECK (ringLevelAt (wood, 0.1f) < 1.0f && ringLevelAt (wood, 1.0f) < ringLevelAt (wood, 0.1f));
    CHECK (ringEndSeconds (damped) < ringEndSeconds (wood));

    p.body = StrikeBody::Metal;
    p.ring = 1.0f;
    const auto ringing = strikeShape (p);
    CHECK (ringing.size() == 8);
    CHECK (ringing[0].strength == 1.0f); // the strongest is 1
    for (size_t k = 1; k < ringing.size(); ++k)
        CHECK (ringing[k].strength < ringing[k - 1].strength); // the higher the weaker
    CHECK (ringing[0].ringSeconds > ringing[7].ringSeconds); // and the shorter

    // Friction: a bright tone with its partials
    p.exciter = StrikeExciter::Friction;
    p.tone = 1.0f;
    const auto screech = strikeShape (p);
    CHECK (screech.size() == 7 && screech[0].ratio == 1.0f && screech[6].ratio == 7.0f);
    p.tone = 0.0f;
    CHECK (strikeShape (p)[6].strength < screech[6].strength); // a dark tone loses the top

    // Steam and Friction: fade in, plateau to half of Length, then down to silence at Length
    p.exciter = StrikeExciter::Steam;
    p.lengthMs = 1000.0f;
    CHECK (steadyLevelAt (p, 0.0f) == 0.0f);
    CHECK (steadyLevelAt (p, 300.0f) == 1.0f);
    CHECK (steadyLevelAt (p, 750.0f) < 1.0f && steadyLevelAt (p, 750.0f) > steadyLevelAt (p, 1000.0f));
    CHECK (std::abs (steadyLevelAt (p, 1000.0f) - 1.0e-4f) < 2.0e-5f); // -80 dB at Length
}

int main()
{
    testBleep();
    testModes();
    return finish ("soundshape");
}

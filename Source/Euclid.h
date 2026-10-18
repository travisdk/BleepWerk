// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <algorithm>
#include <array>

// A Euclidean rhythm: `pulses` onsets spread as evenly as possible over `steps` steps, made with
// Bjorklund's algorithm, so E(3,8) is x..x..x. and E(5,8) is x.xx.xx. The pattern starts on an
// onset and `rotation` moves it later by that many steps, wrapping round. Any values are
// accepted: steps is limited to 1..kMaxEuclidSteps, pulses to 0..steps. (Adapted from SeqVerse,
// same author and licence.) JUCE-free.
namespace bleepforge
{
inline constexpr int kMaxEuclidSteps = 32;

struct EuclidPattern
{
    int steps, pulses;
    std::array<bool, kMaxEuclidSteps> onset {};

    EuclidPattern (int stepsIn, int pulsesIn, int rotation = 0)
        : steps (std::clamp (stepsIn, 1, kMaxEuclidSteps)), pulses (std::clamp (pulsesIn, 0, steps))
    {
        std::array<bool, kMaxEuclidSteps> base {};
        bjorklund (base);
        const int shift = ((rotation % steps) + steps) % steps;
        for (int i = 0; i < steps; ++i)
            onset[(size_t) ((i + shift) % steps)] = base[(size_t) i];
    }

    bool isOnset (int step) const { return onset[(size_t) (((step % steps) + steps) % steps)]; }

private:
    struct Builder
    {
        const int* counts;
        const int* remainders;
        int* pattern;
        int length;

        void build (int level)
        {
            if (level == -1)
                pattern[length++] = 0;
            else if (level == -2)
                pattern[length++] = 1;
            else
            {
                for (int i = 0; i < counts[level]; ++i)
                    build (level - 1);
                if (remainders[level] != 0)
                    build (level - 2);
            }
        }
    };

    void bjorklund (std::array<bool, kMaxEuclidSteps>& out) const
    {
        if (pulses == 0)
            return;
        if (pulses == steps)
        {
            std::fill (out.begin(), out.begin() + steps, true);
            return;
        }

        int counts[kMaxEuclidSteps + 2] = {};
        int remainders[kMaxEuclidSteps + 2] = {};
        int divisor = steps - pulses;
        remainders[0] = pulses;

        int level = 0;
        do
        {
            counts[level] = divisor / remainders[level];
            remainders[level + 1] = divisor % remainders[level];
            divisor = remainders[level];
            ++level;
        } while (remainders[level] > 1);
        counts[level] = divisor;

        int pattern[kMaxEuclidSteps] = {};
        Builder builder { counts, remainders, pattern, 0 };
        builder.build (level);

        // start on an onset
        int first = 0;
        while (first < steps - 1 && pattern[first] == 0)
            ++first;
        for (int i = 0; i < steps; ++i)
            out[(size_t) i] = pattern[(i + first) % steps] != 0;
    }
};
} // namespace bleepforge

// Copyright (C) 2026 Henrik Juhl. Part of BleepWerk, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

// Minimal test helpers: CHECK records a failure and keeps going, finish()
// prints a summary and returns the process exit code for CTest.
inline int& testFailures()
{
    static int failures = 0;
    return failures;
}

#define CHECK(condition)                                                                   \
    do                                                                                     \
    {                                                                                      \
        if (! (condition))                                                                 \
        {                                                                                  \
            ++testFailures();                                                              \
            std::printf ("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);              \
        }                                                                                  \
    } while (false)

// How bright a sound is, independent of its level: the RMS of its slope over the RMS of the
// signal (about 2 pi f / fs for a sine), so a darker sound scores lower.
inline double brightness (const std::vector<float>& v)
{
    double slope = 0.0, level = 0.0;
    for (size_t i = 1; i < v.size(); ++i)
    {
        const double d = (double) v[i] - v[i - 1];
        slope += d * d;
        level += (double) v[i] * v[i];
    }
    return std::sqrt (slope / std::max (1.0e-30, level));
}

inline int finish (const char* name)
{
    std::printf ("%s: %s (%d failed checks)\n", name, testFailures() == 0 ? "passed" : "FAILED", testFailures());
    return testFailures() == 0 ? 0 : 1;
}

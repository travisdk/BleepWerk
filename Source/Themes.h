// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>

// The colour themes: one hue in two shades on the dark panels. The bright shade is the main
// one (knobs, titles, probability, the root rows); the deeper one marks the other values
// (velocity, ratchets). Both stay well clear of the dark background, which the tests check by
// contrast ratio. JUCE-free so the tests can read it; colours are 0xRRGGBB.
namespace bleepforge
{
inline constexpr uint32_t kBackgroundRgb = 0x121315, kPanelRgb = 0x1c1f24, kPanelAltRgb = 0x20242c;

struct ThemeChoice
{
    const char* name;
    uint32_t primary, secondary;
};

inline constexpr ThemeChoice kThemes[] = {
    { "Hazard", 0xf5c518, 0xd9780f }, // warning yellow and orange: the control-panel look, the default
    { "Mint", 0x4fe0b0, 0x1ba477 },   { "Lime", 0x97d912, 0x6e9e0d },   { "Amber", 0xffb92e, 0xc48300 },
    { "Orange", 0xff9d57, 0xeb6200 }, { "Coral", 0xfc9a8f, 0xfa4e3b },  { "Pink", 0xfc94c8, 0xfa3d9c },
    { "Violet", 0xcaa5fd, 0xa96ffb }, { "Blue", 0x8db5fc, 0x4789fa },   { "Cyan", 0x40d7f5, 0x0a9dbb },
    { "Silver", 0xc0c6ce, 0x8690a0 },
};
inline constexpr int kNumThemes = (int) std::size (kThemes);

// WCAG relative luminance and contrast ratio (1 to 21).
inline double relativeLuminance (uint32_t rgb)
{
    auto channel = [rgb] (int shift)
    {
        const double c = (double) ((rgb >> shift) & 0xffu) / 255.0;
        return c <= 0.03928 ? c / 12.92 : std::pow ((c + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * channel (16) + 0.7152 * channel (8) + 0.0722 * channel (0);
}

inline double contrastRatio (uint32_t a, uint32_t b)
{
    const double la = relativeLuminance (a), lb = relativeLuminance (b);
    return (std::max (la, lb) + 0.05) / (std::min (la, lb) + 0.05);
}
} // namespace bleepforge

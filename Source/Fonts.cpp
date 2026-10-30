// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#include "Fonts.h"
#include "BinaryData.h"

namespace bleepforge::fonts
{
namespace
{
juce::Typeface::Ptr load (const void* data, int size)
{
    return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
}
} // namespace

juce::Typeface::Ptr labelTypeface()
{
    static const auto face = load (BinaryData::BarlowSemiCondensedSemiBold_ttf, BinaryData::BarlowSemiCondensedSemiBold_ttfSize);
    return face;
}

juce::Font label (float height)
{
    return juce::Font (juce::FontOptions (labelTypeface()).withHeight (height));
}

juce::Font title (float height)
{
    static const auto face = load (BinaryData::BarlowSemiCondensedBold_ttf, BinaryData::BarlowSemiCondensedBold_ttfSize);
    return juce::Font (juce::FontOptions (face).withHeight (height)).withExtraKerningFactor (0.06f);
}

juce::Font value (float height)
{
    static const auto face = load (BinaryData::IBMPlexMonoMedium_ttf, BinaryData::IBMPlexMonoMedium_ttfSize);
    return juce::Font (juce::FontOptions (face).withHeight (height));
}
} // namespace bleepforge::fonts

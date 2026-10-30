// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_graphics/juce_graphics.h>

// The typefaces, embedded in the plug-in (both under the SIL Open Font License, see
// THIRD-PARTY-NOTICES.txt): Barlow Semi Condensed, a signage face, for names, buttons, menus and
// titles, and IBM Plex Mono for the values, whose fixed width keeps the numbers from jumping
// about as a knob is turned.
namespace bleepforge::fonts
{
juce::Typeface::Ptr labelTypeface(); // the default for the whole interface

juce::Font label (float height);  // names, buttons, menus
juce::Font title (float height);  // group titles and the wordmark: bold, a little spaced
juce::Font value (float height);  // numbers
} // namespace bleepforge::fonts

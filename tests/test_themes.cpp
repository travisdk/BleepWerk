// Copyright (C) 2026 Henrik Juhl. Part of BleepWerk, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

// The colour themes: both shades of every theme read clearly on the darkest and lightest of
// the dark panels (at least 4.5:1, WCAG AA for text), and the two shades of a theme are far
// enough apart to tell probability from velocity.
#include "Themes.h"
#include "TestUtils.h"

#include <cstdio>
#include <cstring>
#include <set>
#include <string>

using namespace bleepwerk;

int main()
{
    std::set<std::string> names;
    for (const auto& t : kThemes)
    {
        CHECK (names.insert (t.name).second);
        for (uint32_t shade : { t.primary, t.secondary })
            for (uint32_t panel : { kBackgroundRgb, kPanelRgb, kPanelAltRgb })
                if (contrastRatio (shade, panel) < 4.5)
                {
                    std::printf ("FAIL %s: #%06x on #%06x is only %.2f:1\n", t.name, shade, panel, contrastRatio (shade, panel));
                    ++testFailures();
                }
        const double apart = relativeLuminance (t.primary) / relativeLuminance (t.secondary);
        if (apart < 1.5)
        {
            std::printf ("FAIL %s: the shades are too close (%.2f)\n", t.name, apart);
            ++testFailures();
        }
    }
    CHECK (std::strcmp (kThemes[0].name, "Hazard") == 0); // the default
    return finish ("themes");
}

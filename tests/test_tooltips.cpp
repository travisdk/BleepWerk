// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

// Every control tied to a parameter has a hover text: each sound parameter has one, and so do the
// few global ones the panels show, so a new control can't ship without a tooltip.
#include "Patch.h"
#include "Tooltips.h"
#include "TestUtils.h"

#include <cstring>

using namespace bleepforge;

static void everySoundParameterHasATooltip()
{
    for (const auto& p : kSoundParams)
    {
        const char* text = tooltips::forParameter (p.id);
        if (text == nullptr || std::strlen (text) < 10)
            std::printf ("missing tooltip: %s\n", p.id);
        CHECK (text != nullptr && std::strlen (text) >= 10);
    }
}

static void panelParametersHaveTooltips()
{
    for (const char* id : { "bendRange", "seqRoot" })
        CHECK (tooltips::forParameter (id) != nullptr);
}

static void unknownParameterHasNone()
{
    CHECK (tooltips::forParameter ("noSuchParameter") == nullptr);
}

int main()
{
    everySoundParameterHasATooltip();
    panelParametersHaveTooltips();
    unknownParameterHasNone();
    return finish ("tooltips");
}

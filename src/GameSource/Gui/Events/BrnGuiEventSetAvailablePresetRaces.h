#pragma once

#include <cstddef>
#include "SharedClasses/Progression/BrnRace.h"

namespace BrnGui
{
    // ARTIST AddGuiEvent823D2E48 posts the raw 728-byte record, without a GuiEvent header.
    // RecEvent8250E5C0 reads the count at +0x2D0 and the first route at +0x30/+0x70.
    // DecFIGS BrnGuiEventTypeDefs.h:1407/1408 supplies these member names and types.
    struct GuiEventSetAvailablePresetRaces
    {
        BrnProgression::Race maPresetRaces[6];
        s32 miNumPresetRaces;
        s32 GetEventType() const { return 170; }
    };
    static_assert(sizeof(GuiEventSetAvailablePresetRaces) == 728,
                  "ARTIST AddGuiEventSetAvailablePresetRaces posts 0x2D8 bytes");
    static_assert(offsetof(GuiEventSetAvailablePresetRaces, miNumPresetRaces) == 720,
                  "ARTIST race-count word is at record+0x2D0");
}

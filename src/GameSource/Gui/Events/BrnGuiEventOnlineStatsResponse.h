#pragma once

// BrnGuiEventOnlineStatsResponse.h
// Home of BrnGui::GuiEventOnlineStatsResponse -- the online-stats screen's six totals (GUI
// event 242). Like the other leaf event homes here it is the bare payload with no GuiEvent
// header: the out-queue wrapper adds { size, type, offset } in front of it.

#include "types.hpp"

namespace BrnGui
{
    // GUI event 242: the six player totals the online-stats page prints.
    struct GuiEventOnlineStatsResponse
    {
        s32 miTotalGames;        // +0x00
        s32 miWinRate;           // +0x04
        s32 miTakedowns;         // +0x08
        s32 miRivals;            // +0x0C
        s32 miMugshots;          // +0x10
        s32 miDisconnectRate;    // +0x14

        s32 GetEventType() const { return 242; }
    };

    static_assert(sizeof(GuiEventOnlineStatsResponse) == 24,
                  "the stats response payload is six words");
}

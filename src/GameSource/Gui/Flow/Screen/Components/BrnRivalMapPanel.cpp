// ===================================================================================
// BrnGui::RivalMapPanel  -- implementation
//   class:BrnGui::RivalMapPanel
//
// StorePlayerInfo @ 0x824176C0
//   Asserts the incoming response event is non-null ("NULL != lpStatsResponseEvent",
//   BrnRivalMapPanel.h:153), copies 0x1B0 (432) bytes from it into the panel's cache at
//   +0x5F0 (memcpy), then sets the cached-valid byte at +0x7A0 to 1 (stb). Reconstructed
//   store-for-store from the X360 asm; CrashNavPanel::RecEvent feeds it the stats response.
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/Components/BrnRivalMapPanel.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cstring>   // memcpy

namespace BrnGui
{
    // @ 0x824176C0
    void RivalMapPanel::StorePlayerInfo(const void* lpStatsResponseEvent)
    {
        CGS_ASSERT(lpStatsResponseEvent != 0, "NULL != lpStatsResponseEvent");   // @0x824176E4

        std::memcpy(maStatsResponse, lpStatsResponseEvent, KI_STATS_RESPONSE_SIZE); // @0x82417710
        mbHasPlayerInfo = true;                                                      // @0x82417718 (stb 1)
    }
}

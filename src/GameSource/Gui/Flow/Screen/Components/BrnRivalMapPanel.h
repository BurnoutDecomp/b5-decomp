#pragma once

// ===================================================================================
// BrnGui::RivalMapPanel  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Screen/Components/BrnRivalMapPanel.h
//
// The online rival-map HUD panel. StorePlayerInfo (@0x824176C0) caches the most recent
// player-stats response event into the panel and marks the cached copy valid so the
// renderer can draw the rival markers from it.
//
// No prior reconstruction carries this panel's full member layout, and there is no DecFIGS
// DWARF for this TU. The recovered shape comes from the X360 access in
// StorePlayerInfo: a 432-byte (0x1B0) stats blob copied into object +0x5F0 and a single
// "valid" flag byte at object +0x7A0 (stb 1). The blob spans +0x5F0..+0x79F, so the flag
// at +0x7A0 sits immediately after it (0x5F0 + 0x1B0 == 0x7A0). The unrecovered head ahead
// of the blob is kept as a reserved byte-span so each named field lands at its binary
// offset without raw-offset casting; all access is by name.
// (assert site: BrnRivalMapPanel.h:153.)
// ===================================================================================

#include "types.hpp"

namespace BrnGui
{
    class RivalMapPanel
    {
    public:
        // Size of the cached player-stats response event blob (X360 memcpy size 0x1B0).
        static const s32 KI_STATS_RESPONSE_SIZE = 0x1B0;   // 432 bytes

        // @ 0x824176C0 - copy the incoming player-stats response event into the panel's
        // cache and mark it valid. lpStatsResponseEvent must be non-null.
        void StorePlayerInfo(const void* lpStatsResponseEvent);

    private:
        // Reserved storage for the unrecovered panel head that precedes the cached stats
        // blob (object +0x5F0). Layout-recovery padding only.
        u8   maHeadReserved[0x5F0];

        // Cached copy of the most recent player-stats response event (object +0x5F0).
        u8   maStatsResponse[KI_STATS_RESPONSE_SIZE];   // +0x5F0 .. +0x79F

        // Set true once a stats response has been cached (X360 stb 1 @ +0x7A0, immediately
        // following the cached blob).
        bool mbHasPlayerInfo;   // +0x7A0
    };
}

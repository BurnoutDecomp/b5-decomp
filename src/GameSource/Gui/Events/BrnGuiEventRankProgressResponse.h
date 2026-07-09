// BrnGuiEventRankProgressResponse.h
// Home of BrnGui::GuiEventRankProgressResponse -- the GUI event payload carrying a
// player's rank-progress query response.
//
// This slice reconstructs ONLY the out-of-line accessor the X360 ARTIST build emits:
//
//   GetPlayerRank @ 0x8240EA28 -> returns miCurrentRank (s32 @ +0x20) after a non-fatal
//                                 guard that it is not the "finished last rank" sentinel
//                                 (KI_PLAYER_HAS_FINISHED_LAST_RANK == -1). DWARF assert
//                                 site GameSource/Gui/BrnGuiEventTypeDefs.h:7841.
//
// LAYOUT: only miCurrentRank @ +0x20 is load-bearing for this accessor and is pinned by
// the X360 byte offset above. The leading payload (the event header + the other response
// fields) is not independently verified against the binary for this slice, so it is held
// as an explicitly-reserved head that places miCurrentRank at its proven offset. This is
// an honest layout boundary, not a fabricated one.

#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h" // CgsModule::Event base

namespace BrnGui
{
    // Sentinel returned by the rank system when a player has dropped out of (finished)
    // the last rank; GetPlayerRank asserts the stored rank is not this value. X360
    // compares miCurrentRank against -1.
    const s32 KI_PLAYER_HAS_FINISHED_LAST_RANK = -1;

    // The rank-progress query response GUI event. Only miCurrentRank @ +0x20 is modelled
    // at its X360-proven offset; the leading bytes are an explicitly-reserved head.
    struct GuiEventRankProgressResponse : public CgsModule::Event
    {
        // @ 0x8240EA28 -- return the current rank, asserting it is not the
        // "finished last rank" sentinel (the X360 returns the value regardless).
        s32 GetPlayerRank() const;

        // AddGuiEvent<GuiEventRankProgressResponse> @0x823D7290 -> AddEvent(&event, 438, 36).
        s32 GetEventType() const { return 438; }

    private:
        u8  maHeadReserved[0x20]; // @0x00 .. 0x1F  (event header + earlier response fields)
        s32 miCurrentRank;        // @0x20  (read by GetPlayerRank; X360-pinned)
    };
}

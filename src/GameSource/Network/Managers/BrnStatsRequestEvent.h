#pragma once

#include "types.hpp"

// BrnNetwork::StatsRequestEvent - one queued "fetch this player's stat" request:
// a stat-name key plus the requesting player id. DWARF home
// BrnStatsRequestEvent.h:43. X360 layout: macName[16] @+0x00,
// mPlayerID @+0x10 (Construct @0x825487D8 strncpy's 16 bytes and stores the id
// word at +0x10; the PS3 DWARF sizes the name buffer [20] -- the X360 stores
// win, and the Construct assert bounds the source at < 16). NetworkPlayerID is
// the repo-established s32 typedef (RoadRulesRecvData::NetworkPlayerID).
namespace BrnNetwork
{
    struct StatsRequestEvent
    {
        // DWARF h:62 -- the canonical user-stat key (declaration-only; the
        // defining TU owns the literal).
        static const char* KPC_USER_STAT_NAME;

        // @0x825487D8 (this TU, DWARF h:50) -- latch the stat name + player id.
        void Construct(const char* lpcName, s32 liPlayerID);

        // DWARF h:54/h:58 -- trivial accessors (header-inline on the X360).
        const char* GetName() const { return macName; }
        s32 GetPlayerID() const     { return mPlayerID; }

    private:
        char macName[16];   // +0x00 (X360; PS3 DWARF spells [20] -- binary wins)
        s32  mPlayerID;     // +0x10
    };
}

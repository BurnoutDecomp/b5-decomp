// ===================================================================================
// BrnNetwork::NetworkDirtyTrickManager::GetDirtyTrickDataEntry
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkDirtyTrickManager.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   BrnNetwork::NetworkDirtyTrickManager::Get @ 0x825486A8
//     (the DWARF name is GetDirtyTrickDataEntry, h:126; called by AddPlayer/RemovePlayer)
//
// Linear-search the fixed 7-slot table for the entry whose mPlayerID matches the search
// key. The X360 body walks the records at a 0x6C (108) byte stride, comparing each
// record's first word (mPlayerID) against lNetworkPlayerID; on a match it forms the
// entry pointer (index * 0x6C + base) and -- when non-null -- returns it. On no match
// (or a null pointer) it fires the "lpEntry" assert and returns the (null) result.
//
// Store/branch-for-branch match to the asm:
//   lpEntry = 0; for (i=0; i<7; ++i) { if (maDirtyTrickData[i].mPlayerID == key) {
//       lpEntry = &maDirtyTrickData[i]; break; } }
//   if (lpEntry == NULL) ASSERT("lpEntry"); return lpEntry;
// The asm's "if (lpEntry) return" / fall-through-to-assert is exactly the lpEntry==NULL
// guarded assert; the matched-entry address is never null in practice, but the guard is
// reproduced faithfully (the assert fires only on the no-match fall-through).

#include "GameSource/Network/Managers/BrnNetworkDirtyTrickManager.h"

namespace BrnNetwork
{
    NetworkDirtyTrickManager::DirtyTrickData*
    NetworkDirtyTrickManager::GetDirtyTrickDataEntry(NetworkPlayerID lNetworkPlayerID)
    {
        DirtyTrickData* lpEntry = nullptr;

        for (s32 liIndex = 0; liIndex < KI_MAX_DIRTY_TRICK_PLAYERS; ++liIndex)
        {
            if (maDirtyTrickData[liIndex].mPlayerID == lNetworkPlayerID)
            {
                lpEntry = &maDirtyTrickData[liIndex];
                break;
            }
        }

        CGS_ASSERT(lpEntry != nullptr, "lpEntry");
        return lpEntry;
    }
} // namespace BrnNetwork

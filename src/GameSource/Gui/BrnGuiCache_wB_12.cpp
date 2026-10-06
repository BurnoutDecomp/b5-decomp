#include "GameSource/Gui/BrnGuiCache.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cstdlib>   // qsort (replay-players-active sort)

// Reconstructed from BURNOUT_X360_ARTIST.XEX. GuiCache replay-players-active sort +
// the field-init constructor. Faithful store-for-store to the X360 asm; named members
// per the frozen BrnGuiCache.h layout.

// LobbyNameCmp: the dirtysdk lobby name comparator (extern "C" vendor leaf, declared in
// the DirtySock server-interface TUs). The GuiCache TU reaches it as a global; links from
// the vendor TU. Used both as the qsort tie-break and by IncrementReplayPlayerActive.
extern "C" s32 LobbyNameCmp(const char* pNameA, const char* pNameB);

namespace BrnGui
{
    // @ 0x824EF028 -- qsort comparator over the 16 stride-32 replay-players-active entries.
    // Reads each entry's count word @+0x18 (un-homed within the raw entry storage; see
    // BrnGuiCache.h "count@+0x18") as unsigned and sorts DESCENDING by it; ties (equal
    // counts) fall back to LobbyNameCmp on the entry's lead CgsNetwork::PlayerName (entry
    // base). Returns 1 (a before b) when a's count is smaller, -1 when larger.
    s32 GuiCache::_SortReplayPlayersActiveByCount(const void* lpA, const void* lpB)
    {
        const u32 luCountA = *reinterpret_cast<const u32*>(static_cast<const u8*>(lpA) + 0x18);
        const u32 luCountB = *reinterpret_cast<const u32*>(static_cast<const u8*>(lpB) + 0x18);
        if (luCountA < luCountB)
        {
            return 1;
        }
        if (luCountA <= luCountB)
        {
            return LobbyNameCmp(static_cast<const char*>(lpA), static_cast<const char*>(lpB));
        }
        return -1;
    }

    // @ 0x824F8C58 -- sort the replay-players-active table once. Asserts it has not already
    // been sorted, latches mbReplayHasBeenSorted (@0x143E8), then qsorts the 16 stride-32
    // entries of maReplayPlayersActive (@0x141E0) by _SortReplayPlayersActiveByCount.
    void GuiCache::SortReplayPlayersActive()
    {
        CGS_ASSERT(!mbReplayHasBeenSorted, "mbReplayHasBeenSorted == false");
        mbReplayHasBeenSorted = true;
        qsort(maReplayPlayersActive, 16, 32, _SortReplayPlayersActiveByCount);
    }

    // GuiCache::GuiCache @0x827E05B8 is homed in mounted BrnGuiCache.cpp.

}

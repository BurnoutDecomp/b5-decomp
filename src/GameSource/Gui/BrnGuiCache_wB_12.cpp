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

    // @ 0x827E05B8 -- field-init constructor (embedded sub-objects are constructed by their
    // own ctors elsewhere; this writes only the scalar sentinels/counters the X360 ctor sets).
    GuiCache::GuiCache()
    {
        // CgsArray "used before Construct" sentinels (-1) + their companion zero fields.
        miCtorSentinel_0ED8 = -1;
        miCtorField_0EDC = 0;
        miCtorField_1EEC = 0;
        miEventStartsCount = -1;
        mEventsCtorSentinel = -1;
        miCtorSentinel_9ED8 = -1;
        miCtorSentinel_9F00 = -1;
        miScoringTrafficCount = -1;
        miCtorSentinel_A7E0 = -1;

        // per-racer numeric pairs @0xAA30 (stride 56, 8 lanes): int@+0 = 0, float@+4 = 0.
        for (s32 li = 0; li < 8; ++li)
        {
            maPerRacerData_AA30[li].miField_00 = 0;
            maPerRacerData_AA30[li].mfField_04 = 0.0f;
        }

        // online-player records @0xAC80 (stride 312, 8 lanes): int@+0x60 = 0, float@+0x64 = 0
        // (un-homed fields within the InGamePlayerStatusData byte storage; ctor-attested).
        for (s32 li = 0; li < 8; ++li)
        {
            *reinterpret_cast<s32*>(&maPlayerInfo[li][0x60]) = 0;
            *reinterpret_cast<f32*>(&maPlayerInfo[li][0x64]) = 0.0f;
        }

        miProfileEventsCount = -1;
    }
}

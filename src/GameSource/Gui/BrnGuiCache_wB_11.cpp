#include "GameSource/Gui/BrnGuiCache.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/GameState/BrnCgsPlayerName.h"   // CgsNetwork::PlayerName::Construct (append path)

// Reconstructed from BURNOUT_X360_ARTIST.XEX. GuiCache replay-player-active table
// (maReplayPlayersActive @0x141E0, 16 entries of stride 32; per entry: lead
// CgsNetwork::PlayerName name @+0x00, a 64-bit value slot @+0x10 and the u32 hit
// count @+0x18). Accessed BY NAME against the ReplayPlayerActive record in BrnGuiCache.h.
// Guarded by the game's debug assert (CGS_ASSERT is a no-op in this build, matching the
// X360 release assert machinery).

// External DirtySDK lobby C-API name comparator (vendor SDK; not yet homed in vendor/).
// A not-yet-homed callee is satisfied by its declaration under cl /c; no body needed.
extern "C" s32 LobbyNameCmp(const char* pNameA, const char* pNameB);

namespace BrnGui
{
    // @ 0x824EEE28 -- reset the replay-player-active table. Clears mbReplayHasBeenSorted
    // (@0x143E8), then per entry (stride 32) zeroes the name's leading byte, the 64-bit
    // value slot @+0x10 (std) and the u32 count @+0x18 (stw); finally clears the per-ARCI
    // replay-rendered flags (@0x143E0).
    void GuiCache::ClearReplayPlayerActive()
    {
        mbReplayHasBeenSorted = false;

        for (s32 liEntry = 0; liEntry < 16; ++liEntry)
        {
            ReplayPlayerActive& lEntry = maReplayPlayersActive[liEntry];
            lEntry.mName.macName[0] = 0;   // stb  name[0]
            lEntry.mValue = 0;             // std  value slot
            lEntry.muActiveCount = 0;      // stw  count
        }

        for (s32 liArci = 0; liArci < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liArci)
        {
            maReplayARCRendered[liArci] = false;
        }
    }

    // @ 0x824EEEC0 -- record one hit for lpcPlayerName in the replay-player-active table.
    // Only while the list is still unsorted (mbReplayHasBeenSorted != 1) and the name is
    // non-empty: scan the 16 entries; on a name match bump that entry's count; on the first
    // empty slot (count == 0) construct the name, seed the +0x10 value slot with liValue
    // (std) and set the count to 1. Runs off the end of the 16 slots with no change.
    void GuiCache::IncrementReplayPlayerActive(const char* lpcPlayerName, s32 liValue)
    {
        if (mbReplayHasBeenSorted != true && *lpcPlayerName != '\0')
        {
            u32 luEntry = 0;
            while (true)
            {
                ReplayPlayerActive& lEntry = maReplayPlayersActive[luEntry];
                if (LobbyNameCmp(lEntry.mName.GetPlayerName(), lpcPlayerName) == 0)
                {
                    ++lEntry.muActiveCount;
                    break;
                }
                if (lEntry.muActiveCount == 0)
                {
                    lEntry.mName.Construct(lpcPlayerName);
                    lEntry.mValue = liValue;
                    ++lEntry.muActiveCount;
                    break;
                }
                if (++luEntry >= 16)
                {
                    break;
                }
            }
        }
    }

    // @ 0x824EEFA0 -- index the sorted replay-player-active table. Asserts the table has
    // been sorted (mbReplayHasBeenSorted == true @0x143E8) and luIndex < 16, then returns
    // &maReplayPlayersActive[luIndex].mName (32*(luIndex+2575)+this == base+0x141E0+32*luIndex).
    const CgsNetwork::PlayerName* GuiCache::GetSortedReplayPlayerActive(u32 luIndex) const
    {
        CGS_ASSERT(mbReplayHasBeenSorted == true, "mbReplayHasBeenSorted == true");
        CGS_ASSERT(luIndex < 16, "luIndex < 16");
        return &maReplayPlayersActive[luIndex].mName;
    }
}

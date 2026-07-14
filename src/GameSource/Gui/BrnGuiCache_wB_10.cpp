#include "GameSource/Gui/BrnGuiCache.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/GameState/Progression/BrnProfile.h"       // Profile + CarData (DetermineCarUnlockPending)
#include "GameSource/Replays/BrnReplayStatusInterface.h"        // StatusInterface::GetReel (ReplayConvert...)

#include <cstdint>

// Reconstructed from BURNOUT_X360_ARTIST.XEX. A trio of GuiCache leaves that walk
// the profile / replay tables: the car-unlock-pending determination, the GUI-slot ->
// replay-reel-index resolver, and the per-ARCI "replay actor rendered" flag read. Each
// mirrors the X360 ARTIST asm store-for-store; the debug asserts are the game's release
// CGS_ASSERT (a no-op in this build, matching the X360 assert machinery).

namespace BrnGui
{
    // @ 0x824EC678 -- scan the player's profile car list for any unlocked car whose
    // unlock sequence has not yet been shown. Sets mbCarUnlockDetermined on entry, then
    // mbCarUnlockPending == true iff such a car exists (empty list -> pending == false).
    void GuiCache::DetermineCarUnlockPending(BrnProgression::Profile* lpProfile)
    {
        mbCarUnlockDetermined = true;

        s32 liCarCount = lpProfile->GetCarCount();          // Profile miCarCount @+0x26C
        if (liCarCount <= 0)
        {
            mbCarUnlockPending = false;
            return;
        }

        s32 liCarIndex = 0;
        const BrnProgression::CarData* lpProfileCar = lpProfile->GetCarData(0);   // &maCars[0] @+0x280 (stride 0x18)
        while (true)
        {
            CGS_ASSERT(liCarIndex >= 0 && liCarIndex < liCarCount,
                       "liCarIndex >= 0 && liCarIndex < miCarCount");
            CGS_ASSERT(lpProfileCar != nullptr, "lpProfileCar");

            // CarData::mbUnlockSequenceAlreadyShown @+0x0A: 0 == unlock still to be shown.
            if (!lpProfileCar->WasUnlockSequenceAlreadyShown())
            {
                mbCarUnlockPending = true;
                return;
            }

            liCarCount = lpProfile->GetCarCount();
            ++liCarIndex;
            ++lpProfileCar;
            if (liCarIndex >= liCarCount)
            {
                mbCarUnlockPending = false;
                return;
            }
        }
    }

    // @ 0x824EEBE0 -- resolve a GUI replay slot index to the underlying reel index by
    // linear-searching the embedded ReplayStatusInterface's reels for the one matching the
    // slot's cached reel handle (maReplayReelForSlot). Bounds-asserts the slot, then asserts
    // the reel was located.
    s32 GuiCache::ReplayConvertGuiSlotIndexToReelIndex(s32 liSlotIndex) const
    {
        CGS_ASSERT(liSlotIndex >= 0, "liSlotIndex >= 0");
        CGS_ASSERT(liSlotIndex < 6, "liSlotIndex < BrnReplays::KI_MAX_REELS");
        CGS_ASSERT(liSlotIndex < miReplaySlotsUsed, "liSlotIndex < miReplaySlotsUsed");

        const BrnReplays::ReplayIO::StatusInterface* lpStatus =
            reinterpret_cast<const BrnReplays::ReplayIO::StatusInterface*>(mReplayStatusInterfaceStorage);

        // The cached slot->reel entry is the X360 reel handle (a 32-bit word); compare it
        // against each reel pointer StatusInterface::GetReel hands back (32-bit compare, as
        // on the X360 target).
        const s32 liReelForSlot = maReplayReelForSlot[liSlotIndex];
        s32 liReelIndex = 0;
        while (liReelForSlot
               != static_cast<s32>(reinterpret_cast<std::uintptr_t>(lpStatus->GetReel(liReelIndex))))
        {
            if (++liReelIndex >= 6)
            {
                CGS_ASSERT(false, "Didn't find the reel thats requested");
                break;
            }
        }
        return liReelIndex;
    }

    // @ 0x824EEDA8 -- has the replay actor for the given active-race-car index been rendered
    // (maReplayARCRendered @0x143E0). Range-asserts the ARCI.
    bool GuiCache::IsReplayARCRendered(EActiveRaceCarIndex leActiveRaceCarIndex) const
    {
        CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                   "leARCI >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leARCI < E_ACTIVE_RACE_CAR_INDEX_COUNT");
        return maReplayARCRendered[leActiveRaceCarIndex];
    }
}

#include "GameSource/Gui/BrnGuiCache.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX. Online-player / stunt-list accessors that
// index the cache's far member tables at their asm-proven offsets. CGS_ASSERT is a no-op
// in this build (CgsAssert.h), matching the project convention for the X360 assert
// machinery -- each getter fires the bounds assert on a bad index, then returns the raw
// table element the release build would have.

namespace BrnGui
{
    // @ 0x8240F770  ( maStuntToDisplay @+0xAC5C, stride 8, -1-terminated )
    const StuntToDisplayInfo* GuiCache::GetStuntToDisplay(s32 liIndex) const
    {
        // GetNumberOfStuntsToDisplay() inlined: walk the list counting leading valid
        // entries (miStuntId == -1 terminates); the X360 loop is bounded at 1.
        s32 liNumberOfStuntsToDisplay = 0;
        const StuntToDisplayInfo* lpStunt = maStuntToDisplay;
        do
        {
            if (lpStunt->miStuntId == -1)
            {
                break;
            }
            ++liNumberOfStuntsToDisplay;
            ++lpStunt;
        }
        while (liNumberOfStuntsToDisplay < 1);

        CGS_ASSERT(liIndex < liNumberOfStuntsToDisplay, "liIndex < GetNumberOfStuntsToDisplay()");

        return &maStuntToDisplay[liIndex];
    }

    // @ 0x8240F890  ( maPlayerInfo @+0xAC80, stride 312 == sizeof(InGamePlayerStatusData) )
    const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData*
        GuiCache::GetOnlinePlayerInfo(s32 liIndex) const
    {
        CGS_ASSERT(liIndex >= 0, "liPlayerInfoIndex >= 0");
        CGS_ASSERT(liIndex < 8, "liPlayerInfoIndex < KI_MAX_PLAYERS");

        return reinterpret_cast<const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData*>(
            maPlayerInfo[liIndex]);
    }

    // @ 0x8240F910  ( maCurrentPlayerTeam @+0xB808, 4*(idx+11778)+this )
    s32 GuiCache::GetCurrentOnlinePlayerTeam(EActiveRaceCarIndex leActiveRaceCarIndex) const
    {
        CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                   "lePlayerActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "lePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

        return maCurrentPlayerTeam[leActiveRaceCarIndex];
    }
}

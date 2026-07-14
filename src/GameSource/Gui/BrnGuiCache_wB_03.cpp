#include "GameSource/Gui/BrnGuiCache.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX. Two ARCI-indexed online-player state
// accessors from the GuiCache online-lobby table span (@0xB84C..). Each bounds-checks
// the E_ACTIVE_RACE_CAR_INDEX argument via the debug assert front-end (a no-op through
// CgsAssert.h in this build) and then reads the corresponding bool lane. Store-for-store
// with the X360 asm; raw far offsets map to the frozen header's named members.

namespace BrnGui
{
    // @ 0x8240F988 -- maOnlinePlayerDisconnected @ +0xB84C (47180)
    bool GuiCache::GetOnlinePlayerDisconnected(EActiveRaceCarIndex leActiveRaceCarIndex) const
    {
        CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                   "lePlayerActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "lePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

        return maOnlinePlayerDisconnected[leActiveRaceCarIndex];
    }

    // @ 0x8240FA08 -- maOnlinePlayerEliminated @ +0xB85C (47196)
    bool GuiCache::IsOnlinePlayerEliminated(EActiveRaceCarIndex leActiveRaceCarIndex) const
    {
        CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                   "leCurrentPlayer >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leCurrentPlayer < E_ACTIVE_RACE_CAR_INDEX_COUNT");

        return maOnlinePlayerEliminated[leActiveRaceCarIndex];
    }
}

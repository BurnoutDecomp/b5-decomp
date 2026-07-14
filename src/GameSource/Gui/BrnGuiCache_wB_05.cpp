#include "GameSource/Gui/BrnGuiCache.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX. GuiCache race-car-info accessors
// (mRaceCarInfo SoA, ARCI-indexed). Each reads one named lane at its asm-proven
// offset, guarded by the game's debug asserts (CGS_ASSERT is a no-op in this
// build, matching the X360 release assert machinery). Offsets / branch senses
// come straight from the X360 ARTIST asm; members are accessed BY NAME against
// the recovered GuiCache layout in BrnGuiCache.h.

namespace BrnGui
{
    // @ 0x82443750
    const Vector4& GuiCache::GetRaceCarPosition(EActiveRaceCarIndex leActiveRaceCarIndex) const
    {
        CGS_ASSERT(E_ACTIVE_RACE_CAR_INDEX_0 <= leActiveRaceCarIndex,
                   "Invalid EActiveRaceCarIndex : ");
        CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "Invalid EActiveRaceCarIndex : ");
        CGS_ASSERT(maRaceCarUsed[leActiveRaceCarIndex],
                   "true == mRaceCarInfo.mabCarsUsed[leActiveRaceCarIndex]");
        return maRaceCarPositions[leActiveRaceCarIndex];
    }

    // @ 0x824438A8
    bool GuiCache::IsRaceCarCrashing(EActiveRaceCarIndex leActiveRaceCarIndex) const
    {
        CGS_ASSERT(E_ACTIVE_RACE_CAR_INDEX_0 <= leActiveRaceCarIndex,
                   "Invalid EActiveRaceCarIndex : ");
        CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "Invalid EActiveRaceCarIndex : ");
        CGS_ASSERT(maRaceCarUsed[leActiveRaceCarIndex],
                   "true == mRaceCarInfo.mabCarsUsed[leActiveRaceCarIndex]");
        return maRaceCarCrashing[leActiveRaceCarIndex];
    }

    // @ 0x82443A00
    bool GuiCache::IsActiveRaceCarIndexUsed(EActiveRaceCarIndex leActiveRaceCarIndex) const
    {
        CGS_ASSERT(E_ACTIVE_RACE_CAR_INDEX_0 <= leActiveRaceCarIndex,
                   "Invalid EActiveRaceCarIndex : ");
        CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "Invalid EActiveRaceCarIndex : ");
        return maRaceCarUsed[leActiveRaceCarIndex];
    }
}

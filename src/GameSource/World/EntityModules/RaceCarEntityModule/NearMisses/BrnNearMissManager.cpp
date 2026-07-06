#include "GameSource/World/EntityModules/RaceCarEntityModule/NearMisses/BrnNearMissManager.h"

// BrnWorld::NearMissManager::HasThereBeenARecentNearMiss @ 0x822CD2E8. Reconstructed from
// BURNOUT_X360_ARTIST.XEX. True iff either the traffic-vehicle near-miss list or the race-car
// near-miss list currently holds a remembered vehicle.
//
// The X360 body inlines the traffic side's query (reads mTrafficNearMissData.maNearMiss' count
// word @ this+0x54, firing the CgsArray Construct/Clear assert, then tests count>0) and issues a
// real call to the race-car side (mRaceCarNearMissData @ this+0x148). Both collapse to the same
// public NearMissData<A,B>::HasThereBeenARecentNearMiss() query at source level; the compiler's
// choice to inline one and call the other is a codegen detail with no observable difference.
bool BrnWorld::NearMissManager::HasThereBeenARecentNearMiss() const
{
    if (mTrafficNearMissData.HasThereBeenARecentNearMiss())
    {
        return true;
    }
    return mRaceCarNearMissData.HasThereBeenARecentNearMiss();
}

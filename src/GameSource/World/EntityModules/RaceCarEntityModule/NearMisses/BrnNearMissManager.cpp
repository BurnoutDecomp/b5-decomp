#include "GameSource/World/EntityModules/RaceCarEntityModule/NearMisses/BrnNearMissManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the not-already-contained tripwire)

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

// BrnWorld::NearMissManager::AddNearRaceCar @ 0x822ED198. Log a race-car section/entity id into
// the race-car near-section working list. The X360 ARTIST build guards with a manager-level debug
// assert (the rodata names the manager member mRaceCarNearMissData), then defers the length<cap
// append to NearMissData<4,7>::AddNear (which the compiler inlines here).
void BrnWorld::NearMissManager::AddNearRaceCar(u32 luRaceCarId)
{
    CGS_ASSERT(!mRaceCarNearMissData.IsContainedInNearArray(luRaceCarId),
               "!mRaceCarNearMissData.IsContainedInNearArray( luRaceCarId )");
    mRaceCarNearMissData.AddNear(luRaceCarId);
}

// BrnWorld::NearMissManager::AddNearTraffic @ 0x822ED250. Sibling of AddNearRaceCar for the
// traffic-vehicle near list (NearMissData<4,8> @ +0x0C); the AddNear body is likewise inlined.
void BrnWorld::NearMissManager::AddNearTraffic(u32 luEntityId)
{
    CGS_ASSERT(!mTrafficNearMissData.IsContainedInNearArray(luEntityId),
               "!mTrafficNearMissData.IsContainedInNearArray( luEntityId )");
    mTrafficNearMissData.AddNear(luEntityId);
}

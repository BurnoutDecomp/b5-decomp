#include "GameSource/Network/Managers/BrnNetworkLiveRevengeRelationship.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// @ 0x82355540  int __fastcall BrnNetwork::LiveRevengeRelationship::GetTotalTakedowns(_DWORD *a1)
// Sum of the local player's and the rival's lifetime takedowns across the whole
// relationship. X360: return *a1 + a1[9]  ->  mOverallStats.mPlayerStats.miTakedowns
// (+0) + mOverallStats.mRivalStats.miTakedowns (+36). The X360 build asserts the
// total is non-negative before returning it.
// Original assert site: GameSource/Network/Managers/BrnNetworkLiveRevengeRelationship.h:389
s32 BrnNetwork::LiveRevengeRelationship::GetTotalTakedowns() const
{
    const s32 liTotal = mOverallStats.mPlayerStats.miTakedowns +
                        mOverallStats.mRivalStats.miTakedowns;

    CGS_ASSERT(liTotal >= 0,
               "mOverallStats.mPlayerStats.miTakedowns + mOverallStats.mRivalStats.miTakedowns >= 0");

    return liTotal;
}

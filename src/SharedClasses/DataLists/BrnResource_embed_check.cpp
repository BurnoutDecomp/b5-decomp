// Embed/compile check for the class:BrnResource leaf batch.
//
// Forces compilation of the header-inline accessor reconstructed in this batch
// (ChallengeListEntryAction::GetActionType, inline in ChallengeListEntry.h) and pulls
// in the resource-id factory + container homes so the gate exercises every owned home
// in one TU. The free-function / explicit-instantiation bodies live in their own .cpp
// files (BrnAssetIds.cpp, CgsResourcePtr_DataListResources.cpp,
// IndexedPool_GameDataEventSlot.cpp); this TU only verifies the header surfaces and
// the inline accessor.
#include "SharedClasses/DataLists/ChallengeListEntry.h"
#include "GameSource/Resource/SharedIO/BrnAssetIds.h"
#include "GameShared/GameClasses/Containers/CgsIndexedPool.h"
#include "GameSource/Resource/BrnGameDataEventSlot.h"

namespace
{
    // Reference the inline GetActionType accessor so it is instantiated/compiled here.
    BrnResource::ChallengeListEntryAction::EChallengeActionType
    ProbeGetActionType(const BrnResource::ChallengeListEntryAction& lrAction)
    {
        return lrAction.GetActionType();
    }

    // Reference the IndexedPool accessor body for the GameDataEventSlot element type.
    BrnResource::GameDataEventSlot&
    ProbePool(CgsContainers::IndexedPool<BrnResource::GameDataEventSlot, 256>& lrPool, s16 lsIndex)
    {
        return lrPool[lsIndex];
    }

    // Reference the asset-id factories so their declarations are checked against use.
    CgsID ProbeIds(const char* lpcName, u32 luUnit)
    {
        return BrnResource::MakeVehicleId(lpcName)
             + BrnResource::MakeTrafficVehicleId(lpcName)
             + BrnResource::MakeTrackUnitId(luUnit);
    }
}

#include "GameSource/World/AI/SharedIO/BrnRaceCarAIInterfaces.h"
#include "GameSource/World/AI/BrnAICar.h"          // BrnAI::AICar / Route / RouteNode accessors
#include "GameSource/World/AI/Route/BrnRoute.h"    // BrnAI::Route / RouteNode

// Out-of-line bodies of BrnAI::AIModuleIO::AIRaceCarInterface's 7-function TU: the
// inactive/out-of-range per-car snapshot published by the AI module each frame
// (position + facing + updated/pass-through bitsets) plus the two player route-node
// positions cached for extrapolation. Index math and bit-array reads are X360
// store-for-store against the grown layout in BrnRaceCarAIInterfaces.h.

namespace BrnAI
{
namespace AIModuleIO
{
    // @0x8276D120  ProcessOutOfRangeVehicles publishes one inactive car's snapshot.
    void AIRaceCarInterface::UpdateInactiveRaceCarData(EGlobalRaceCarIndex leGlobalRaceCarIndex,
                                                       Vector3 lPosition, Vector3 lAt)
    {
        CGS_ASSERT((leGlobalRaceCarIndex < BrnWorld::KI_MAX_OUT_OF_RANGE_RACE_CARS) &&
                       (leGlobalRaceCarIndex != E_GLOBAL_RACE_CAR_INDEX_INVALID),
                   "( leGlobalRaceCarIndex < BrnWorld::KI_MAX_OUT_OF_RANGE_RACE_CARS ) && ( leGlobalRaceCarIndex != E_GLOBAL_RACE_CAR_INDEX_INVALID )");
        CGS_ASSERT(!mSetRaceCars.IsBitSet(static_cast<u32>(leGlobalRaceCarIndex)),
                   "Inactive race car AI data being set twice");

        mSetRaceCars.SetBit(static_cast<u32>(leGlobalRaceCarIndex));

        maPositions[leGlobalRaceCarIndex] = lPosition;
        maAts[leGlobalRaceCarIndex]       = lAt;
    }

    // @0x8276D360  ProcessInRangeVehicles records the can-pass-through-traffic flag.
    void AIRaceCarInterface::UpdateAllRaceCarData(EGlobalRaceCarIndex leGlobalRaceCarIndex,
                                                  bool lbCanPassThroughTraffic)
    {
        CGS_ASSERT(leGlobalRaceCarIndex < BrnWorld::KI_MAX_OUT_OF_RANGE_RACE_CARS,
                   "leGlobalRaceCarIndex < BrnWorld::KI_MAX_OUT_OF_RANGE_RACE_CARS");

        if (lbCanPassThroughTraffic)
        {
            mCanPassThroughTraffic.SetBit(static_cast<u32>(leGlobalRaceCarIndex));
        }
    }

    // @0x827650B8  ExportCarData caches the player's current + next route-node (x,y).
    void AIRaceCarInterface::SetPlayerRouteNodePositions(const AICar* lpAICar)
    {
        CGS_ASSERT(lpAICar != NULL, "lpAICar != NULL");
        const Route* lpPlayerRoute = lpAICar->GetRoute();
        CGS_ASSERT(lpPlayerRoute != NULL, "lpPlayerRoute != NULL");

        const s32 liNextNodeIndex = lpAICar->GetNextRouteNodeIndex();

        if (liNextNodeIndex < lpPlayerRoute->GetNodeCount() - 1)
        {
            const RouteNode* lpRouteNode     = lpPlayerRoute->GetNode(liNextNodeIndex);
            const RouteNode* lpNextRouteNode = lpPlayerRoute->GetNode(liNextNodeIndex + 1);

            CGS_ASSERT(lpRouteNode != NULL, "lpRouteNode != NULL");
            CGS_ASSERT(lpNextRouteNode != NULL, "lpNextRouteNode != NULL");

            // Only the planar (x,y) is carried; the z/w lanes are zeroed (std r9=0).
            mCurrentNodePosition.x = lpRouteNode->GetX();
            mCurrentNodePosition.y = lpRouteNode->GetY();
            mCurrentNodePosition.z = 0.0f;
            mCurrentNodePosition.w = 0.0f;

            mNextNodePosition.x = lpNextRouteNode->GetX();
            mNextNodePosition.y = lpNextRouteNode->GetY();
            mNextNodePosition.z = 0.0f;
            mNextNodePosition.w = 0.0f;
        }
    }

    // @0x822B2DC0  True iff UpdateInactiveRaceCarData has recorded this car.
    bool AIRaceCarInterface::WasInactiveRaceCarUpdated(s8 liRaceCar) const
    {
        CGS_ASSERT(liRaceCar < BrnWorld::KI_MAX_OUT_OF_RANGE_RACE_CARS,
                   "liRaceCar < BrnWorld::KI_MAX_OUT_OF_RANGE_RACE_CARS");

        return mSetRaceCars.IsBitSet(static_cast<u32>(liRaceCar));
    }

    // @0x822B3010  The recorded world position of an updated inactive car.
    Vector3 AIRaceCarInterface::GetInactiveRaceCarPosition(s8 liRaceCar) const
    {
        CGS_ASSERT(liRaceCar < BrnWorld::KI_MAX_OUT_OF_RANGE_RACE_CARS,
                   "liRaceCar < BrnWorld::KI_MAX_OUT_OF_RANGE_RACE_CARS");
        CGS_ASSERT(mSetRaceCars.IsBitSet(static_cast<u32>(liRaceCar)),
                   "mSetRaceCars.IsBitSet( liRaceCar )");

        return maPositions[liRaceCar];
    }

    // @0x822B3178  The recorded facing ("at") of an updated inactive car.
    Vector3 AIRaceCarInterface::GetInactiveRaceCarAt(s8 liRaceCar) const
    {
        CGS_ASSERT(liRaceCar < BrnWorld::KI_MAX_OUT_OF_RANGE_RACE_CARS,
                   "liRaceCar < BrnWorld::KI_MAX_OUT_OF_RANGE_RACE_CARS");
        CGS_ASSERT(mSetRaceCars.IsBitSet(static_cast<u32>(liRaceCar)),
                   "mSetRaceCars.IsBitSet( liRaceCar )");

        return maAts[liRaceCar];
    }

    // @0x822B2EE8  True iff the car is permitted to pass through traffic.
    bool AIRaceCarInterface::CanPassThroughTraffic(s8 liRaceCar) const
    {
        CGS_ASSERT(liRaceCar < BrnWorld::KI_MAX_OUT_OF_RANGE_RACE_CARS,
                   "liRaceCar < BrnWorld::KI_MAX_OUT_OF_RANGE_RACE_CARS");

        return mCanPassThroughTraffic.IsBitSet(static_cast<u32>(liRaceCar));
    }
}
}

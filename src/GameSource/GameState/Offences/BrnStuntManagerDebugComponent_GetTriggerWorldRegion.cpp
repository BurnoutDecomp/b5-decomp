#include "GameSource/GameState/Offences/BrnStuntManagerDebugComponent.h"

#include "GameSource/GameState/Offences/BrnStuntManager.h"   // BrnGameState::StuntManager (mpStuntManager target)
#include "GameShared/GameClasses/World/CgsWorldMap2D.h"      // CgsWorld::WorldMap2D::GetValue, KU_INVALID_WORLD_MAP_VALUE

// ============================================================================
// BrnGameState::StuntManagerDebugComponent::GetTriggerWorldRegion @ 0x8236F070
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. This body lands the function keyed
// in the ledger to SharedClasses/World/BrnWorldRegion.h (its callees are
// BrnWorld::WorldRegion::Construct / ::DistrictToCounty + CgsWorld::WorldMap2D::
// GetValue) in the StuntManagerDebugComponent's own home, where the function
// actually lives.
//
// X360 flow (asm @ 0x8236F070):
//   v10[0..2] = position[0..2];  v10[3] = 0.0;          (build a Vector3, w=0)
//   value = WorldMap2D::GetValue(mpStuntManager->mWorldMap2D @ +880, v10);
//   result.meDistrict = 18 (E_DISTRICT_INVALID);
//   result.meCounty   = WorldRegion::DistrictToCounty(18);
//   if ( value != 255 )                                  (KU_INVALID_WORLD_MAP_VALUE)
//       result.Construct(value);
//   return result;
//
// The X360 stores meDistrict = E_DISTRICT_INVALID and meCounty =
// DistrictToCounty(E_DISTRICT_INVALID) directly before the branch -- which is
// byte-identical to WorldRegion::Construct(E_DISTRICT_INVALID) (it asserts
// 18 < E_DISTRICT_COUNT == 19, then sets the same two members). Reconstructed via
// Construct() so the private WorldRegion members are written BY NAME rather than
// by raw store. The position is consumed as three floats with a zero w lane,
// exactly as the X360 vector load does (the trailing vperm is the SIMD register
// staging for the Vector3 GetValue overload).
// ============================================================================

namespace BrnGameState
{

BrnWorld::WorldRegion
StuntManagerDebugComponent::GetTriggerWorldRegion(const Vector3& lrTriggerPosition)
{
    // Build the sample position: x/y/z from the trigger, w lane forced to 0.
    Vector3 lSamplePosition;
    lSamplePosition.x = lrTriggerPosition.x;
    lSamplePosition.y = lrTriggerPosition.y;
    lSamplePosition.z = lrTriggerPosition.z;
    lSamplePosition.w = 0.0f;

    const u8 luValue =
        mpStuntManager->GetDistrictMap().GetValue(lSamplePosition);

    // Default the region to the invalid district (county derived from it) -- the
    // same two-member write the X360 emits unconditionally before the branch.
    BrnWorld::WorldRegion lRegion;
    lRegion.Construct(BrnWorld::E_DISTRICT_INVALID);

    // On-map sample: rebuild the region from the sampled district byte.
    if (luValue != CgsWorld::KU_INVALID_WORLD_MAP_VALUE)
    {
        lRegion.Construct(static_cast<BrnWorld::EDistrict>(luValue));
    }

    return lRegion;
}

}

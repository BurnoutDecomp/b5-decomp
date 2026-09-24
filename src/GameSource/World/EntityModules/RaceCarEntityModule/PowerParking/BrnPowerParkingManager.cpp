#include "GameSource/World/EntityModules/RaceCarEntityModule/PowerParking/BrnPowerParkingManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameSource/Math/BrnMathUtils.h"            // BrnMath::GetPointToInfiniteLineDistance (declared there)
#include "rw/math/vpu/vector3_operation.h"           // rw::math::vpu::Dot / Magnitude / MagnitudeSquared

// ============================================================================
// GameSource/World/EntityModules/RaceCarEntityModule/PowerParking/BrnPowerParkingManager.cpp
//
// PowerParkingManager members (reconstructed from BURNOUT_X360_ARTIST.XEX).
//
// The free candidacy test BrnWorld::CheckVehicleForPowerPark (X360 @0x822B1FA0) is a header
// inline (DWARF BrnPowerParkingManager.h:182) and lives there, transcribed from the ARTIST dump
// (crash parity FX-RCEM4 2026-09-24). Its one callee without a body, BrnMath::
// GetPointToInfiniteLineDistance (@0x82540448), is bodied at the foot of this file -- see the
// MOVE-WHEN note there.
// ============================================================================
namespace BrnWorld
{

    // Max perpendicular distance (from the player to the target vehicle's heading line) that still
    // counts as "aligned". DWARF file-scope global BrnPowerParkingManager.cpp:30, a plain (tweakable,
    // non-const) float32_t: the image keeps it in .data with its siblings (0x82CDB4C0..0x82CDB50C),
    // and x360rd reads flt_82CDB4D4 == 0x40400000 == 3.0f. DetermineOutcome loads it at 0x822A74D0.
    f32 KF_MAX_PERPENDICULAR_DISTANCE_FOR_ALIGNMENT = 3.0f;

    // X360 0x822C24B8 -- reset the per-attempt scoring state, then register the scorer's debug
    // component with the debug menu. Returns true (the RaceCarEntityModule::Prepare convention).
    // Grounded store-for-store on the asm: only the fields the asm writes are cleared here (it
    // leaves miOverallRating, the traffic/parked counts, last-frame pose and the closest-*
    // measurements untouched).
    bool PowerParkingManager::Prepare()
    {
        mfProximityScore                 = 0.0f;
        mfRotationScore                  = 0.0f;
        mfDistanceScore                  = 0.0f;
        mfSpeedScore                     = 0.0f;
        mfPositionAlignmentScore         = 0.0f;
        mfAngleAlignmentScore            = 0.0f;

        miWeightedDistanceScore          = 0;
        miWeightedProximityScore         = 0;
        miWeightedSpeedScore             = 0;
        miWeightedRotationScore          = 0;
        miWeightedPositionAlignmentScore = 0;
        miWeightedAngleAlignmentScore    = 0;

        mfLowestSpeedThisPark            = 3.4028235e38f;   // FLT_MAX (flt_8201442C)

        mePowerParkOutcome               = E_PPO_TO_BE_DETERMINED;
        mfTimeUntilDisplayOutcome        = 0.0f;
        mbPowerParkInProgress            = false;

        mPowerParkingDebugComponent.Register();
        return true;
    }

    // X360 0x822A74A0 -- resolve the final power-park outcome once the attempt has settled.
    void PowerParkingManager::DetermineOutcome()
    {
        // Need at least two nearby parked cars, and the player must be well aligned to the kerb
        // (perpendicular distance below the alignment threshold); otherwise the outcome is left
        // undetermined and nothing is scored.
        if (muNearbyParkedCarCount < 2u ||
            mfClosestPerpendicularDist >= KF_MAX_PERPENDICULAR_DISTANCE_FOR_ALIGNMENT)
        {
            mePowerParkOutcome = E_PPO_TO_BE_DETERMINED;
            return;
        }

        miOverallRating = miWeightedDistanceScore
                        + miWeightedProximityScore
                        + miWeightedSpeedScore
                        + miWeightedRotationScore
                        + miWeightedPositionAlignmentScore
                        + miWeightedAngleAlignmentScore;

        CGS_ASSERT(miOverallRating >= 0, "miOverallRating >= 0");

        if (miOverallRating >= 100)
            miOverallRating = 100;

        // Only promote a still-undetermined attempt to a success (a FAILURE stays a FAILURE).
        if (mePowerParkOutcome == E_PPO_TO_BE_DETERMINED)
            mePowerParkOutcome = E_PPO_SUCCESS;
    }
}

// ============================================================================
// BrnMath::GetPointToInfiniteLineDistance -- X360 0x82540448 (DWARF home Math/BrnMathUtils.cpp,
// locals lLineDir :116, lLineToPoint :120, lfDelta :122, lProjectedPointOnLine :124,
// lPointToProjectedPoint :126, lfDistance :128; its assert names BrnMathUtils.cpp:118,
// li r5, 0x76). Its only caller is CheckVehicleForPowerPark (0x822B2294).
//
// MOVE-WHEN: BrnMathUtils.cpp (declared in BrnMathUtils.h:75) belongs to no lane of the crash
// parity campaign, so the body is homed beside its only caller; move it into BrnMathUtils.cpp
// verbatim when that file is next opened, and delete it here in the same commit.
//
// The projection is NOT divided by |lLineDir|^2: the console takes lfDelta = Dot(lLineDir,
// lLineToPoint) (vmsum3fp128 at 0x82540504) and multiplies straight back (vmaddfp128 at
// 0x82540530). That is the perpendicular distance for a unit direction -- the traffic transform's
// Z row and ActiveRaceCar::GetDirection -- and the console's answer, not a distance, otherwise.
// ============================================================================
namespace BrnMath
{
    f32 GetPointToInfiniteLineDistance(Vector3 lPoint, Vector3 lPointOnLine1, Vector3 lPointOnLine2)
    {
        const Vector3 lLineDir = lPointOnLine2 - lPointOnLine1;                     // 0x82540484 vsubfp128

        // 0x825404A8 vmsum3fp128 ; 0x825404B0 vcmpgtfp. against 0.0 (flt_82001CC0) ; bne all-true
        // skips. So a zero-length or NaN direction fires.
        CGS_ASSERT(rw::math::vpu::MagnitudeSquared(lLineDir) > 0.0f, "RwMath::MagnitudeSquared( lLineDir ) > 0.0f");

        const Vector3 lLineToPoint = lPoint - lPointOnLine1;                         // 0x825404E4 vsubfp128
        const f32 lfDelta = rw::math::vpu::Dot(lLineDir, lLineToPoint);              // 0x82540504 vmsum3fp128
        const Vector3 lProjectedPointOnLine = lLineDir * lfDelta + lPointOnLine1;    // 0x82540530 vmaddfp128
        const Vector3 lPointToProjectedPoint = lProjectedPointOnLine - lPoint;       // 0x82540534 vsubfp128

        // 0x82540538..0x82540568: vmsum3fp128, vrsqrtefp + two Newton steps, x * rsqrt(x), vsel 0 at 0.
        // FLAG (PC-platform, numeric): the rsqrt estimate is de-optimised to std::sqrt (Magnitude).
        const f32 lfDistance = rw::math::vpu::Magnitude(lPointToProjectedPoint);
        return lfDistance;
    }
}

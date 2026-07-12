#include "GameSource/World/EntityModules/RaceCarEntityModule/PowerParking/BrnPowerParkingManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// ============================================================================
// GameSource/World/EntityModules/RaceCarEntityModule/PowerParking/BrnPowerParkingManager.cpp
//
// PowerParkingManager members (reconstructed from BURNOUT_X360_ARTIST.XEX).
//
// NOTE: the free candidacy test BrnWorld::CheckVehicleForPowerPark (X360 @0x822B1FA0)
// is intentionally NOT homed here. Its body is absent from every recovered dossier, and
// a prior reconstruction FABRICATED it -- inventing rodata angle constants presented as
// attested, an inferred register-ABI signature, unconfirmable CGS_ASSERT strings, and a
// declare-only BrnMath::GetPointToInfiniteLineDistance -- so it was removed pending a real
// disassembly dump (block-don't-fabricate).
// ============================================================================
namespace BrnWorld
{

    // Max perpendicular distance (from the player to the target vehicle's heading line) that still
    // counts as "aligned". DWARF file-scope global (BrnPowerParkingManager.cpp:30). Its X360 rodata
    // value (flt_82CDB4D4) is NOT in this dossier, so it is declared extern here and DEFINED when
    // Update/UpdateScoring are homed with the recovered rodata float pool -- referenced by NAME
    // only, never a fabricated value.
    extern const f32 KF_MAX_PERPENDICULAR_DISTANCE_FOR_ALIGNMENT;

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

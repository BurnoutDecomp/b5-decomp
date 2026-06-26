#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"

#include <cmath>   // std::fabs

// BrnPhysics::Vehicle::VehicleManager -- takedown impact classification.
// This TU is reconstructed PARTIALLY: only the classifier entry point
// CheckForAllTypesOfImpacts is bodied here (the full 64-function VehicleManager is built out by
// its own reconstruction passes). The eight per-type sub-classifiers it dispatches to are
// declared in the header and bodied by their own TUs.

namespace BrnPhysics
{
namespace Vehicle
{
    // The minimum combined closing speed below which a contact is too gentle to be any kind of
    // takedown/shunt (X360 reads the rodata float at flt_82FB8290 for the `>=` gate).
    // FLAG: the rodata value is not in the per-function exports -- shipped as a flagged 0.0f
    // placeholder (with which the gate is a pass-through). Resolve the real threshold from the
    // XEX .rdata @0x82FB8290 before relying on the gate magnitude.
    static const f32 KF_MIN_IMPACT_SPEED_SUM = 0.0f;   // FLAG: rodata flt_82FB8290 value unrecovered

    // -------------------------------------------------------------------------------------------
    // CheckForAllTypesOfImpacts  @0x82642E58
    //
    // The takedown CLASSIFIER. Given a populated RaceCarResponseInfo, run the per-type
    // sub-classifiers in strict priority order and stop at the first that fires (first-match-wins;
    // each sub-classifier commits its own side effects -- crashing the victim etc. -- when it
    // matches, so this entry point returns void). Priority order (from the X360 asm):
    //   1. player slamming an AI into another AI   2. hitting an already-crashing car
    //   -- the remaining geometric tests only run if NEITHER car is already crashing --
    //   3. vertical   4. T-bone   5. head-to-head   6. shunt/nudge   7. slam/trading-paint
    //   8. stationary-target.
    // A leading energy gate skips classification entirely for gentle contacts
    // (|speedB| + |speedA| must clear KF_MIN_IMPACT_SPEED_SUM).
    //
    // FLAG (signature): the X360 Hex-Rays rendered this `int(int result, int a2)`; the DWARF
    // (BrnVehicleManager.h:1182) gives the true shape `void CheckForAllTypesOfImpacts(
    // RaceCarResponseInfo*)`. The "result" the pseudocode returns is the first-match value, which
    // the declared void signature discards.
    // -------------------------------------------------------------------------------------------
    void VehicleManager::CheckForAllTypesOfImpacts(RaceCarResponseInfo* lpInfo)
    {
        // Energy gate: ignore contacts whose combined closing speed is below the threshold.
        if (std::fabs(lpInfo->mfRaceCarBSpeed) + std::fabs(lpInfo->mfRaceCarASpeed) < KF_MIN_IMPACT_SPEED_SUM)
            return;

        // Highest priority: a player shunting an AI into a third AI, and re-hits on a car that is
        // already crashing -- these run even if a car is mid-crash.
        if (CheckForPlayerSlammingAIIntoAI(lpInfo))    return;
        if (CheckForHittingAlreadyCrashingCar(lpInfo)) return;

        // The geometric classifiers only apply to a fresh impact: a car already crashing cannot be
        // freshly taken down.
        if (lpInfo->mbRaceCarAIsCrashing || lpInfo->mbRaceCarBIsCrashing)
            return;

        if (CheckForVerticalTakedown(lpInfo))   return;
        if (CheckForTBoneTakedown(lpInfo))      return;
        if (CheckForHeadToHead(lpInfo))         return;
        if (CheckForShuntAndNudge(lpInfo))      return;
        if (CheckForSlamAndTradingPaint(lpInfo)) return;
        CheckForStationaryTargetTakedown(lpInfo);
    }
}
}

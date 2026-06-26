#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"

#include <cmath>    // std::fabs
#include <cstddef>  // offsetof (layout asserts)

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

    // -------------------------------------------------------------------------------------------
    // The sentinel the per-car crash-state array holds while a car is in the fatal/active crash
    // state. InstantTakedown skips re-crashing the victim when its crash-state already equals this.
    // FLAG: no recovered enum home for the crash-state values -- the X360 compares the array slot
    // against the bare literal 2, so it is reproduced here as a named integer literal (NOT an
    // invented enum). Resolve the real crash-state enum and replace this when its home is recovered.
    static const s32 KI_RACECAR_CRASH_STATE_FATAL = 2;   // FLAG: literal sentinel; enum home unrecovered

    // -------------------------------------------------------------------------------------------
    // InstantTakedown  @0x82636108
    //
    // The takedown COMMIT routine the impact classifiers call once a takedown is decided. It:
    //   1. decodes the victim and aggressor EntityIds to active-car indices (the recurring
    //      `(muValue >> 10) & 0x3FFF` packing used TU-wide);
    //   2. does nothing unless takedowns are enabled (the master gate at mbTakedownsEnabled);
    //   3. crashes the victim via SetRaceCarCrashing -- UNLESS the victim is already in the fatal
    //      crash state -- forwarding the collision normal + contact point, the four output/
    //      deformation interfaces, and the takedown type (lfNormalStressSq is NOT forwarded);
    //   4. if the victim is the local player, zeroes the aggressor's per-car recovery timer;
    //   5. records the aggressor (via miAttackerToRecord) as the victim's last attacker, and marks
    //      the aggressor's "taken down this frame" status byte.
    //
    // INDEXING NOTE (asm-authoritative, surprising): the crash-state check and the last-attacker
    // write are indexed by the VICTIM slot, while the recovery-timer zero and the taken-down status
    // byte are indexed by the AGGRESSOR slot. This matches the X360 exactly (5216*v39 and 224*v39
    // use the aggressor index v39; 4*(v38+...) use the victim index v38) -- reproduced verbatim.
    //
    // FLAG (signature): the X360 Hex-Rays rendered this with a 37-arg `int(...)` prototype -- an
    // artefact of SIMD-spilled Vector3s and pass-through registers. The DWARF (BrnVehicleManager.h
    // :1257) gives the true 10-parameter shape used here; the returned `HIDWORD(a1)` the pseudocode
    // produces is the SetRaceCarCrashing result threaded back through r4, which the void return drops.
    // -------------------------------------------------------------------------------------------
    void VehicleManager::InstantTakedown(EntityId lVictimEntityId,
                                         EntityId lAggressorEntityId,
                                         Vector3 lCollisionNormal,
                                         Vector3 lContactPoint,
                                         f32 lfNormalStressSq,
                                         BrnPhysics::PhysicsModuleIO::VehicleOutputRequestInterface* lpRequestOutputInterface,
                                         VehicleManagerOutputInterface* lpManagerOutputInterface,
                                         BrnGameState::GameStateModuleIO::VehicleOutputInterface* lpVehicleOutputInterface,
                                         BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
                                         BrnGameState::ETakedownType leTakedownType)
    {
        // lfNormalStressSq is decided by the classifier but not used by the commit (the X360 never
        // forwards a3); reference it so the unused parameter is explicit rather than a warning.
        (void)lfNormalStressSq;

        // Decode both entities to their active-car slots (TU-wide packing: bits 10..23 of muValue).
        const s32 liVictimActiveRaceCarIndex    = static_cast<s32>((lVictimEntityId.muValue    >> 10) & 0x3FFF);
        const s32 liAggressorActiveRaceCarIndex = static_cast<s32>((lAggressorEntityId.muValue >> 10) & 0x3FFF);

        // Master gate: do nothing at all unless takedowns are currently enabled.
        if (!mbTakedownsEnabled)
            return;

        // Crash the victim, unless it is already in the fatal/active-crash state.
        if (maRaceCarCrashState[liVictimActiveRaceCarIndex] != KI_RACECAR_CRASH_STATE_FATAL)
        {
            SetRaceCarCrashing(lVictimEntityId,
                               lAggressorEntityId,
                               lCollisionNormal,
                               lContactPoint,
                               lpRequestOutputInterface,
                               lpManagerOutputInterface,
                               lpVehicleOutputInterface,
                               lpDeformationInterface,
                               leTakedownType);
        }

        // If the player was the one taken down, reset the aggressor's recovery timer.
        if (mePlayerActiveRaceCarIndex == liVictimActiveRaceCarIndex)
            maRaceCarVehicles[liAggressorActiveRaceCarIndex].mfRecoveryTimer = 0.0f;

        // Record who took the victim down, and flag the aggressor as having scored a takedown this frame.
        maRaceCarLastAttacker[liVictimActiveRaceCarIndex]   = miAttackerToRecord;
        maRaceCarStatus[liAggressorActiveRaceCarIndex].mbTakenDown = 1;
    }

    // -------------------------------------------------------------------------------------------
    // Layout pins for the deep members InstantTakedown reaches. Never called; exists only to host
    // the offsetof asserts (offsetof on a private member must be evaluated in member-function scope).
    // Each offset here is asm-proven; if a padding run drifts, the gate fails -- that is intended.
    // -------------------------------------------------------------------------------------------
    void VehicleManager::_AssertLayout()
    {
        static_assert(sizeof(RaceCarStatusRecord)  == 224,  "RaceCarStatusRecord stride (asm: 224)");
        static_assert(offsetof(RaceCarStatusRecord, mbTakenDown) == 124, "taken-down byte (asm: +124)");
        static_assert(sizeof(RaceCarVehicleRecord) == 5216, "RaceCarVehicleRecord stride (asm: 5216)");
        static_assert(offsetof(RaceCarVehicleRecord, mfRecoveryTimer) == 5120, "recovery timer (asm: +5120)");

        static_assert(offsetof(VehicleManager, maRaceCarStatus)          == 0,      "maRaceCarStatus (asm base 0)");
        static_assert(offsetof(VehicleManager, maRaceCarVehicles)        == 1856,   "maRaceCarVehicles (asm base 1856)");
        static_assert(offsetof(VehicleManager, maRaceCarCrashState)      == 44192,  "maRaceCarCrashState (asm base 44192)");
        static_assert(offsetof(VehicleManager, mbTakedownsEnabled)       == 171464, "mbTakedownsEnabled (asm +171464)");
        static_assert(offsetof(VehicleManager, miAttackerToRecord)       == 171540, "miAttackerToRecord (asm +171540)");
        static_assert(offsetof(VehicleManager, maRaceCarLastAttacker)    == 171684, "maRaceCarLastAttacker (asm base 171684)");
        static_assert(offsetof(VehicleManager, mePlayerActiveRaceCarIndex) == 172204, "mePlayerActiveRaceCarIndex (DWARF/asm +172204)");
    }
}
}

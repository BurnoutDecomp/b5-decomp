// =================================================================================================
// GameSource/Physics/VehicleManager/BrnVehicleManager_InstantTakedown.cpp
//
// VehicleManager::InstantTakedown @0x82636108 -- BUILD-MECHANICS SPLIT ONLY (2026-08-11,
// create-drain wave; the RaceCarPhysics_Construct.cpp precedent). The body below is byte-identical
// to the one that lived in BrnVehicleManager.cpp:499 and its declared home is unchanged; it moved
// because DoHornTakedowns (BrnVehicleManager_DriverArms.cpp, mounted this wave) calls it every
// frame the horn cheat fires, and the home TU is still unmountable. Its one out-of-TU callee,
// SetRaceCarCrashing @0x82634C90, resolves to the loud trap in the mounted
// BrnVehicleManagerLinkStubs.cpp -- the honest state for this edge path until the 923-insn crash
// commit lands. TO RE-MERGE: mount BrnVehicleManager.cpp, move this body back, delete this TU.
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"

namespace BrnPhysics
{
namespace Vehicle
{
    // -------------------------------------------------------------------------------------------
    // InstantTakedown  @0x82636108
    //
    // The takedown COMMIT routine the impact classifiers call once a takedown is decided. It:
    //   1. decodes the victim and aggressor EntityIds to active-car indices (the recurring
    //      `(muValue >> 10) & 0x3FFF` packing used TU-wide);
    //   2. does nothing unless takedowns are enabled (the master gate at mbSlamsAndShuntsOn);
    //   3. crashes the victim via SetRaceCarCrashing -- UNLESS the victim is already in the fatal
    //      crash state -- forwarding the collision normal + contact point, the four output/
    //      deformation interfaces, and the takedown type (lfNormalStressSq is NOT forwarded);
    //   4. if the victim is the local player, zeroes the aggressor's per-car recovery timer;
    //   5. records the aggressor (via mfMinSecondsBetweenImpacts) as the victim's last attacker, and marks
    //      the aggressor's "taken down this frame" status byte.
    //
    // INDEXING NOTE (asm-authoritative, surprising): the car-type check and the last-attacker
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
                                         BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
                                         VehicleManagerOutputInterface* lpManagerOutputInterface,
                                         BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutputInterface,
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
        if (!mbSlamsAndShuntsOn)
            return;

        // Crash the victim, unless it is a NETWORK car (the X360 `!= 2`; see the maeRaceCarTypes
        // note above -- this used to read as "already in the fatal crash state").
        if (maeRaceCarTypes[liVictimActiveRaceCarIndex] != BrnWorld::E_RACE_CAR_TYPE_NETWORK)
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
            maRaceCarVehicles[liAggressorActiveRaceCarIndex].mfTimeSinceTookDownPlayer = 0.0f;

        // Record who took the victim down, and flag the aggressor as having scored a takedown this frame.
        mafNoImpactTimeSeconds[liVictimActiveRaceCarIndex]   = mfMinSecondsBetweenImpacts;
        maRaceCarDrivers[liAggressorActiveRaceCarIndex].mControls.mbIsInvulnerableToVehicles = true;
    }
}
}

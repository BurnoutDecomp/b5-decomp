// =================================================================================================
// GameSource/Physics/VehicleManager/BrnVehicleManager_ReadUpdatedBodies.cpp
//
// THE PER-FRAME GRAVITY + INTEGRATION STEP, for race cars AND traffic.
//   VehicleManager::ReadUpdatedBodies          @0x82619A10 (198 insns)
//   PhysicalTrafficManager::ReadUpdatedBodies  @0x825EF608 (334 insns)
//
// Both were conductor gates (BrnPhysicsConductorGates.cpp) / absent; both are real here, and the
// gate for the first is DELETED in the same commit (LNK2005 is the intended tripwire if it comes
// back).
//
// THE NAME IS A LIE AND THE COMMITTED HEADER COMMENT WAS WRONG. "ReadUpdatedBodies" reads
// NOTHING back from the simulation. The X360 body takes the OutUpdateRigidBody queue in r4,
// stashes it at var_1C, and NEVER DEREFERENCES IT -- it only forwards it, unread, to the traffic
// manager's twin, where it is consumed by a DEBUG DUPLICATE CHECK and nothing else. The header's
// old claim ("then drain the sim's OutUpdateRigidBody queue into the owning cars") describes code
// that does not exist in either body, and its other clause ("live cars NOT owned by the sim this
// frame") describes an ownership test the asm does not make -- the only guard is mbFrozen. Both
// are corrected at the declaration.
//
// What these two functions ACTUALLY are, and why that matters more than the name: a Burnout race
// car is a BrnPhysics::ExternalPhysicsBody -- the game integrates it ITSELF and only publishes the
// pose to rw::physics. rw::physics' own SimulationParams::mGravity never touches it, and
// ExternalPhysicsBody::CalculateNewVelocity loads no constants at all. So THIS is the only place
// gravity enters a car, and the only place a car's pose advances. With this gated, a body added to
// the simulation would have sat perfectly still forever. (The identification of the constant is
// banked at BrnVehicleConstants.h:10-46, [V] 2026-08-03; this wave re-derived the same reading
// independently from the asm before writing a line.)
//
// AS-SHIPPED, NAMED HONESTLY: the y-lane subtraction below is a raw Euler velocity step with no
// ground reaction of any kind, because the traction-line family that produces the road contact
// (StartVehicleTractionLineTests / EndVehicleTractionLineTests / ReadRaceCarTractionLineTest-
// Results) is still gated. That is the console's own arithmetic, not a simplification -- but it
// means a body in the sim TODAY would accelerate downward unopposed. The create path is
// deliberately NOT enabled in this commit for exactly that reason.
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"      // KF_GRAVITY
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT

namespace BrnPhysics
{
namespace Vehicle
{
namespace
{
    // The one shared leg of both bodies, byte-for-byte the same instruction group in each
    // (X360 0x82619AF8..0x82619B30 and 0x825EF914..0x825EF94C):
    //     v13 = <splatted KF_GRAVITY> * <splatted dt>
    //     v0  = vspltw(mLinearVelocity, 1)          ; lane 1 == .y
    //     v0  = v0 - v13
    //     v12 = vrlimi128(mLinearVelocity, v0, 4, 0); mask 4 == insert lane 1 only
    //     store v12 back over mLinearVelocity
    //     IntegrateTransform(dt)                    ; dt still live in v1
    // i.e. mLinearVelocity.y -= KF_GRAVITY * dt, then advance the pose. The console writes the
    // whole 16-byte register back with x/z/w carried through unchanged, which is what assigning
    // the single named lane expresses.
    inline void ApplyGravityAndIntegrate(ExternalPhysicsBody& lrBody, VecFloat lvfDeltaTime)
    {
        Vector3 lvLinearVelocity = lrBody.GetLinearVelocity();
        lvLinearVelocity.y -= KF_GRAVITY * lvfDeltaTime.x;
        lrBody.SetLinearVelocity(lvLinearVelocity);

        lrBody.IntegrateTransform(lvfDeltaTime);
    }
}

// -------------------------------------------------------------------------------------------
// VehicleManager::ReadUpdatedBodies   @0x82619A10   (DWARF :366)
//
// Body, read from the asm:
//   r19 = this + 44224 == &mUsedRaceCars  (`addis r19,r14,1 ; addi r19,r19,-0x5340`)
//   the two inlined BitArray<8u> scans (first-set-bit: field scan + isolate-lowest +
//   `word*64 + 63 - cntlzd`; next-set-bit: linear within the field then re-scan), with the
//   container's own bounds assert baked at CgsBitArray.h:203 -- expressed here through the
//   committed GetFirstNonZeroBit()/GetNextNonZeroBit() pair, whose bit math is that same
//   arithmetic, and with the assert hoisted to the call site (the committed BitArray header is
//   deliberately assert-free; the CrashingRaceCarInterface::SetFromVehicleOutputInterface
//   precedent in BrnVehicleOutputInterface.cpp does exactly this).
//   per set slot:  `mulli r11, r31, 0x1460 ; add r11,r11,r14 ; addi r11,r11,0x740`
//                  == &maRaceCarVehicles[slot]  (console stride 5216, base +1856)
//                  `lbz r10, 0x70(r11)` == mbFrozen -> skip when set
//                  the shared gravity+integrate leg above, on the +0x10 base sub-object
//   tail:          r3 = this + 44768, r4 = the (still unread) queue, v1 = dt
//                  -> PhysicalTrafficManager::ReadUpdatedBodies
//
// EVERY SEAT IS REACHED BY NAME. The console's 0x1460 stride is 5216 and the host's
// sizeof(RaceCarPhysics) is 5008 (BrnVehicleManager.h's KU_HOST_DRIFT_AFTER_RACECAR_ARRAY note),
// so an offset-based transcription of this function would have indexed into the wrong car from
// slot 1 onward while still compiling, linking and running.
// -------------------------------------------------------------------------------------------
void VehicleManager::ReadUpdatedBodies(
    const CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::OutUpdateRigidBody, 200>* lpUpdatedBodyQueue,
    VecFloat lvfTimeStep)
{
    for (s32 liRaceCar = mUsedRaceCars.GetFirstNonZeroBit();
         liRaceCar >= 0;
         liRaceCar = mUsedRaceCars.GetNextNonZeroBit(liRaceCar))
    {
        CGS_ASSERT(static_cast<u32>(liRaceCar) < 8u, "invalid index : < 8");   // CgsBitArray.h:203

        RaceCarPhysics& lrRaceCar = maRaceCarVehicles[liRaceCar];

        if (lrRaceCar.IsFrozen())
        {
            continue;
        }

        ApplyGravityAndIntegrate(lrRaceCar, lvfTimeStep);
    }

    // The queue is handed on exactly as received -- this function never looked inside it.
    mPhysicalTrafficManager.ReadUpdatedBodies(lpUpdatedBodyQueue, lvfTimeStep);
}

// -------------------------------------------------------------------------------------------
// PhysicalTrafficManager::ReadUpdatedBodies   @0x825EF608
//
// Two halves, and the FIRST is the only consumer of the queue anywhere in this pair:
//
//  1. A DEV DUPLICATE-DETECTION SWEEP. The O(n^2) double loop compares every event's leading
//     8-byte mID against every other's and fires "Recieved two update events for rigid body
//     0x%08X%08X" (the console's own spelling, typo included) at :1945 on a match. It reads
//     nothing else and changes nothing. `i == j` is skipped by the console's own `cmpw r22,r24 ;
//     beq` -- so the guard is index equality, not id equality.
//
//  2. The same gravity + IntegrateTransform leg as the race-car half, over the used-traffic
//     bitset (`addis r22,r20,2 ; addi r22,r22,-0x6798` == this + 104552 == mUsedTrafficVehicles,
//     capacity 20), through mpaTrafficVehicles[i].mpVehicleBody (`lwz 0x1C`), guarded by that
//     vehicle's mu8PhysicalType (`lbz 0x32`) being E_PHYSICAL_TRAFFIC_TYPE_FULL and by the body's
//     own mbFrozen (`lbz 0x70`).
//
// THE TYPE TEST IS EMITTED TWICE and both copies are kept. The console reads mu8PhysicalType,
// range-asserts it (BrnPhysicalTrafficVehicle.h:382) and branches on it; then RE-READS it,
// range-asserts it AGAIN and asserts "IsFullyPhysical()" (:391) before dereferencing
// mpVehicleBody. That is `if (!IsFullyPhysical()) continue;` followed by an inlined
// GetVehiclePhysics() which re-checks the same predicate -- an inlining artifact of the shipped
// source, reproduced rather than folded: folding it away would remove a shipped assert, and a
// shipped assert is behaviour.
// -------------------------------------------------------------------------------------------
void PhysicalTrafficManager::ReadUpdatedBodies(
    const CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::OutUpdateRigidBody, 200>* lpUpdatedBodies,
    VecFloat lvfTimeStep)
{
    CGS_ASSERT(lpUpdatedBodies != 0, "lpUpdatedBodies != NULL");     // :1932

    // ---- 1. the dev duplicate sweep (the queue's ONLY reader) --------------------------------
    for (s32 liEvent = 0; liEvent < lpUpdatedBodies->GetLength(); ++liEvent)
    {
        const u64 lu64Id = lpUpdatedBodies->GetEvent(liEvent).mID;

        for (s32 liOther = 0; liOther < lpUpdatedBodies->GetLength(); ++liOther)
        {
            if (liEvent == liOther)
            {
                continue;
            }

            CGS_ASSERT(lpUpdatedBodies->GetEvent(liOther).mID != lu64Id,
                       "Recieved two update events for rigid body 0x%08X%08X");   // :1945
        }
    }

    // ---- 2. gravity + integrate, per fully-physical traffic vehicle --------------------------
    for (s32 liVehicle = mUsedTrafficVehicles.GetFirstNonZeroBit();
         liVehicle >= 0;
         liVehicle = mUsedTrafficVehicles.GetNextNonZeroBit(liVehicle))
    {
        // (the console's assert text names the source-level constant ku8TotalMaxNumPhysical-
        //  Traffic; this tree spells that same 20 as KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC, and the
        //  message is kept verbatim as the repo convention requires)
        CGS_ASSERT(static_cast<u32>(liVehicle) < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC,
                   "liVehicle < ku8TotalMaxNumPhysicalTraffic");                  // :741

        PhysicalTrafficVehicle& lrVehicle = mpaTrafficVehicles[liVehicle];

        // the first inlined copy of the predicate
        CGS_ASSERT(lrVehicle.mu8PhysicalType < PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_COUNT,
                   "leType < E_PHYSICAL_TRAFFIC_TYPE_COUNT");                     // :382
        if (lrVehicle.mu8PhysicalType != PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_FULL)
        {
            continue;
        }

        // the second copy, inlined from GetVehiclePhysics() -- kept as shipped
        CGS_ASSERT(lrVehicle.mu8PhysicalType < PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_COUNT,
                   "leType < E_PHYSICAL_TRAFFIC_TYPE_COUNT");                     // :382
        CGS_ASSERT(lrVehicle.mu8PhysicalType == PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_FULL,
                   "IsFullyPhysical()");                                          // :391

        SimpleVehiclePhysics* const lpBody = lrVehicle.mpVehicleBody;

        if (lpBody->IsFrozen())
        {
            continue;
        }

        ApplyGravityAndIntegrate(*lpBody, lvfTimeStep);
    }
}
}
}

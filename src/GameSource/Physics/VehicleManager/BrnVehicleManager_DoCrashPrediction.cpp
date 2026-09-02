// =================================================================================================
// GameSource/Physics/VehicleManager/BrnVehicleManager_DoCrashPrediction.cpp
//
// The HEAD of the crash-prediction web.
//
//   VehicleManager::DoCrashPrediction  @0x82645FE0 (814)  -- DWARF BrnVehicleManager.h:341
//
// Called every frame by PhysicsModule::Update (BrnPhysicsModuleUpdateFunctions.cpp:416). The chain:
//   DoCarCarContactGeneration -> maCustomEventQueues[8] -> DoCrashPredictionForRaceCarAndTrafficVehicle
//   -> HandleCrashPredictionForRaceCarAndTrafficVehicle -> HandleRaceCarTrafficCarPotentialContact
//   -> PhysicalTrafficManager::SetTrafficVehicle{Crashing,Slammed,Checked} / TestForNearMissFreakOut
//   -> the PhysicalTrafficState readback the world consumes.
//
// Read off the ARTIST asm (.ida-exports 0x82645FE0). Hex-Rays renders it as a 31-int prototype
// because of the PPC float-arg GPR skip: r3=this, r4/r5 = the two stacks, f1 = timestep with r6
// SKIPPED, then r7 input / r8 vehicle-out / r9 request-out / r10 manager-out and the last two
// interfaces on the stack. The eight leading asserts name every parameter (BrnVehicleManager.cpp
// :2880..:2887), which is what pins the mapping.
//
// THE AVERAGER IS A STACK LOCAL OF THIS FUNCTION. `v149[1680] @ sp+0x150` with `v150 @ sp+0x7E0`
// == 0x150+0x690 is exactly PotentialContactAverager (20 * 0x50 pairs, 20 weights @0x640, count
// @0x690), and `v150 = 0` right after AllocateInternalBuffers is its only initialisation. That is
// why no PotentialContactAverager instance exists anywhere else in the tree: the console has none.
//
// QUEUE INDICES, byte-proven against BrnPhysicsModuleIO_PotentialContactInterface.h's
// `16 + n * 0x28010` map:
//     ifc + 2130144 -> [13] traffic-with-traffic        (count at +2130152)
//     ifc + 1310864 -> [ 8] race-car-with-traffic       (count at +1310872)
//     ifc + 1474720 -> [ 9] traffic-with-world          (count at +1474728)
//
// THREE NAMED GATES, each at its seat below. None of them is on the race-car-vs-traffic path.
// (A fourth -- the race-car-vs-WORLD driver at seat (2b) -- was DISCHARGED 2026-09-02 by the crash
// wave; its two callees are bodied in BrnVehicleManager_WorldCrashArm.cpp.)
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"
#include "GameSource/Physics/VehicleManager/BrnPotentialContactAverager.h"
#include "GameSource/Physics/BrnPhysicsModuleIO_PotentialContactInterface.h"
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h"
#include "GameShared/GameClasses/Module/CgsIOBufferStack.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnPhysics
{
namespace Vehicle
{

// -------------------------------------------------------------------------------------------
// DoCrashPrediction  @0x82645FE0 (814)  -- DWARF BrnVehicleManager.h:341
// -------------------------------------------------------------------------------------------
void VehicleManager::DoCrashPrediction(
    CgsModule::IOBufferStack* lpInputBufferStack,
    CgsModule::IOBufferStack* lpOutputBufferStack,
    f32 lfTimeStep,
    const VehicleInputInterface* lpInputInterface,
    VehicleOutputInterface* lpVehicleOutputInterface,
    BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
    VehicleManagerOutputInterface* lpManagerOutputInterface,
    BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
    BrnPhysics::PhysicsModuleIO::PotentialContactInterface* lpContactInterface)
{
    typedef CgsSceneManager::SceneManagerIO::PotentialContact                       PotentialContact;
    typedef PhysicsModuleIO::PotentialContactInterface::CustomPotentialContactQueue Queue;

    // The eight leading asserts, BrnVehicleManager.cpp:2880..:2887, in console order.
    CGS_ASSERT(lpInputBufferStack != nullptr,       "lpInputBufferStack != NULL");             // :2880
    CGS_ASSERT(lpOutputBufferStack != nullptr,      "lpOutputBufferStack != NULL");            // :2881
    CGS_ASSERT(lpInputInterface != nullptr,         "lpInputInterface != NULL");               // :2882
    CGS_ASSERT(lpVehicleOutputInterface != nullptr, "lpVehicleOutputInterface != NULL");       // :2883
    CGS_ASSERT(lpRequestOutputInterface != nullptr, "lpRequestOutputInterface != NULL");       // :2884
    CGS_ASSERT(lpManagerOutputInterface != nullptr, "lpVehicleManagerOutputInterface != NULL");// :2885
    CGS_ASSERT(lpDeformationInterface != nullptr,   "lpDeformationInterface != NULL");         // :2886
    CGS_ASSERT(lpContactInterface != nullptr,       "lpPotentialContactQueue != NULL");        // :2887

    // `stwx r30, r15, r11` x2 @0x82646180/84 -> +172416 / +172420, the car-car prediction cache.
    muCachedCarASlot = 0;
    muCachedCarBSlot = 0;

    mPhysicalTrafficManager.AllocateInternalBuffers(lpInputBufferStack, lpOutputBufferStack);

    // v149 @ sp+0x150 (1680 bytes) + v150 @ sp+0x7E0 == the averager; `stw r30, var_B0` is its
    // count = 0. Constructed per frame, on the stack, exactly as the console does.
    PotentialContactAverager lContactPairAverager;
    lContactPairAverager.Reset();

    // `stwx r30, r15, 0x273A8` -> +160680 == mDiscardedContacts.miLength.
    mDiscardedContacts.Clear();

    // ---- (1) traffic-vs-traffic, custom queue [13] -----------------------------------------
    // Owner bytes: A at record +0x30, B at +0x38 (HIBYTE on a big-endian target is byte 0).
    // 0 = world/scene, 1 = race car, 2 = traffic vehicle.
    {
        const Queue& lrQueue = lpContactInterface->GetTrafficWithTrafficQueue();
        for (s32 liIndex = 0; liIndex < lrQueue.GetLength(); ++liIndex)
        {
            const PotentialContact& lContact = lrQueue.GetEvent(liIndex);
            const u8 lu8OwnerA = lContact.muVolumeInstanceIdA.GetEntityIDOwner();
            const u8 lu8OwnerB = lContact.muVolumeInstanceIdB.GetEntityIDOwner();

            if (lu8OwnerA == 0u)
            {
                CGS_ASSERT(lu8OwnerB != 1u,
                           "Found a race car-world potential contact outside of race car-world queue");  // :2911
                CGS_ASSERT(lu8OwnerB != 2u,
                           "Found a traffic-world potential contact outside of traffic-world queue");    // :2912
            }

            // GATE VehicleManager::HandleTrafficCarTrafficCarPotentialContact @0x8263EC90 (279)
            //   blocker: unbodied. Args are (contact BY VALUE, request, vehicle, manager,
            //   deformation, timestep) -- same shape as the race-car arm. DELETE-WHEN it lands.
            (void)(lu8OwnerA == 2u && lu8OwnerB == 2u);
        }
    }

    // ---- (2) race-car-vs-PHYSICAL-traffic, custom queue [8] ---------------------------------
    // THE QUEUE RECORDS STILL HOLD GLOBAL ENTITY IDS ON BOTH SIDES.
    // FixUpVehicleContacts splices the physical id into a LOCAL VolumeInstanceId and passes it to
    // the DeformationManager (BrnPhysicsModuleUpdateFunctions.cpp:110-119); it never writes the id
    // back into the record (the console's @0x825A6010 has no `std` into the queue storage, only
    // two stack slots). That is why the consumer does its own lookup --
    // HandleRaceCarTrafficCarPotentialContact @0x8263FA50 calls
    // GetTrafficPhysicsEntityIDFromGlobalEntityID_Safe on the record's B id
    // (BrnVehicleManager_RaceCarTrafficContact.cpp:415-428).
    // The console re-reads miLength every iteration (`lwz r11, 8(r29)` @0x826463FC) and copies the
    // 80-byte record to the stack before the call; the by-value local reproduces both.
    {
        const Queue& lrQueue = lpContactInterface->GetRaceCarWithTrafficQueue();

        for (s32 liIndex = 0; liIndex < lrQueue.GetLength(); ++liIndex)
        {
            const PotentialContact lContact = lrQueue.GetEvent(liIndex);   // 80-byte stack copy
            DoCrashPredictionForRaceCarAndTrafficVehicle(
                &lContactPairAverager, &lContact, lfTimeStep,
                lpVehicleOutputInterface, lpRequestOutputInterface,
                lpManagerOutputInterface, lpDeformationInterface);
        }
    }

    // Final flush of whatever the loop accumulated (@0x82646428).
    HandleCrashPredictionForRaceCarAndTrafficVehicle(
        &lContactPairAverager, lfTimeStep,
        lpVehicleOutputInterface, lpRequestOutputInterface,
        lpManagerOutputInterface, lpDeformationInterface);

    // ---- (2b) race-car-vs-WORLD (@0x82646428+): the validated queue [6], grouped per car and
    //      ordered by predicted impact time, then classified + committed contact by contact.
    // GATE DISCHARGED 2026-09-02 (crash wave). The DELETE-WHEN that stood here ("both callees are
    // declare-only") is met: HandleRaceCarWorldPotentialContact @0x8263E3B8 and
    // PredictCarWorldContactTime @0x825B5300 are bodied in BrnVehicleManager_WorldCrashArm.cpp and
    // BrnVehicleManagerCrashPrediction.cpp is mounted. Argument order is the console's
    // (timestep, contactIfc, inputIfc, vehicleOut, requestOut, managerOut, deformIn); the
    // GameStateModuleIO fork spelling on arg 4 was retired with it (BrnVehicleManager.h banner).
    // THIS IS THE LINE THAT MAKES HITTING A WALL ACTUALLY CRASH THE CAR.
    HandleCrashPredictionForRaceCarAndWorld(
        lfTimeStep, lpContactInterface, lpInputInterface,
        lpVehicleOutputInterface, lpRequestOutputInterface,
        lpManagerOutputInterface, lpDeformationInterface);

    // ---- (3) the force-no-slow-mo clear (0x82646450..0x82646B8C) ---------------------------
    // GATE: the ~410-insn VMX128 block guarded by
    //   maRaceCarVehicles[mePlayerActiveRaceCarIndex].mbCrashedThisFrame (+0xE53) && mbForceNoSlowMo
    // -- it walks the player car's cached triangle list and CLEARS mbForceNoSlowMo when the
    // nearest hit's normal-dot exceeds 0.7. Blocker: CgsCachedTriangleList (48-byte per-car cache
    // records, mpaTriangleCache) is not read anywhere in this tree yet.
    // DELETE-WHEN the triangle-cache reader lands. EFFECT OF THE GATE, bounded: mbForceNoSlowMo is
    // set only by HandleRaceCarTrafficCarPotentialContact and is read + reset unconditionally one
    // statement later (BrnPhysicsModuleUpdateFunctions.cpp:438/447), so the gate can only inhibit
    // slow-mo for the single frame of a traffic hit that the console would have un-inhibited.

    // ---- (4) traffic-vs-world, custom queue [9] ---------------------------------------------
    // GATE: VehicleManager::HandleTrafficCarWorldPotentialContact @0x8263F0F0 (220) -- unbodied.
    // Drains ifc+1474720 with (contact BY VALUE, request, vehicle, manager, deformation, timestep).
    // DELETE-WHEN that body lands; it is the traffic-car-hits-scenery arm, not this round's.

    // ---- tail --------------------------------------------------------------------------------
    // The console inlines BridgeArticulatedJointRequestsToSim here; its two asserts are the ones
    // Hex-Rays cites at BrnPhysicalTrafficManager.h:880/881.
    mPhysicalTrafficManager.BridgeArticulatedJointRequestsToSim(lpRequestOutputInterface);
    mPhysicalTrafficManager.DeallocateInternalBuffers(lpInputBufferStack, lpOutputBufferStack);
}

}   // namespace Vehicle
}   // namespace BrnPhysics

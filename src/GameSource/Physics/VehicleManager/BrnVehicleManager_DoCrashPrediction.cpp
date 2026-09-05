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

            // 0x826462FC..0x8264630C: both owners TRAFFIC_VEHICLE -> the traffic-traffic arm, with
            // the 80-byte record copied to the outgoing-arg area (r4..r10 + the 24-byte memcpy).
            // GATE DISCHARGED 2026-09-02 (traffic crash wave): bodied in
            // BrnVehicleManager_TrafficCrashArms.cpp.
            if (lu8OwnerA == 2u && lu8OwnerB == 2u)
            {
                HandleTrafficCarTrafficCarPotentialContact(
                    lContact, lpRequestOutputInterface, lpVehicleOutputInterface,
                    lpManagerOutputInterface, lpDeformationInterface, lfTimeStep);
            }
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
    // nearest hit's normal-dot exceeds 0.7.
    // ⛔ THE OLD BLOCKER ("CgsCachedTriangleList has no reader") IS RETIRED and must not be quoted
    // again: TriangleCacheInterface::GetCache / GetNumCachedTriangleBatches are bodied and live, and
    // this block's own prologue uses their exact idiom. So does everything else it needs.
    //
    // ⭐ RECONNAISSANCE, 2026-09-05 (detach wave). This wave read the block end to end and did NOT
    // body it -- for a scheduling reason, not a decoding one, and the next wave should not have to
    // re-derive any of the following. NOTHING IN IT IS UNREADABLE:
    //   PROLOGUE (0x82646450..0x82646620), fully decoded:
    //     r23 = this + 0x2A0AC  (mePlayerActiveRaceCarIndex)   r22 = this + 0x2A11D (mbForceNoSlowMo)
    //     r31 = <arg> + 0x1F410 (the TriangleCacheInterface; asserts "mpTriangleCacheManager != NULL",
    //           CgsSceneManagerModuleIO.h:0x506)
    //     per-car slot record stride 48; `lwz +0x24` == miIndexIntoTriangleCache and `lwz +0x28` ==
    //     miNumCachedTriangleBatches -- i.e. GetCache/GetNumCachedTriangleBatches INLINED, on the
    //     player's own slot (mePlayerActiveRaceCarIndex, the same numbering
    //     AddRaceCarTractionLineTests uses). Triangle4 stride 0xE0.
    //     The ray is built from the player RaceCarPhysics at +0xDE0, +0x780 and +0x770.
    //   LOOP (0x826466C0..0x82646AF8), one Triangle4 batch per iteration, 4 triangles wide:
    //     nine `lvx128` at batch+0x00..+0x80 then vmrghw/vmrglw transposes to SoA x/y/z of the three
    //     verts; two `vpermwi128 0x63` cross products, each normalised by vrsqrtefp + TWO Newton
    //     refinements; an edge/barycentric sign test folded with vand into a 4-lane mask; four
    //     `vspltw` + `vcmpeqfp.` lane peels deciding "any hit"; then vrefp + two Newton steps for
    //     1/denominator, and a four-stage `vsel` running MINIMUM that carries the nearest t in v59
    //     with its companions in v57/v58 (all three read back through the IDA vA-swap: `vor128 v5,
    //     v91, v59` is v5 = v59, and `vxor128 v0, v90, v0` is -v58).
    //   EPILOGUE (0x82646AFC..0x82646B88): if any lane hit, dot3(-v58, playerCar+0x770) > 0.7 clears
    //     mbForceNoSlowMo (`stb r30, 0(r22)`, r30 == 0).
    //   ⭐ EVERY CONSTANT IS RECOVERED -- none of them is a flagged zero any more:
    //     unk_82FB9F10 <- flt_8200D5F0 == 1e-8    (splat; the degeneracy epsilon)
    //     unk_82FB9EF0 <- flt_82004884 == 1e-5    (splat)
    //     unk_82FBA360 <- flt_82004018 == 0.75    (lazily built, bit 0 of dword_82FBA370)
    //     unk_82FBA350 <- flt_82004C68 == 0.7     (lazily built, bit 1 -- the normal-dot threshold)
    //     the running-min seed is flt_8208F5EC == FLT_MAX (3.4028235e38)
    //     unk_82CDA3C0 / unk_82CDA400 are ordinary rodata vperm CONTROL vectors, readable with
    //       x360rd: {00 01 02 03, 00 01 02 03, 00 01 02 03, 14 15 16 17} and
    //               {08 09 0A 0B, 1C 1D 1E 1F, 00 01 02 03, 00 01 02 03}
    //   ⚠️ WHY IT WAS STILL LEFT: a 4-wide SoA intersection is the one shape where a half-right body
    //   is worse than none (it would silently answer "no hit" and look exactly like the gate), and
    //   this wave was already landing two crash-path corrections in the same subsystem -- one of
    //   which access-violated the game the first time it ran. Landing an unverified 460-instruction
    //   SIMD body beside an unverified crash fix would make the next failure unattributable.
    // EFFECT OF THE GATE, bounded: mbForceNoSlowMo is
    // set only by HandleRaceCarTrafficCarPotentialContact and is read + reset unconditionally one
    // statement later (BrnPhysicsModuleUpdateFunctions.cpp:438/447), so the gate can only inhibit
    // slow-mo for the single frame of a traffic hit that the console would have un-inhibited.

    // ---- (4) traffic-vs-world, custom queue [9] (0x82646B8C..0x82646C08) ----------------------
    // GATE DISCHARGED 2026-09-02 (traffic crash wave): HandleTrafficCarWorldPotentialContact
    // @0x8263F0F0 is bodied in BrnVehicleManager_TrafficCrashArms.cpp. The console re-reads
    // miLength every iteration (`lwz r11, 8(r29)` @0x82646BFC) and copies the record to the
    // outgoing-arg area before each call; the by-value local reproduces both. NOTE: on the console
    // this arm commits NOTHING (no store leaves its stack frame) -- traffic knocked into scenery is
    // not crashed here; see the TU banner before reading this loop as a "wall crash" for traffic.
    {
        const Queue& lrQueue = lpContactInterface->GetTrafficWithWorldQueue();
        for (s32 liIndex = 0; liIndex < lrQueue.GetLength(); ++liIndex)
        {
            const PotentialContact lContact = lrQueue.GetEvent(liIndex);
            HandleTrafficCarWorldPotentialContact(
                lContact, lpRequestOutputInterface, lpVehicleOutputInterface,
                lpManagerOutputInterface, lpDeformationInterface, lfTimeStep);
        }
    }

    // ---- tail --------------------------------------------------------------------------------
    // The console inlines BridgeArticulatedJointRequestsToSim here; its two asserts are the ones
    // Hex-Rays cites at BrnPhysicalTrafficManager.h:880/881.
    mPhysicalTrafficManager.BridgeArticulatedJointRequestsToSim(lpRequestOutputInterface);
    mPhysicalTrafficManager.DeallocateInternalBuffers(lpInputBufferStack, lpOutputBufferStack);
}

}   // namespace Vehicle
}   // namespace BrnPhysics

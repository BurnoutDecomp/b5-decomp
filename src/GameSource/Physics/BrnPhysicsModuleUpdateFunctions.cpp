// ============================================================================
// GameSource/Physics/BrnPhysicsModuleUpdateFunctions.cpp
//
// BrnPhysics::PhysicsModule -- the per-frame Update helper functions' home TU
// (the console path baked into this TU's asserts is
// "...\gamesource\unity\../Physics/BrnPhysicsModuleUpdateFunctions.cpp").
// The dossier keys this function to BrnPhysicsModule.cpp; the baked :915 path
// above is the byte-grounded correction.
//
// This slice: FixUpVehicleContacts @ 0x825A6010 (1067 insns) -- the big-five
// opener of the PhysicsModule::Update subtree. Reconstructed from the
// BURNOUT_X360_ARTIST.XEX asm; the PS3 DecFIGS out-of-line build (@0x699058)
// corroborates the structure call-for-call (it keeps GetPhysicsEntityIDFrom-
// GlobalEntityID and the queue GetEvent out-of-line where the X360 inlines /
// out-of-lines differently).
//
// PhysicsModule::Update @0x825B0640 (1,999 insns)
// LANDS BELOW -- the WorldLinkStubs boot gate is DELETED and this TU is the real
// per-frame physics conductor now. FixUpVehicleContacts' old "still a link stub"
// caller note is retired by the same commit.
// ============================================================================

#include "GameSource/Physics/BrnPhysicsModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"                // CgsSceneManager::EntityId
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h" // PotentialContact (muVolumeInstanceIdA/B)
#include "GameSource/Physics/BrnPhysicsModuleIO_PotentialContactInterface.h"  // the three custom-queue accessors
#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"            // VehicleManager::GetPhysicsEntityIDFromGlobalEntityID
#include "GameSource/Physics/DeformationManager/BrnDeformationManager.h"    // DeformationManager::FixUpVehicleContact[ByInterpolation]

#include "GameSource/Physics/BrnPhysicsModuleIO.h"                          // PhysicsModuleIO::{InputBuffer,OutputBuffer}
#include "GameSource/Physics/VehicleManager/BrnVehicleManagerIO.h"          // Vehicle::VehicleManagerOutputBuffer
// (Props::PropRaceCarContactBuffer arrives complete through BrnPhysicsModule.h ->
//  BrnPropManager.h, its DWARF home -- defined there 2026-08-09.)
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationModuleIO.h"    // CgsPhysics::PhysicsSimulationIO::{InputBuffer,OutputBuffer}
#include "GameShared/GameClasses/Module/CgsIOBufferStack.h"                 // CgsModule::IOBufferStack (CreateIOBuffer/DestroyIOBuffer)
#include "GameShared/GameClasses/Module/CgsModuleUtils.h"                   // CgsModule::Lock/UnlockBuffersForIO
#include "GameShared/GameClasses/Memory/CgsIOStackLinearMalloc.h"           // CgsMemory::IOStackLinearMalloc<1048576>
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.h" // CgsCollision::CollisionGenerator
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"   // CgsDev::PerfMonCpu::Start/StopMonitor
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                  // gpDebugPrint / gxMessageFilterFlags (the [pausebit] witnesses + the prop-diag trace)

namespace BrnPhysics
{
    // ---- [dv] the one-step velocity witness (DEFINED in ExternalPhysicsBody.cpp; read its
    // banner there for what it can and cannot see). NOT IN THE X360 BINARY, opt-in on
    // BRN_DV_PROBE=<threshold m/s>. The three entry points below bracket ONE physics step and
    // tag every accumulator drain with the stage it happened in; the whole ledger is printed
    // only for a step whose |dv| crosses the threshold. Declared extern here rather than in
    // ExternalPhysicsBody.h for the same reason gpCrashResponseDiagBody is
    // (BrnVehicleManager_UpdateVehiclePhysics.cpp:62): a diagnostic must not widen a shared
    // header's surface.
    void DvWitnessBeginStep();
    void DvWitnessMark(const char* lpcPhase);
    void DvWitnessEndStep(f32 lfTimeStep, u32 luFrame);

    namespace Vehicle
    {
        // The [kerb] probe's per-frame counter (DEFINED in
        // BrnVehicleManager_ValidateRaceCarWorldContact.cpp, bumped once per step by
        // DoRaceCarWorldContactValidation). Read here only so a [dv] dump carries the same
        // frame index as the [kerb]/[kerb-car]/[kerb-imp] lines for that step.
        extern u32 guKerbProbeFrame;
    }

    namespace
    {
        // Owner byte of a packed 64-bit VolumeInstanceId (entity word == the HIGH dword).
        inline u32 GetVolumeInstanceOwner(const CgsSceneManager::VolumeInstanceId& lrId)
        {
            return static_cast<u32>(lrId.muId >> 56) & 0xFFu;
        }

        // Splice a rewritten 32-bit physics entity id into the HIGH dword of a packed
        // 64-bit volume-instance id, preserving the low dword (X360 @0x825A6620:
        // `sldi rewritten,32 ; clrldi low,32 ; or`).
        inline CgsSceneManager::VolumeInstanceId SpliceEntityWord(
            const CgsSceneManager::VolumeInstanceId& lrId, CgsSceneManager::EntityId lNewEntityId)
        {
            CgsSceneManager::VolumeInstanceId lResult;
            lResult.muId = (static_cast<u64>(static_cast<u32>(lNewEntityId)) << 32)
                         | (lrId.muId & 0x00000000FFFFFFFFull);
            return lResult;
        }

        // [FLAG PC bring-up] PostSceneUpdate's player-index EDGE. NOT console state -- see the
        // PC-BUILD GUARD #1 banner at its use site. A file-scope static rather than a member
        // because PhysicsModule's layout is offset-pinned and this state is not the console's
        // (the same device, and the same reason, as
        // RaceCarEntityModule::PublishNewVehicleToDirectorWithoutPhysicsBringUp's edge flag).
        EActiveRaceCarIndex gs_ePublishedPlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
    }

    // ==========================================================================================
    // FixUpVehicleContacts @ 0x825A6010
    //
    // Deform-fix every vehicle-vs-vehicle potential contact this frame:
    //   1) the racecar-vs-traffic queue ([8]): assert the (RACECAR, TRAFFIC_VEHICLE) owner pair,
    //      rewrite the traffic B id GLOBAL->PHYSICAL, then FixUpVehicleContactByInterpolation;
    //   2) the racecar-vs-racecar queue ([7]): assert (RACECAR, RACECAR), no rewrite,
    //      FixUpVehicleContactByInterpolation;
    //   3) the scene-manager contact queue ([0]): FILTER (no assert) for (TRAFFIC, TRAFFIC)
    //      pairs, rewrite BOTH ids GLOBAL->PHYSICAL, then FixUpVehicleContact.
    // Queue lengths are re-read every iteration, as the console does (`lwz 8(queue)` inside the
    // loop). The contacts are mutated IN PLACE through the queues' storage -- the X360 walks the
    // event array directly and the DeformationManager callees write the records; GetEvent's
    // const overload is cast away exactly once per loop to reproduce that in-place write.
    // ==========================================================================================
    void PhysicsModule::FixUpVehicleContacts(
        PhysicsModuleIO::PotentialContactInterface* lpPotentialContactsInterface)
    {
        CGS_ASSERT(lpPotentialContactsInterface != nullptr, "lpPotentialContactsInterface != NULL");   // :915

        typedef PhysicsModuleIO::PotentialContactInterface::CustomPotentialContactQueue Queue;
        typedef CgsSceneManager::SceneManagerIO::PotentialContact                       PotentialContact;

        // ---- (1) racecar vs traffic -----------------------------------------------------------
        {
            const Queue& lrQueue = lpPotentialContactsInterface->GetRaceCarWithTrafficQueue();
            for (s32 liContact = 0; liContact < lrQueue.GetLength(); ++liContact)
            {
                PotentialContact& lrContact = const_cast<PotentialContact&>(lrQueue.GetEvent(liContact));

                const CgsSceneManager::VolumeInstanceId lRaceCarVolInstId = lrContact.muVolumeInstanceIdA;
                CgsSceneManager::VolumeInstanceId       lTrafficVolInstId = lrContact.muVolumeInstanceIdB;

                CGS_ASSERT(GetVolumeInstanceOwner(lRaceCarVolInstId) == 1u &&
                           GetVolumeInstanceOwner(lTrafficVolInstId) == 2u,
                           "lRaceCarVolInstId.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR && "
                           "lTrafficVolInstId.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");   // :947

                // The Fixup* id-rewrite: the traffic car's GLOBAL entity id -> its LOCAL PHYSICS id.
                const CgsSceneManager::EntityId lPhysicsId =
                    mVehicleManager.GetPhysicsEntityIDFromGlobalEntityID(
                        CgsSceneManager::EntityId(static_cast<u32>(lTrafficVolInstId.muId >> 32)));
                lTrafficVolInstId = SpliceEntityWord(lTrafficVolInstId, lPhysicsId);

                mDeformationManager.FixUpVehicleContactByInterpolation(lrContact, lRaceCarVolInstId,
                                                                       lTrafficVolInstId);
            }
        }

        // ---- (2) racecar vs racecar -----------------------------------------------------------
        {
            const Queue& lrQueue = lpPotentialContactsInterface->GetRaceCarWithRaceCarQueue();
            for (s32 liContact = 0; liContact < lrQueue.GetLength(); ++liContact)
            {
                PotentialContact& lrContact = const_cast<PotentialContact&>(lrQueue.GetEvent(liContact));

                CGS_ASSERT(GetVolumeInstanceOwner(lrContact.muVolumeInstanceIdA) == 1u &&
                           GetVolumeInstanceOwner(lrContact.muVolumeInstanceIdB) == 1u,
                           "lPotentialContact.muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR"
                           " && lPotentialContact.muVolumeInstanceIdB.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR");   // :975

                mDeformationManager.FixUpVehicleContactByInterpolation(lrContact,
                                                                       lrContact.muVolumeInstanceIdA,
                                                                       lrContact.muVolumeInstanceIdB);
            }
        }

        // ---- (3) the scene queue's traffic-vs-traffic pairs (filtered, not asserted) ----------
        {
            const Queue& lrQueue = lpPotentialContactsInterface->GetSceneManagerContactQueue();
            for (s32 liContact = 0; liContact < lrQueue.GetLength(); ++liContact)
            {
                PotentialContact& lrContact = const_cast<PotentialContact&>(lrQueue.GetEvent(liContact));

                CgsSceneManager::VolumeInstanceId lVolInstIdA = lrContact.muVolumeInstanceIdA;
                CgsSceneManager::VolumeInstanceId lVolInstIdB = lrContact.muVolumeInstanceIdB;
                if (GetVolumeInstanceOwner(lVolInstIdA) != 2u || GetVolumeInstanceOwner(lVolInstIdB) != 2u)
                {
                    continue;   // X360 @0x825A6760/@0x825A6774: skip, no assert
                }

                lVolInstIdA = SpliceEntityWord(lVolInstIdA,
                    mVehicleManager.GetPhysicsEntityIDFromGlobalEntityID(
                        CgsSceneManager::EntityId(static_cast<u32>(lVolInstIdA.muId >> 32))));
                lVolInstIdB = SpliceEntityWord(lVolInstIdB,
                    mVehicleManager.GetPhysicsEntityIDFromGlobalEntityID(
                        CgsSceneManager::EntityId(static_cast<u32>(lVolInstIdB.muId >> 32))));

                mDeformationManager.FixUpVehicleContact(lrContact, lVolInstIdA, lVolInstIdB);
            }
        }
    }

    namespace
    {
        // The vcmpeqfp-with-itself NaN test the X360 runs per lane (x/y/z; w ignored).
        // A NaN fails self-equality; everything else passes.
        inline bool IsVectorFinite3(const rw::math::vpu::Vector3& lrVec)
        {
            return lrVec.x == lrVec.x && lrVec.y == lrVec.y && lrVec.z == lrVec.z;
        }
    }

    // ==========================================================================================
    // PhysicsModule::Update @ 0x825B0640  (1,999 insns) -- THE CONDUCTOR.       2026-08-09
    //
    // Reconstructed from the BURNOUT_X360_ARTIST.XEX asm (every bl decoded from the image
    // bytes; the accessor identities were re-derived from each callee's own return-offset
    // `addi`, NOT from the truncated export names) with the PS3 DecFIGS out-of-line build
    // @0x69FD28 corroborating the call sequence and every callee signature by mangled name.
    //
    // Structure (console order, kept 1:1):
    //   * four argument asserts (:244..:247)
    //   * mContactData.Clear() (ten inlined miLength stores) + VehicleManager::CheckState
    //   * five CreateIOBuffer legs (sim in/out, VehManager, PotentialContacts,
    //     PropRaceCarContacts), lock + SetConstQueue, the two timer products,
    //     HandleGameActions, the camera latch
    //   * the NON-CATCHUP block ((lUpdateSet & 1) == 0): contact generation start/end,
    //     traction line tests, prop generation, racecar-world validation, traffic ordering,
    //     FixUpVehicleContacts, crash prediction, the super-slow-motion latch, UpdateDrivers,
    //     ProcessWheelContacts (empty as shipped), UpdateVehiclePhysics, UpdateVehicleEffects,
    //     part/prop generation end, the request bridge, SetTimeStep, sensor displacements,
    //     BridgeContactsToSimulation, ClearSnappedNetworkCarContacts, DeformationManager::
    //     Update, BridgeUpdatedVehiclesToSimulation, THE SIM STEP (mSimulationModule.Update),
    //     the three NaN-validation sweeps over the sim's spy queues, sim-input recreate,
    //     ReadUpdatedBodies, UpdateVehiclePhysicsPostSimulation, deformation post-physics +
    //     OutputData + ProcessDeformationStates, ProcessCrashingNetworkCars
    //   * the shared tail: WriteOutVehicleStats, ProcessResetEvents, prop read-back (catchup
    //     path skips it), BridgeVehicleManagerToSimulation_PostPhysics, ProcessInput (catchup)
    //     or the contact-spy leg (BridgeSimulationToOutput + ProcessContactSpies +
    //     UpdateFatalCrashFlags), BridgeVehicleManagerToOutput, DestroyIOBuffer x5.
    //
    // CATCHUP (lUpdateSet & 1): the console's network-catchup fast path -- locks the sim
    // output READ up front, skips the whole generation/step block, and calls the sim's
    // ProcessInput instead of the spy leg at the tail. Reconstructed as shipped even though
    // nothing sets the bit on PC yet.
    //
    // The streamed dev asserts (gpcMessageBuffer + StrStream hex ids) are LOWERED to
    // plain CGS_ASSERT with the console's message prefix -- the committed physics-TU
    // convention (see BrnPhysicsModuleBridgeFunctions.cpp:371).
    //
    // HONEST-CLOSURE NOTE: several callees below are LOUD one-shot inert gates this
    // wave (BrnPhysicsConductorGates.cpp names each with its X360 address + insn count).
    // The CALL SEQUENCE here is complete and faithful; the gated legs log once per boot
    // and do nothing until their bodies land. No call is dropped, no order changed.
    // ==========================================================================================
    void PhysicsModule::Update( CgsModule::IOBufferStack* lpInputBufferStack,
                                CgsModule::IOBufferStack* lpOutputBufferStack,
                                const PhysicsModuleIO::InputBuffer* lpPhysicsModuleInputBuffer,
                                PhysicsModuleIO::OutputBuffer* lpPhysicsModuleOutputBuffer,
                                BrnUpdateSet lUpdateSet )
    {
        CGS_ASSERT(lpInputBufferStack  != 0, "lpInputBufferStack != NULL");          // :244
        CGS_ASSERT(lpOutputBufferStack != 0, "lpOutputBufferStack != NULL");         // :245
        CGS_ASSERT(lpPhysicsModuleInputBuffer  != 0, "lpPhysicsModuleInputBuffer != NULL");   // :246
        CGS_ASSERT(lpPhysicsModuleOutputBuffer != 0, "lpPhysicsModuleOutputBuffer != NULL");  // :247

        const bool lbNetworkCatchup = (lUpdateSet & 1) != 0;   // r30 = lUpdateSet & 1

        // [pausebit] witness. NOT X360. Value-change only; pairs with the probe in
        // PropEntityModule::PostPhysicsUpdate. When bit 0 is set this function does NOT call
        // BridgeSimulationToOutput, so the output buffer's contact-spy interface keeps its
        // Construct-time NULL for the frame -- which is exactly why the prop module must skip
        // ProcessContacts on the same frame. The two probes must agree, always.
        {
            static u32 suLastSet = 0xFFFFFFFFu;
            const u32 luSet = static_cast<u32>(lUpdateSet);
            if (luSet != suLastSet && CgsDev::Log::gpDebugPrint != 0)
            {
                suLastSet = luSet;
                *CgsDev::Log::gpDebugPrint
                    << "[pausebit] PhysicsModule::Update updateSet="
                    << CgsDev::E_PRINTMODE_HEXONCE << luSet
                    << " bit0=" << static_cast<s32>(luSet & 1)
                    << " -> the bit alone would "
                    << (lbNetworkCatchup ? "SKIP" : "REACH")
                    << " BridgeSimulationToOutput\n";
            }
        }

        // ⭐⭐⭐ THE PC SIM-TIMER GUARD IS **DELETED** (pauseresume wave, 2026-08-27), and this
        // banner is its obituary because deleting a guard silently is how the next wave puts it
        // back. What stood here from 2026-08-09 was:
        //     lbSimTimerRunning = GetTimerInterface()->GetSimTimerStatus()->IsRunning();
        //     if (!lbSimTimerRunning) { log once; return; }        // "[FLAG PC boot guard]"
        // justified as: "the world spine reaches this function during BOOT frames too -- states in
        // which the SIM TIMER has never started, so BOTH TIMER PRODUCTS ARE 0.0 ... a conducted
        // frame with timestep 0 fires `mfTimeStep > 0.0f` EVERY FRAME (measured: 934 assert
        // dialogs in one 275s boot)", with the DELETE-WHEN "when the boot flow stops driving world
        // updates through dead timers".
        //
        // ⛔ IT HAD NO CONSOLE COUNTERPART. X360 PhysicsModule::Update @0x825B0640 is 1999
        // instructions and reaches the timer block twice, both through
        // InputBuffer::GetTimerInterface @0x8259FC90 (which returns `a1 + 327152` -- the block is
        // EMBEDDED, not a pointer):
        //     0x825B0860  addi r21, r3, 0x18     ; r21 = the SIM TimerStatus (+24)
        //     0x825B0864  lfs  f0,  8(r21)       ; mfTimeStepMultiplier
        //     0x825B086C  lfs  f13, 4(r21)       ; mfBaseTimeStep
        //     0x825B0870  fmuls f31, f0, f13     ; THE SIM TIMESTEP  (the pair read below)
        //     0x825B0878..88                     ; the same two loads off the GAME block (+0)
        // EXHAUSTIVE, not anecdotal: every use of r21 across its whole live range (0x825B0860 to
        // its reload at 0x825B0DC4) is +4, +8, +0x10, +0x14 -- **+0xC (`mbRunning`) is never
        // touched**, and the only `lbz` in the entire function (0x825B0C98, `0x713(r11)`) is
        // nowhere near the timer block. The console reads the timer's PRODUCTS and never its
        // RUNNING FLAG. Update-set bit 0 is the sole gate, and it gates the three sites this file
        // already reproduces (contact gen + UpdateVehiclePhysics, the prop read-back, and
        // BridgeSimulationToOutput -- the sole binder of the contact-spy interface).
        //
        // ⛔⛔ AND IT BROKE THE RESUME, because it made a STALE MIRROR load-bearing. The timer
        // status this function sees is a SNAPSHOT (StoreTimers -> BridgeTimers -> the world input
        // -> SetTimerInterface's 48-byte copy), published one frame before the CheckGameActions
        // that flips mSimTimer's running flag. The console's snapshot is stale by the same one
        // frame -- DoUpdate @0x823F0AF8 runs BridgeTimers @0x823F0DE4 BEFORE
        // DoUpdate_GameStatePreWorld @0x823F10CC (which is where CheckGameActions lives) and
        // DoUpdate_World @0x823F14B4 after both -- so the staleness is FAITHFUL; READING it was
        // not. On the resume frame ConstructUpdateSetFromFsm reads mbSimPaused LIVE (bit 0 clears
        // at once) while the snapshot still said stopped, so this guard early-returned,
        // BridgeSimulationToOutput never ran, and PropEntityModule::ProcessContacts -- correctly
        // ungated by the same clear bit -- found the contact-spy interface still NULL and fired
        // `mpData != NULL`. That is a THIRD state the console never has: bit 0 clear but physics
        // inert. INVENTED-ARM class.
        //
        // ⭐⭐ WHY DELETED OUTRIGHT RATHER THAN RE-PREDICATED ON THE TIMESTEP. Measured at this
        // exact site, in this exact branch (run pr_measure, a probe printing both predicates):
        //     line 1057  running=1 baseStep=0.016667 mult=1.000000 step=0.016667  -> conducted
        //     line 4387  running=0 baseStep=0.016667 mult=1.000000 step=0.016667  -> EARLY RETURN
        // (a) The two predicates DISAGREE: the guard's own justification was "both timer products
        //     are 0.0", but at the pause the timestep is a perfectly good 1/60. It was firing on a
        //     state its banner said it did not cover.
        // (b) The one-shot line "sim timer not running -- inert this frame [FLAG PC boot guard]"
        //     printed at log line 4388 -- during the PAUSE, ~130 s in -- while its CONDUCTING twin
        //     printed at 1058, BEFORE it. **The boot guard never fired at boot.** The first time
        //     this function is ever reached the snapshot is already published and running, and in a
        //     whole 150 s session it early-returned exactly once: the pause. Its DELETE-WHEN was
        //     satisfied silently some time ago, so there was nothing left to re-predicate.
        // ⇒ A timestep-shaped replacement would have been a guard for a state measured
        //   unreachable -- an invention preserved by renaming. If a future boot path ever does
        //   reach here with a Construct-cleared (all-zero, never-published) timer block, the
        //   symptom is the sim module's own `mfTimeStep > 0.0f` assert storm, which is loud,
        //   attributable, and a truer report than a silent early return.

        // The ten per-frame contact-spy container clears (mContactData.Clear() inlined on
        // the console -- see BrnContactSpyData.cpp), then the first state sweep.
        mContactData.Clear();
        mVehicleManager.CheckState();

        // ---- the five per-frame IO buffers ---------------------------------------------------
        // CreateIOBuffer<T> runs each type's Construct after the alloc, exactly as the X360
        // instantiations do (addresses per line below) -- no hand Construct call here.
        CgsPhysics::PhysicsSimulationIO::InputBuffer*  lpSimInputBuffer  = 0;
        CgsPhysics::PhysicsSimulationIO::OutputBuffer* lpSimOutputBuffer = 0;
        Vehicle::VehicleManagerOutputBuffer*           lpVehManagerBuffer = 0;
        PhysicsModuleIO::PotentialContactInterface*    lpPotentialContacts = 0;
        Props::PropRaceCarContactBuffer*               lpPropRaceCarContacts = 0;

        lpInputBufferStack->CreateIOBuffer(&lpSimInputBuffer, "Simulation");            // @0x8259D940
        lpOutputBufferStack->CreateIOBuffer(&lpSimOutputBuffer, "Simulation");          // @0x8259DCA0
        lpOutputBufferStack->CreateIOBuffer(&lpVehManagerBuffer, "VehManager");         // @0x8259DAF0
        lpOutputBufferStack->CreateIOBuffer(&lpPotentialContacts, "PotentialContacts"); // @0x825AC3C8
        lpOutputBufferStack->CreateIOBuffer(&lpPropRaceCarContacts, "PropRaceCarContacts"); // @0x825AC4A0

        // a const VIEW of the sim output buffer, nothing more.
        // The post-step read-back legs run with that buffer READ-locked, and the console reads
        // its update-rigid-body queue there through the CONST accessor @0x8259EFD0. Naming the
        // const view once is how those two call sites select that overload; it aliases the same
        // object and changes no lifetime, no lock and no order.
        const CgsPhysics::PhysicsSimulationIO::OutputBuffer* const lpConstSimOutputBuffer =
            lpSimOutputBuffer;

        lpPhysicsModuleInputBuffer->LockForRead();
        lpPhysicsModuleOutputBuffer->LockForWrite();

        // Seat the scene's merged potential-contact queue on the interface.
        lpPotentialContacts->LockForWrite();
        lpPotentialContacts->SetConstQueue(lpPhysicsModuleInputBuffer->GetPotentialContactQueue());
        lpPotentialContacts->UnlockForWrite();

        // ---- the two timer products (lfs +32 * +28 == sim; lfs +8 * +4 == game) --------------
        const f32 lfSimTimerTimeStepBase =
            lpPhysicsModuleInputBuffer->GetTimerInterface()->GetSimTimerStatus()->GetCurrentTimeStep();
        f32 lfSimTimerTimeStep  = lfSimTimerTimeStepBase;
        const f32 lfGameTimerTimeStep =
            lpPhysicsModuleInputBuffer->GetTimerInterface()->GetGameTimerStatus()->GetCurrentTimeStep();

        // [dv] open the one-step velocity witness (opt-in; no-ops otherwise). Nothing between
        // here and the first mark touches a body velocity.
        DvWitnessBeginStep();

        // FLAG: gate-bodied this wave (BrnPhysicsConductorGates.cpp) -- the dispatch's own
        // ~10-method web is not reconstructed. The call and its arguments are the console's.
        HandleGameActions(lpPhysicsModuleInputBuffer->GetGameActionQueue(), lpPhysicsModuleOutputBuffer);

        // The camera latch: four lvx/stvx pairs, input camera block -> mVehicleManager's
        // mCameraMatrix (VehicleManager::UpdateCameraMatrix inlined).
        mVehicleManager.UpdateCameraMatrix(
            reinterpret_cast<const Matrix44Affine*>(lpPhysicsModuleInputBuffer->GetCameraInput()));

        lpVehManagerBuffer->LockForWrite();

        if ( !lbNetworkCatchup )
        {
            mVehicleManager.CheckState();
            lpPotentialContacts->LockForWrite();

            CgsDev::PerfMonCpu::StartMonitor(miPhysicsUpdateContactGenAsyncPM);          // +433140
            CgsDev::PerfMonCpu::StartMonitor(miPhysicsUpdateDoVehicleContactGenStartPM); // +433144

            mDeformationManager.VerifyPartIndices();

            // "Vehicle prim alloc": a 1 MB linear arena on the input stack.
            CgsMemory::IOStackLinearMalloc<1048576>* lpVehiclePrimAlloc = 0;
            lpInputBufferStack->CreateIOBuffer(&lpVehiclePrimAlloc, "Vehicle prim alloc"); // @0x825A36E0
            lpVehiclePrimAlloc->Prepare();                        // LinearMalloc::Create(+32, 1 MB)
            lpVehiclePrimAlloc->GetMalloc()->SetAlignment(16);

            mVehicleManager.StartVehicleContactGeneration(
                lpPhysicsModuleInputBuffer->GetVehicleInputInterface()->GetTriangleCacheInterface(),
                lpPhysicsModuleInputBuffer->GetOverlapPairsQueue(),
                lfSimTimerTimeStep,
                &mDeformationManager,
                lpInputBufferStack,
                lpVehiclePrimAlloc->GetMalloc(),
                lpPotentialContacts);
            CgsDev::PerfMonCpu::StopMonitor(miPhysicsUpdateDoVehicleContactGenStartPM);

            mVehicleManager.StartVehicleTractionLineTests(
                lpInputBufferStack,
                lpPhysicsModuleInputBuffer->GetVehicleInputInterface(),
                &mDeformationManager,
                lfSimTimerTimeStep);

            CgsDev::PerfMonCpu::StartMonitor(miPhysicsUpdateDoPartContactGenStartPM);    // +433152
            mVehicleManager.StartPartContactGeneration(
                lpPhysicsModuleInputBuffer->GetVehicleInputInterface()->GetTriangleCacheInterface(),
                lfSimTimerTimeStep,
                &mDeformationManager,
                lpInputBufferStack,
                lpPotentialContacts,
                lpVehiclePrimAlloc->GetMalloc());
            mDeformationManager.VerifyPartIndices();
            CgsDev::PerfMonCpu::StopMonitor(miPhysicsUpdateDoPartContactGenStartPM);

            // ---- prop-world generation begin -------------------------------------------------
            CgsDev::PerfMonCpu::StartMonitor(miPhysicsUpdateDoPropContactGenStartPM);    // +433160
            CgsSceneManager::CgsCollision::CollisionGenerator* lpPropCollisionGenerator = 0;
            lpInputBufferStack->CreateIOBuffer(&lpPropCollisionGenerator,
                                               "Prop-world contact generator");          // @0x8259DD78
            CgsMemory::IOStackLinearMalloc<1048576>* lpPropLinearAlloc = 0;
            lpInputBufferStack->CreateIOBuffer(&lpPropLinearAlloc, "Prop linear alloc"); // @0x825A36E0
            lpPropLinearAlloc->Prepare();
            lpPropLinearAlloc->GetMalloc()->SetAlignment(16);

            mPropManager.BeginPropWorldContactGeneration(
                lpPhysicsModuleInputBuffer->GetVehicleInputInterface()->GetTriangleCacheInterface(),
                lpPropCollisionGenerator,
                lpPropLinearAlloc->GetMalloc(),
                VecFloat{ lfSimTimerTimeStep, lfSimTimerTimeStep, lfSimTimerTimeStep, lfSimTimerTimeStep });
            CgsDev::PerfMonCpu::StopMonitor(miPhysicsUpdateDoPropContactGenStartPM);

            // ---- vehicle generation end (the async harvest) ----------------------------------
            CgsDev::PerfMonCpu::StartMonitor(miPhysicsUpdateDoVehicleContactGenEndPM);   // +433148
            mVehicleManager.EndVehicleContactGeneration(
                lpPhysicsModuleInputBuffer->GetVehicleInputInterface()->GetTriangleCacheInterface(),
                lpPhysicsModuleInputBuffer->GetOverlapPairsQueue(),
                lfSimTimerTimeStep,
                &mDeformationManager,
                lpInputBufferStack,
                lpVehiclePrimAlloc->GetMalloc(),
                lpPotentialContacts);
            CgsDev::PerfMonCpu::StopMonitor(miPhysicsUpdateDoVehicleContactGenEndPM);

            CgsDev::PerfMonCpu::StartMonitor(miPhysicsUpdateValidateRaceCarWorldContactPM); // +433168
            mVehicleManager.DoRaceCarWorldContactValidation(
                lpPotentialContacts,
                lpPhysicsModuleInputBuffer->GetVehicleInputInterface()->GetTriangleCacheInterface(),
                lfSimTimerTimeStep,
                &mDeformationManager);
            CgsDev::PerfMonCpu::StopMonitor(miPhysicsUpdateValidateRaceCarWorldContactPM);

            DvWitnessMark("contactgen");   // [dv] after harvest + ValidateRaceCarWorldContact

            mVehicleManager.DoTrafficWorldContactOrdering(lpPotentialContacts);

            CgsDev::PerfMonCpu::StartMonitor(miPhysicsUpdateFixUpVehContactsPM);         // +433200
            FixUpVehicleContacts(lpPotentialContacts);
            CgsDev::PerfMonCpu::StopMonitor(miPhysicsUpdateFixUpVehContactsPM);

            lpPotentialContacts->UnlockForWrite();
            lpPotentialContacts->LockForRead();

            // ---- crash prediction ------------------------------------------------------------
            CgsDev::PerfMonCpu::StartMonitor(miPhysicsUpdateCrashPredictionPM);          // +433112
            mVehicleManager.DoCrashPrediction(
                lpInputBufferStack,
                lpOutputBufferStack,
                lfSimTimerTimeStep,
                lpPhysicsModuleInputBuffer->GetVehicleInputInterface(),
                lpPhysicsModuleOutputBuffer->GetVehicleOutputInterface(),
                lpVehManagerBuffer->GetVehicleOutputRequestInterface(),
                lpPhysicsModuleOutputBuffer->GetVehicleManagerOutputInterface(),
                &mDeformationInput,
                lpPotentialContacts);
            CgsDev::PerfMonCpu::StopMonitor(miPhysicsUpdateCrashPredictionPM);
            mVehicleManager.CheckState();

            CgsDev::PerfMonCpu::StartMonitor(miPhysicsUpdateVehiclePhysicsPM);           // +433108

            // ---- the super-slow-motion latch (asm 0x825B0C44..0x825B0D00) --------------------
            // Slot 0's handling body -> its race car; a crash this frame arms three frames of
            // 0.001x time scale unless the inhibit latch is set; the latch always resets.
            {
                Vehicle::RaceCarPhysics* lpSlowMoCar =
                    mVehicleManager.GetRaceCarPhysics(mVehicleManager.GetRigidBodyId(0));
                if (lpSlowMoCar != 0 && lpSlowMoCar->HasCrashedThisFrame() &&
                    !mVehicleManager.GetForceNoSlowMo())
                {
                    miFramesToForceSuperSlowMotion = 3;
                }
                if (miFramesToForceSuperSlowMotion > 0)
                {
                    miFramesToForceSuperSlowMotion -= 1;
                    lfSimTimerTimeStep = lfSimTimerTimeStep * 0.001f;   // flt_82013F90
                }
                mVehicleManager.ResetForceNoSlowMo();
            }

            // FLAG: gate-bodied. Console: the per-driver control dispatch.
            // FLAG (storage->real seam cast, deliberate -- the driver-input span is still a
            // size-pinned opaque storage; retire when it adopts the real type):
            mVehicleManager.UpdateDrivers(
                lfSimTimerTimeStep,
                reinterpret_cast<const Vehicle::VehicleDriverInputInterface*>(
                    lpPhysicsModuleInputBuffer->GetVehicleDriverInterface()),
                lpVehManagerBuffer->GetVehicleOutputRequestInterface(),
                lpPhysicsModuleOutputBuffer->GetVehicleManagerOutputInterface(),
                &mDeformationInput,
                lpPhysicsModuleOutputBuffer->GetVehicleOutputInterface());

            DvWitnessMark("drivers");   // [dv] after crash prediction + UpdateDrivers

            // EMPTY AS SHIPPED (the retail body is one `blr`, ICF-folded with
            // BaseCollisionGenerator::Destruct -- see the declaration's banner).
            mVehicleManager.ProcessWheelContacts(lfSimTimerTimeStep, lpPotentialContacts);
            mVehicleManager.CheckState();

            // ---- THE FORCE PRODUCER ----------------------------------------------------------
            CgsSystem::Time lCurrentTime =
                lpPhysicsModuleInputBuffer->GetTimerInterface()->GetSimTimerStatus()->GetTime();
            mVehicleManager.UpdateVehiclePhysics(
                lpInputBufferStack,
                lpOutputBufferStack,
                lUpdateSet,
                lCurrentTime,
                lfSimTimerTimeStep,
                lfGameTimerTimeStep,
                lpPhysicsModuleInputBuffer->GetVehicleInputInterface(),
                lpPhysicsModuleOutputBuffer->GetVehicleOutputInterface(),
                lpVehManagerBuffer->GetVehicleOutputRequestInterface(),
                lpPhysicsModuleOutputBuffer->GetVehicleManagerOutputInterface(),
                &mDeformationInput,
                mbIsOnlineGameMode,
                mWorldEntityId);
            mVehicleManager.CheckState();

            DvWitnessMark("vehphys");   // [dv] after UpdateVehiclePhysics -- THE FORCE PRODUCER

            mVehicleManager.UpdateVehicleEffects(
                reinterpret_cast<const Vehicle::VehicleEffectsInputInterface*>(
                    lpPhysicsModuleInputBuffer->GetVehicleEffectsInputInterface()));  // FLAG: storage->real seam cast
            CgsDev::PerfMonCpu::StopMonitor(miPhysicsUpdateVehiclePhysicsPM);

            lpVehManagerBuffer->UnlockForWrite();
            lpPotentialContacts->UnlockForRead();
            lpPotentialContacts->LockForWrite();

            CgsDev::PerfMonCpu::StartMonitor(miPhysicsUpdateDoPartContactGenEndPM);      // +433156
            mVehicleManager.EndPartContactGeneration(lfSimTimerTimeStep, &mDeformationManager,
                                                     lpInputBufferStack, lpPotentialContacts);
            CgsDev::PerfMonCpu::StopMonitor(miPhysicsUpdateDoPartContactGenEndPM);

            CgsDev::PerfMonCpu::StartMonitor(miPhysicsUpdateDoPropContactGenEndPM);      // +433164
            mPropManager.EndPropWorldContactGeneration(lpPotentialContacts,
                                                       lpPropCollisionGenerator, mWorldEntityId);
            CgsDev::PerfMonCpu::StopMonitor(miPhysicsUpdateDoPropContactGenEndPM);
            CgsDev::PerfMonCpu::StopMonitor(miPhysicsUpdateContactGenAsyncPM);

            lpInputBufferStack->DestroyIOBuffer(&lpPropLinearAlloc);
            lpInputBufferStack->DestroyIOBuffer(&lpPropCollisionGenerator);
            mVehicleManager.FreeAllocations(lpInputBufferStack);
            lpInputBufferStack->DestroyIOBuffer(&lpVehiclePrimAlloc);

            lpPotentialContacts->UnlockForWrite();
            lpPotentialContacts->LockForRead();

            // ---- bridge the vehicle manager's requests into the sim --------------------------
            CGS_ASSERT(lpSimInputBuffer != 0, "lpInputBuffer");                          // CgsModuleUtils.h:238
            lpSimInputBuffer->LockForWrite();
            CGS_ASSERT(lpPropRaceCarContacts != 0, "lpInputBuffer");                     // CgsModuleUtils.h:238
            lpPropRaceCarContacts->LockForWrite();
            lpVehManagerBuffer->LockForRead();
            // this used to read `lpVehManagerBuffer->
            // GetVehicleOutputRequestInterface()` directly, and because lpVehManagerBuffer is a
            // NON-const pointer, C++ overload resolution picked the MUTABLE accessor -- whose
            // tripwire is IsBufferLockedForWriting(). The buffer is READ-locked on this leg, so
            // it fired "Not locked for writing" (BrnVehicleManagerIO.cpp:60) on every frame the
            // physics module actually ran. The console calls the CONST accessor @0x825A0FB0
            // here. Selecting it explicitly through a const view is the entire fix: no lock
            // invented, no lock moved, no behaviour changed.
            {
                const Vehicle::VehicleManagerOutputBuffer* const lpConstVehManagerBuffer =
                    lpVehManagerBuffer;
                BridgeVehicleManagerRequestsToSimulation(
                    lpSimInputBuffer,
                    lpConstVehManagerBuffer->GetVehicleOutputRequestInterface());
            }
            lpVehManagerBuffer->UnlockForRead();

            lpSimInputBuffer->SetTimeStep(lfSimTimerTimeStep);
            mDeformationManager.VerifyPartIndices();

            CgsDev::PerfMonCpu::StartMonitor(miDeformationManagerPM);                    // +433132
            mDeformationManager.UpdateSensorDisplacements(
                VecFloat{ lfSimTimerTimeStep, lfSimTimerTimeStep, lfSimTimerTimeStep, lfSimTimerTimeStep });
            CgsDev::PerfMonCpu::StopMonitor(miDeformationManagerPM);
            mDeformationManager.VerifyPartIndices();

            CgsDev::PerfMonCpu::StartMonitor(miPhysicsBridgeContactsPM);                 // +433136
            BridgeContactsToSimulation(lpSimInputBuffer, lpPhysicsModuleInputBuffer,
                                       lpPotentialContacts, lpPropRaceCarContacts);
            CgsDev::PerfMonCpu::StopMonitor(miPhysicsBridgeContactsPM);

            mVehicleManager.ClearSnappedNetworkCarContacts(&mDeformationManager);

            CGS_ASSERT(lpPropRaceCarContacts != 0, "lpInputBuffer");                     // CgsModuleUtils.h:248
            lpPropRaceCarContacts->UnlockForWrite();
            CGS_ASSERT(lpSimInputBuffer != 0, "lpInputBuffer");                          // CgsModuleUtils.h:248
            lpSimInputBuffer->UnlockForWrite();
            lpPotentialContacts->UnlockForRead();
            mVehicleManager.CheckState();

            // ---- deformation step ------------------------------------------------------------
            lpSimInputBuffer->LockForWrite();
            lpSimOutputBuffer->LockForWrite();
            lpPotentialContacts->LockForRead();
            CgsDev::PerfMonCpu::StartMonitor(miPhysicsProcessRaceCarContactsPM);         // +433128
            mDeformationManager.Update(lpSimInputBuffer, lpSimOutputBuffer,
                                       lpPhysicsModuleInputBuffer, lpPhysicsModuleOutputBuffer,
                                       lpPotentialContacts,
                                       VecFloat{ lfSimTimerTimeStep, lfSimTimerTimeStep,
                                                 lfSimTimerTimeStep, lfSimTimerTimeStep },
                                       meCurrentGameMode);
            CgsDev::PerfMonCpu::StopMonitor(miPhysicsProcessRaceCarContactsPM);
            DvWitnessMark("deform");   // [dv] after DeformationManager::Update (the [kerb-imp] arm)
            lpPotentialContacts->UnlockForRead();
            lpSimOutputBuffer->UnlockForWrite();

            BridgeUpdatedVehiclesToSimulation(lpSimInputBuffer, lpPhysicsModuleInputBuffer);
            lpSimInputBuffer->UnlockForWrite();

            // ---- THE SIMULATION STEP (console vtable slot 17 on mSimulationModule) -----------
            CgsDev::PerfMonCpu::StartMonitor(miPhysicsUpdateSimulationPM);               // +433100
            mSimulationModule.Update(lpInputBufferStack, lpOutputBufferStack,
                                     lpSimInputBuffer, lpSimOutputBuffer);
            CgsDev::PerfMonCpu::StopMonitor(miPhysicsUpdateSimulationPM);
            DvWitnessMark("sim");   // [dv] after the rw::physics step

            // ---- the three NaN-validation sweeps over the sim's spy queues -------------------
            lpSimOutputBuffer->LockForWrite();
            {
                typedef CgsPhysics::PhysicsSimulationIO::OutContactSpy OutContactSpy;
                CgsPhysics::PhysicsSimulationIO::OutputBuffer::OutContactSpyQueue* lpSpies =
                    lpSimOutputBuffer->GetContactSpyQueue();
                for (s32 liSpy = 0; liSpy < lpSpies->GetLength(); ++liSpy)
                {
                    const OutContactSpy& lrSpy = lpSpies->GetEvent(liSpy);
                    CGS_ASSERT(IsVectorFinite3(lrSpy.mNormal),
                               "Contact normal IDA/IDB not finite");                     // :632
                    CGS_ASSERT(IsVectorFinite3(lrSpy.mPointOnA),
                               "Contact point on A IDA/IDB not finite");                 // :633
                    CGS_ASSERT(IsVectorFinite3(lrSpy.mPointOnB),
                               "Contact point on B IDA/IDB not finite");                 // :634
                    CGS_ASSERT(IsVectorFinite3(lrSpy.mFrictionStress),
                               "Contact friction IDA/IDB not finite");                   // :635
                }

                typedef CgsPhysics::PhysicsSimulationIO::OutDriveSpy OutDriveSpy;
                CgsPhysics::PhysicsSimulationIO::OutputBuffer::OutDriveSpyQueue* lpDrives =
                    lpSimOutputBuffer->GetDriveSpyQueue();
                for (s32 liDrive = 0; liDrive < lpDrives->GetLength(); ++liDrive)
                {
                    const OutDriveSpy& lrDrive = lpDrives->GetEvent(liDrive);
                    CGS_ASSERT(lrDrive.mAngularDistanceToKey == lrDrive.mAngularDistanceToKey,
                               "Drive ang dist to key IDA not finite");                  // :644
                    CGS_ASSERT(IsVectorFinite3(lrDrive.mAngularStress),
                               "Drive ang stress IDA not finite");                       // :645
                    CGS_ASSERT(IsVectorFinite3(lrDrive.mLinearStress),
                               "Drive lin stress IDA not finite");                       // :646
                    CGS_ASSERT(lrDrive.mLinearDistanceToKey == lrDrive.mLinearDistanceToKey,
                               "Drive lin dist to key IDA not finite");                  // :647
                }

                typedef CgsPhysics::PhysicsSimulationIO::OutJointSpy OutJointSpy;
                CgsPhysics::PhysicsSimulationIO::OutputBuffer::OutJointSpyQueue* lpJoints =
                    lpSimOutputBuffer->GetJointSpyQueue();
                for (s32 liJoint = 0; liJoint < lpJoints->GetLength(); ++liJoint)
                {
                    const OutJointSpy& lrJoint = lpJoints->GetEvent(liJoint);
                    CGS_ASSERT(IsVectorFinite3(lrJoint.mAngularStress),
                               "Joint ang stress IDA not finite");                       // :657
                    CGS_ASSERT(IsVectorFinite3(lrJoint.mLinearStress),
                               "Joint lin stress IDA not finite");                       // :658
                }
            }
            lpSimOutputBuffer->UnlockForWrite();

            // The console destroys and immediately recreates the sim INPUT buffer here --
            // a bulk clear of every input queue for the post-step legs. Reproduced 1:1.
            lpInputBufferStack->DestroyIOBuffer(&lpSimInputBuffer);
            lpInputBufferStack->CreateIOBuffer(&lpSimInputBuffer, "Simulation");

            lpSimOutputBuffer->LockForRead();
            lpVehManagerBuffer->LockForWrite();
            mVehicleManager.CheckState();

            // ---- read the stepped bodies back ------------------------------------------------
            CgsDev::PerfMonCpu::StartMonitor(miPhysicsUpdateReadUpdatedBodiesPM);        // +433104
            // the sim output buffer is READ-locked here
            // (LockForRead above), so the mutable GetUpdateRigidBodyQueue() this used to select
            // fired "Not locked for writing" every frame. The console calls the CONST twin
            // @0x8259EFD0 at exactly this site; it is declared now, and both ReadUpdatedBodies
            // consumers already take a const queue pointer.
            mVehicleManager.ReadUpdatedBodies(
                lpConstSimOutputBuffer->GetUpdateRigidBodyQueue(),
                VecFloat{ lfSimTimerTimeStep, lfSimTimerTimeStep, lfSimTimerTimeStep, lfSimTimerTimeStep });
            CgsDev::PerfMonCpu::StopMonitor(miPhysicsUpdateReadUpdatedBodiesPM);
            // [dv] after gravity + IntegrateTransform -- the ONLY place a car's pose advances.
            DvWitnessMark("integrate");

            mDeformationManager.VerifyPartIndices();

            mVehicleManager.UpdateVehiclePhysicsPostSimulation(
                lpPhysicsModuleInputBuffer->GetVehicleInputInterface(),
                lpSimOutputBuffer,
                lfSimTimerTimeStep,
                reinterpret_cast<BrnGameState::GameStateModuleIO::GameEventQueue*>(
                    lpPhysicsModuleOutputBuffer->GetVehicleOutputInterface()->GetGameEventQueue()));
            mVehicleManager.CheckState();
            // [dv] after UpdateSuspensionPostSimulation -- the bump-stop body translation and the
            // -0.7-restitution inanimate-world recovery impulses live in there.
            DvWitnessMark("postsim");

            CgsDev::PerfMonCpu::StartMonitor(miDeformationManagerPM);                    // +433132 (v518)
            mDeformationManager.UpdatePostPhysics(lpSimOutputBuffer, lpPhysicsModuleOutputBuffer,
                                                  &mContactData, lpInputBufferStack,
                                                  lpPotentialContacts);
            CgsDev::PerfMonCpu::StopMonitor(miDeformationManagerPM);
            mDeformationManager.VerifyPartIndices();
            mDeformationManager.VerifyPartIndices();

            // ⭐ SEAMS RETIRED 2026-08-24 (deform-land wave, P1(b)): the output buffer's two
            // deformation seats hold their real types, so the storage->real reinterpret_casts
            // this block carried are gone.
            mDeformationManager.OutputData(
                lpPhysicsModuleOutputBuffer->GetDeformationOutputInterfaceForEntityModules(),
                lpPhysicsModuleOutputBuffer->GetDeformationOutputInterface());
            mDeformationManager.VerifyPartIndices();

            mVehicleManager.ProcessDeformationStates(
                lpPhysicsModuleOutputBuffer->GetDeformationOutputInterface());
            mDeformationManager.VerifyPartIndices();

            // FLAG: gate-bodied (image-only address 0x8263C7C0).
            mVehicleManager.ProcessCrashingNetworkCars(
                reinterpret_cast<const Vehicle::VehicleDriverInputInterface*>(
                    lpPhysicsModuleInputBuffer->GetVehicleDriverInterface()),  // FLAG: storage->real seam cast
                lpVehManagerBuffer->GetVehicleOutputRequestInterface(),
                lpPhysicsModuleOutputBuffer->GetVehicleManagerOutputInterface(),
                &mDeformationInput,
                lpPhysicsModuleOutputBuffer->GetVehicleOutputInterface());
            mVehicleManager.CheckState();

            // [dv] close the step. The frame number is the [kerb]/[kerb-car] counter so a [dv]
            // dump and the contact lines for the same step share an index.
            DvWitnessEndStep(lfSimTimerTimeStep, Vehicle::guKerbProbeFrame);
        }
        else
        {
            // CATCHUP: the sim output is read-locked for the shared tail; nothing else runs.
            lpSimOutputBuffer->LockForRead();
        }

        // ==== the shared tail (both paths) ====================================================
        mVehicleManager.WriteOutVehicleStats(lpPhysicsModuleOutputBuffer->GetVehicleOutputInterface());

        mVehicleManager.ProcessResetEvents(
            lpPhysicsModuleInputBuffer->GetVehicleInputInterface(),
            lpVehManagerBuffer->GetVehicleOutputRequestInterface(),
            lpPhysicsModuleOutputBuffer->GetVehicleManagerOutputInterface(),
            &mDeformationInput);
        mVehicleManager.CheckState();
        lpVehManagerBuffer->UnlockForWrite();

        CGS_ASSERT(lpSimInputBuffer != 0, "lpInputBuffer");                              // CgsModuleUtils.h:238
        lpSimInputBuffer->LockForWrite();

        if ( !lbNetworkCatchup )
        {
            // ---- prop read-back --------------------------------------------------------------
            CgsDev::PerfMonCpu::StartMonitor(miPropManagerPM);                           // +433172
            // (same const-twin selection as the vehicle read-back above -- the sim output
            //  buffer is still read-locked on this leg.)
            mPropManager.ReadUpdatedBodies(
                lpConstSimOutputBuffer->GetUpdateRigidBodyQueue(),
                reinterpret_cast<CgsSceneManager::SceneManagerIO::InSceneUpdateInterface*>(
                    lpPhysicsModuleOutputBuffer->GetSceneInputInterface()),
                lpSimInputBuffer,
                VecFloat{ lfSimTimerTimeStep, lfSimTimerTimeStep, lfSimTimerTimeStep, lfSimTimerTimeStep });
            mPropManager.OutputUpdatedProps(lpPhysicsModuleOutputBuffer);
            CgsDev::PerfMonCpu::StopMonitor(miPropManagerPM);
        }

        CgsDev::PerfMonCpu::StartMonitor(miDeformationMaintenancePM);                    // +433116
        mVehicleManager.CheckState();
        CgsDev::PerfMonCpu::StopMonitor(miDeformationMaintenancePM);

        lpSimOutputBuffer->UnlockForRead();
        lpPhysicsModuleOutputBuffer->UnlockForWrite();
        mVehicleManager.CheckState();

        lpVehManagerBuffer->LockForRead();
        BridgeVehicleManagerToSimulation_PostPhysics(lpSimInputBuffer, lpVehManagerBuffer);
        lpVehManagerBuffer->UnlockForRead();

        CGS_ASSERT(lpSimInputBuffer != 0, "lpInputBuffer");                              // CgsModuleUtils.h:248
        lpSimInputBuffer->UnlockForWrite();

        // ---- the drain-only sim entry (console vtable slot 18) -------------------------------
        mSimulationModule.ProcessInput(lpSimInputBuffer);

        if ( lbNetworkCatchup )
        {
            lpPhysicsModuleInputBuffer->UnlockForRead();
        }
        else
        {
            // ---- the contact-spy leg ---------------------------------------------------------
            lpSimInputBuffer->LockForRead();
            mVehicleManager.CheckState();
            mVehicleManager.ReadUpdatedBodyProperties(
                lpSimInputBuffer->GetChangeRigidBodyInertiaQueue());
            mVehicleManager.CheckState();
            lpSimInputBuffer->UnlockForRead();
            lpPhysicsModuleInputBuffer->UnlockForRead();

            CgsDev::PerfMonCpu::StartMonitor(miContactSpyListGenerationPM);              // +433120
            CgsModule::LockBuffersForIO(lpPhysicsModuleOutputBuffer,
                                        lpPhysicsModuleInputBuffer, lpSimOutputBuffer);  // @0x823B70E0
            lpPotentialContacts->LockForRead();
            lpPropRaceCarContacts->LockForRead();
            BridgeSimulationToOutput(lpPhysicsModuleOutputBuffer, lpPotentialContacts,
                                     lpPropRaceCarContacts, lpSimOutputBuffer,
                                     VecFloat{ lfSimTimerTimeStep, lfSimTimerTimeStep,
                                               lfSimTimerTimeStep, lfSimTimerTimeStep });
            lpPropRaceCarContacts->UnlockForRead();
            lpPotentialContacts->UnlockForRead();
            CgsModule::UnlockBuffersForIO(lpPhysicsModuleOutputBuffer,
                                          lpPhysicsModuleInputBuffer, lpSimOutputBuffer); // @0x823B7190

            lpVehManagerBuffer->LockForWrite();
            lpPhysicsModuleOutputBuffer->LockForWrite();
            mVehicleManager.ProcessContactSpies(
                &mContactData,
                lpVehManagerBuffer->GetVehicleOutputRequestInterface(),
                lpPhysicsModuleOutputBuffer->GetVehicleOutputInterface(),
                lpPhysicsModuleOutputBuffer->GetVehicleManagerOutputInterface(),
                &mDeformationInput,
                &mDeformationManager,
                lfSimTimerTimeStep);
            // ---- [wave4-B] the tail of ProcessContactSpies, hoisted out of the gate ------------
            // VehicleManager::ProcessContactSpies @0x82646C98 is 118 instructions: the race-car
            // spy loop, ProcessShowtimeShunts @0x82629F20, then -- as its LAST call, 0x82646E5C --
            // PhysicalTrafficManager::DisposeOfNonCrashingTraffic @0x825EFB40 on `this + 44768`.
            // That last call is the ONLY producer of mUnusedPotentialTrafficQueue in the image
            // (xrefs_to on 0x825EFB40 is the single entry ProcessContactSpies), and without it
            // every E_TRAFFIC_TYPE_POTENTIAL collision proxy the wave-4 overlap route creates
            // keeps its physics slot for ever. ProcessContactSpies itself is still a
            // BRN_CONDUCTOR_GATE in BrnPhysicsConductorGates.cpp (a file this wave must not
            // touch), so the call is seated HERE -- the identical frame position, immediately
            // after the gated ProcessContactSpies and before UpdateFatalCrashFlags, on the
            // non-catchup leg only, exactly as the console runs it.
            // DELETE-WHEN ProcessContactSpies gets a real body: the call moves inside it.
            mVehicleManager.GetPhysicalTrafficManager().DisposeOfNonCrashingTraffic();

            mVehicleManager.UpdateFatalCrashFlags(
                lpPhysicsModuleOutputBuffer->GetVehicleOutputInterface());
            lpVehManagerBuffer->UnlockForWrite();
            lpPhysicsModuleOutputBuffer->UnlockForWrite();
            CgsDev::PerfMonCpu::StopMonitor(miContactSpyListGenerationPM);
        }

        // ---- forward the request queues to the module output (the five appends) --------------
        CGS_ASSERT(lpPhysicsModuleOutputBuffer != 0, "lpInputBuffer");                   // CgsModuleUtils.h:259
        CGS_ASSERT(lpVehManagerBuffer != 0, "lpOutputBuffer0");                          // CgsModuleUtils.h:260
        BridgeVehicleManagerToOutput(lpPhysicsModuleOutputBuffer, lpVehManagerBuffer);
        mVehicleManager.CheckState();

        // ---- teardown (exact console LIFO order) ---------------------------------------------
        lpInputBufferStack->DestroyIOBuffer(&lpSimInputBuffer);
        lpOutputBufferStack->DestroyIOBuffer(&lpPropRaceCarContacts);
        lpOutputBufferStack->DestroyIOBuffer(&lpPotentialContacts);
        lpOutputBufferStack->DestroyIOBuffer(&lpVehManagerBuffer);
        lpOutputBufferStack->DestroyIOBuffer(&lpSimOutputBuffer);
        mVehicleManager.CheckState();
    }

    // =============================================================================================
    // PostSceneUpdate  @0x825ABC10  (278 insns; own asserts BrnPhysicsModuleUpdateFunctions.cpp
    // :68..:71). LANDED 2026-08-10 (create-path wave) -- the WorldLinkStubs boot gate that stood
    // at WorldLinkStubs.cpp:3529 since 2026-07-27 is DELETED.
    //
    // WHY THIS FUNCTION AND NOT THE CREATE PATH. The campaign brief named
    // VehicleManager::ProcessCreateEvents @0x82616770 as the head of the list. It is the only
    // writer in the XEX that SETS a bit in mUsedRaceCars, so that is right about the destination --
    // but `xrefs_to` on it is a ONE-element set (ProcessVehicleMaintenanceEvents), and `xrefs_to`
    // on THAT is a one-element set: this function, which was a boot gate. The create body had no
    // caller. Reachability first.
    //
    // Two consequences of this function going live are worth stating where the code is:
    //   1. mVehicleManager.SetPlayerActiveRaceCarIndex STARTS BEING CALLED. Until today the
    //      physics vehicle manager's mePlayerActiveRaceCarIndex was whatever Construct left it
    //      (-1) forever, and the mounted UpdateVehiclePhysics indexes maRaceCarDrivers with it
    //      UNGUARDED (`maRaceCarDrivers[mePlayerActiveRaceCarIndex].meDriverType`). This leg is
    //      what the console uses to make that index valid.
    //   2. mSimulationModule.ProcessInput runs here, on a Simulation buffer created and destroyed
    //      inside this call. Its add-rigid-body queue is EMPTY, because the only thing that fills
    //      it is BridgeVehicleManagerToSimulation_PostScene below -- deliberately still a gate.
    //
    // Console structure, statement for statement (asm 0x825ABC10..0x825AC060):
    //   StartMonitor(miPhysicsPreSceneUpdatePM)   [+433096, `addis r16,r30,7 ; addi -0x6438`]
    //   4 null asserts, CheckState
    //   CreateIOBuffer "Simulation" (input stack) + "VehManager" (output stack)
    //   LockForWrite(out) / LockForRead(in) / LockForWrite(vehManager)
    //   ProcessVehicleMaintenanceEvents(<the four interfaces + both stacks + mDeformationInput>)
    //   LockForWrite(sim)
    //   HandleGameActionsPostScene
    //   StartMonitor(miDeformationManagerPM)      [+433132, `addi -0x6414`]
    //     VerifyPartIndices / DeformationManager::PostSceneUpdate / VerifyPartIndices
    //   StopMonitor ; UnlockForWrite(vehManager)
    //   StartMonitor(miPropManagerPM)             [+433172, `addi -0x63EC`]
    //     PropManager::ProcessInputsPreScene(..., lUpdateSet & 1, sim)
    //   StopMonitor ; UnlockForWrite(out) ; LockForRead(out) ; LockForRead(vehManager)
    //   BridgeVehicleManagerToSimulation_PostScene ; UnlockForRead x2 ; UnlockForWrite(sim)
    //   the player-index / player-model-index arm
    //   UnlockForRead(in) ; mSimulationModule.ProcessInput(sim) ; DestroyIOBuffer x2
    //   CheckState ; StopMonitor
    //
    // Every `this`-relative operand above was resolved to a NAMED member before it was written:
    // 0x4AA0 -> mVehicleManager, `addis 5/-0x3460` -> mDeformationManager, `addis 6/-0x890` ->
    // mDeformationInput, `addis 6/+0x3630` -> mPropManager, 0x230 -> mSimulationModule, and the
    // three perf handles to their names in BrnPhysicsModule.h's 27-handle run. No console byte
    // offset survives into the code.
    // =============================================================================================
    void PhysicsModule::PostSceneUpdate( CgsModule::IOBufferStack* lpInputBufferStack,
                                         CgsModule::IOBufferStack* lpOutputBufferStack,
                                         const PhysicsModuleIO::InputBuffer* lpPhysicsModuleInputBuffer,
                                         PhysicsModuleIO::OutputBuffer* lpPhysicsModuleOutputBuffer,
                                         BrnUpdateSet lUpdateSet )
    {
        CgsDev::PerfMonCpu::StartMonitor(miPhysicsPreSceneUpdatePM);                     // +433096

        CGS_ASSERT(lpInputBufferStack  != 0, "lpInputBufferStack != NULL");              // :68
        CGS_ASSERT(lpOutputBufferStack != 0, "lpOutputBufferStack != NULL");             // :69
        CGS_ASSERT(lpPhysicsModuleInputBuffer  != 0, "lpInput != NULL");                 // :70
        CGS_ASSERT(lpPhysicsModuleOutputBuffer != 0, "lpOutput != NULL");                // :71

        mVehicleManager.CheckState();

        // ---- the two per-frame IO buffers ----------------------------------------------------
        // Same note as Update above: CreateIOBuffer<T> runs T::Construct after the alloc.
        CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInputBuffer   = 0;
        Vehicle::VehicleManagerOutputBuffer*          lpVehManagerBuffer = 0;

        lpInputBufferStack->CreateIOBuffer(&lpSimInputBuffer, "Simulation");             // @0x8259D940
        lpOutputBufferStack->CreateIOBuffer(&lpVehManagerBuffer, "VehManager");          // @0x8259DAF0

        lpPhysicsModuleOutputBuffer->LockForWrite();
        lpPhysicsModuleInputBuffer->LockForRead();
        lpVehManagerBuffer->LockForWrite();

        // The four interface handles, fetched in the console's own evaluation order (each is a
        // lock-tripwire call, so the order is behaviour, and C++ leaves argument evaluation order
        // unspecified -- hence the locals).
        Vehicle::VehicleOutputInterface* const lpVehicleOutputInterface =
            lpPhysicsModuleOutputBuffer->GetVehicleOutputInterface();                    // @0x825A0080
        Vehicle::VehicleManagerOutputInterface* const lpManagerOutputInterface =
            lpPhysicsModuleOutputBuffer->GetVehicleManagerOutputInterface();             // @0x8259FFD8
        Vehicle::VehicleOutputRequestInterface* const lpOutputRequestInterface =
            lpVehManagerBuffer->GetVehicleOutputRequestInterface();                      // @0x825A1058
        const Vehicle::VehicleInputInterface* const lpVehicleInputInterface =
            lpPhysicsModuleInputBuffer->GetVehicleInputInterface();                      // @0x8259F8A0

        mVehicleManager.ProcessVehicleMaintenanceEvents(
            lpInputBufferStack, lpOutputBufferStack,
            lpVehicleInputInterface, lpOutputRequestInterface,
            lpManagerOutputInterface, lpVehicleOutputInterface,
            &mDeformationInput);

        CGS_ASSERT(lpSimInputBuffer != 0, "lpInputBuffer");                              // CgsModuleUtils.h:238
        lpSimInputBuffer->LockForWrite();

        // Same reinterpret_cast seam the mounted prop read-back in Update already uses: the module
        // output buffer's mSceneInputInterface is still an opaque 1-byte storage in
        // BrnPhysicsModuleIO.h, and its real type is what the DWARF calls
        // PhysicsModuleIO::OutputBuffer::SceneInputInterface. Not a new fork -- the identical cast
        // sits at BrnPhysicsModuleUpdateFunctions.cpp's PropManager::ReadUpdatedBodies call.
        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* const lpSceneInputInterface =
            reinterpret_cast<CgsSceneManager::SceneManagerIO::InSceneUpdateInterface*>(
                lpPhysicsModuleOutputBuffer->GetSceneInputInterface());                  // @0x825A0278

        HandleGameActionsPostScene(lpPhysicsModuleInputBuffer->GetGameActionQueue(),      // @0x8259FE88
                                   lpSimInputBuffer,
                                   lpSceneInputInterface);

        // ---- deformation post-scene, bracketed by the two real VerifyPartIndices sweeps --------
        CgsDev::PerfMonCpu::StartMonitor(miDeformationManagerPM);                        // +433132
        mDeformationManager.VerifyPartIndices();
        mDeformationManager.PostSceneUpdate(lpSimInputBuffer, &mDeformationInput,
                                            lpSceneInputInterface);
        mDeformationManager.VerifyPartIndices();
        CgsDev::PerfMonCpu::StopMonitor(miDeformationManagerPM);

        lpVehManagerBuffer->UnlockForWrite();

        // ---- props -----------------------------------------------------------------------------
        CgsDev::PerfMonCpu::StartMonitor(miPropManagerPM);                               // +433172
        mPropManager.ProcessInputsPreScene(
            lpPhysicsModuleInputBuffer->GetPropManagerInputInterface(),                  // @0x8259FDE0
            lpSceneInputInterface,
            (lUpdateSet & 1) != 0,                                                       // `rlwinm r7, a6, 0, 31, 31`
            lpSimInputBuffer);
        CgsDev::PerfMonCpu::StopMonitor(miPropManagerPM);

        lpPhysicsModuleOutputBuffer->UnlockForWrite();

        // THE SIM FIREWALL. Gate-bodied on purpose -- see its banner in
        // BrnPhysicsConductorGates.cpp. The call and its lock bracket are the console's; what is
        // deferred is the callee, which is the ONLY thing that would move the vehicle manager's
        // mRequiredRigidBodiesQueue into the simulation.
        lpPhysicsModuleOutputBuffer->LockForRead();
        lpVehManagerBuffer->LockForRead();
        BridgeVehicleManagerToSimulation_PostScene(lpSimInputBuffer, lpVehManagerBuffer);
        lpVehManagerBuffer->UnlockForRead();
        lpPhysicsModuleOutputBuffer->UnlockForRead();

        CGS_ASSERT(lpSimInputBuffer != 0, "lpInputBuffer");                              // CgsModuleUtils.h:248
        lpSimInputBuffer->UnlockForWrite();

        // ---- publish the player's active-race-car slot + deformation model slot -----------------
        // The console re-fetches the interface three times through the read-locked accessor rather
        // than caching it; each fetch is a lock tripwire, so the repetition is reproduced.
        // The console (0x825ABC10) reads mePlayerActiveRaceCarIndex RAW here -- `if (idx >= 8)
        // assert(h:967); if (idx != -1) active = mbIsPlayerCarActive;` -- which is exactly the
        // inlined IsPlayerCarActive() (its assert IS the h:967 one). It does NOT go through the
        // "hasn't been set" getter (h:980) for this test: -1 is an expected value at this site.
        // Fixed 2026-08-15: this used to call GetPlayerActiveRaceCarIndex() twice for the raw
        // reads, whose own assert fires on -1 -- silent while the (PC-only) IO-buffer zero-fill
        // made the index read 0, an assert storm (440/boot) the moment the interface was cleared
        // to -1 as the console does and the producer bridge is still a PC gate.
        bool lbPlayerCarActive = false;
        {
            const PhysicsModuleIO::InputBuffer::RCEntityOutputInterfaceStorage* const lpRCEntity =
                lpPhysicsModuleInputBuffer->GetRCEntityOutputInterface();                // @0x8259FA98
            lbPlayerCarActive = lpRCEntity->IsPlayerCarActive();                         // h:967 inline
        }

        // TWO DIFFERENT EntityId TYPES LIVE IN THIS TREE and this leg touches both: the packed
        // CgsSceneManager::EntityId class (which owns the K_INVALID_ENTITY_ID constant == the
        // console's dword_82F2A07C) and the plain ::EntityId storage word from BrnCommonTypes.h,
        // which is what both GetPlayerRaceCarEntityId and FindModelIndexByEntityID deal in. The
        // sentinel is taken from the named constant rather than written as a literal.
        EntityId lPlayerHandlingEntityId;
        lPlayerHandlingEntityId.muValue = static_cast<u32>(CgsSceneManager::K_INVALID_ENTITY_ID);

        if (lbPlayerCarActive)
        {
            const PhysicsModuleIO::InputBuffer::RCEntityOutputInterfaceStorage* const lpRCEntity =
                lpPhysicsModuleInputBuffer->GetRCEntityOutputInterface();
            CGS_ASSERT(lpRCEntity->GetPlayerActiveRaceCarIndex() != E_ACTIVE_RACE_CAR_INDEX_INVALID,
                       "Player car index hasn't been set");                              // ...OutputInterface.h:980
            gs_ePublishedPlayerActiveRaceCarIndex = lpRCEntity->GetPlayerActiveRaceCarIndex();
            mVehicleManager.SetPlayerActiveRaceCarIndex(lpRCEntity->GetPlayerActiveRaceCarIndex());

            lPlayerHandlingEntityId =
                lpPhysicsModuleInputBuffer->GetRCEntityOutputInterface()->GetPlayerRaceCarEntityId();  // @0x8259BB58
        }
        else
        {
            // The console INLINED SetPlayerActiveRaceCarIndex here with the constant -1, which is
            // why its own range assert (BrnVehicleManager.h:1667 / the committed body at
            // BrnVehicleManagerPlayerStats.cpp:251) is emitted at this site -- passing -1 ALWAYS
            // fails that range test, so the console fires the dialog on every frame this arm runs.
            //
            // PC-BUILD GUARD #1 (create-path wave 2026-08-10). MEASURED on the first boot with
            // this function live: 267 dialogs from this one line before the flow even reached the
            // junkyard handover. The console's flow does not conduct physics in menus at all, so
            // this arm is transient there; the PC world spine drives WorldModule::Update from BOOT,
            // ~200 s of it. The store is IDEMPOTENT (-1 over -1), so running it only on the edge is
            // byte-identical -- what is held is the redundant repeat of a shipped tripwire, not the
            // behaviour. The edge is a function-local static rather than a member for the same
            // reason PublishNewVehicleToDirectorWithoutPhysicsBringUp uses one: this state is not
            // the console's and the surrounding layout is offset-pinned.
            // DELETE WHEN the PC boot flow stops driving world updates before there is a car.
            if (gs_ePublishedPlayerActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID)
            {
                gs_ePublishedPlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
                mVehicleManager.SetPlayerActiveRaceCarIndex(E_ACTIVE_RACE_CAR_INDEX_INVALID);
            }
        }

        mDeformationManager.SetPlayerModelIndex(-1);   // `li r11,-1 ; stw r11, 0(&miPlayerModelIndex)` on BOTH arms

        // PC-BUILD GUARD #2 (create-path wave 2026-08-10), and this one is a DIRECT CONSEQUENCE
        // OF THIS WAVE'S DELIBERATE DEFERRAL -- worth stating exactly, because the assert it holds
        // is a real one that found a real fact.
        //
        // The console's own test is `!= K_INVALID_ENTITY_ID`, and it is written that way because on
        // the console maRaceCarStates[player].mEntityId is either a live car id or that sentinel.
        // On THIS build it is neither: it is IDENTICALLY ZERO. RCEntityActiveRaceCarOutputInterface
        // ::SetRaceCarState memcpy's the whole RaceCarState out of ActiveRaceCar::mPhysicsState, and
        // the only thing that ever fills mPhysicsState here is the bring-up stand-in
        // SeedPhysicsStateFromCreateEventBringUp, which writes mTransform and nothing else -- the
        // real producer, RaceCarEntityModule::ReadUpdatedActiveRaceCarDataFromPhysics @0x822E87B8,
        // is absent, and behind it VehicleManager::ProcessCreateEvents (this wave's gate).
        // So a zero sails through the console's sentinel test and FindModelIndexByEntityID asserts
        // on owner byte 0: MEASURED 663 dialogs in one run, every one after the junkyard handover.
        // That is the textbook silent-drop-stub signature -- plausible zeros arriving where real
        // data should -- caught by a shipped tripwire rather than by a wrong picture.
        // The owner test below is the PC guard; it is NOT a fix, it is the honest consequence of
        // the deferral, and it is scoped to exactly the ids the console can never produce.
        // DELETE WHEN ProcessCreateEvents + ReadUpdatedActiveRaceCarDataFromPhysics land.
        const u32 luHandlingOwner = (lPlayerHandlingEntityId.muValue >> 24) & 0xFFu;
        const bool lbHandlingIdNamesAVehicle = (luHandlingOwner == 1u) || (luHandlingOwner == 2u);

        if (lPlayerHandlingEntityId.muValue != static_cast<u32>(CgsSceneManager::K_INVALID_ENTITY_ID))
        {
            if (lbHandlingIdNamesAVehicle)
            {
                mDeformationManager.SetPlayerModelIndex(
                    mDeformationManager.FindModelIndexByEntityID(lPlayerHandlingEntityId));
            }
            else
            {
                static bool s_bLoggedZeroHandlingId = false;
                if (!s_bLoggedZeroHandlingId)
                {
                    s_bLoggedZeroHandlingId = true;
                    if (CgsDev::Message::gxMessageFilterFlags & 1)
                    {
                        *CgsDev::Log::gpDebugPrint
                            << "PhysicsModule::PostSceneUpdate: player RaceCarState.mEntityId is 0x"
                            << static_cast<s32>(lPlayerHandlingEntityId.muValue)
                            << " (owner " << static_cast<s32>(luHandlingOwner)
                            << ") -- no physics readback fills it yet, so the deformation model "
                               "lookup is skipped [FLAG PC boot gate]\n";
                    }
                }
            }
        }

        lpPhysicsModuleInputBuffer->UnlockForRead();

        // `lwz r11, 0x230(r30) ; addi r3, r30, 0x230 ; lwz r11, 0x48(r11) ; bctrl` -- the
        // simulation module's vtable slot 18. Slots 16/17/18 are Prepare/Update/ProcessInput per
        // CgsPhysicsSimulationModule.h's slot-by-slot read of off_820CF7D0, so this is
        // ProcessInput: the drain-only entry, no solver step.
        mSimulationModule.ProcessInput(lpSimInputBuffer);

        lpInputBufferStack->DestroyIOBuffer(&lpSimInputBuffer);
        lpOutputBufferStack->DestroyIOBuffer(&lpVehManagerBuffer);

        mVehicleManager.CheckState();
        CgsDev::PerfMonCpu::StopMonitor(miPhysicsPreSceneUpdatePM);
    }

    // =============================================================================================
    // GenerateSceneQueries @0x825A1428 (19 insns) -- the whole body, resetpump wave 2026-08-26.
    // This retires a one-shot "inert" boot gate in WorldLinkStubs.cpp.
    //
    //   0x825A1438  StartMonitor(*(this + 433124))          == miGenerateSceneQueriesPM  [PARKED]
    //   0x825A1448  assert(lpOutputBuffer != NULL)  "lpPhysicsModuleOutputBuffer != NULL"  (:195)
    //   0x825A1468  IOBuffer::LockForWrite(lpOutputBuffer)
    //   0x825A1470  v5 = lpOutputBuffer->GetVehicleOutputRequestInterface()   (@0x8259FF30, +16)
    //   0x825A1478  VehicleManager::GenerateAboveGroundLineTests(this + 19104, v5)
    //   0x825A1480  IOBuffer::UnlockForWrite(lpOutputBuffer)
    //   0x825A1488  StopMonitor                                                            [PARKED]
    //
    // ⚠️ Hex-Rays prints this as `GenerateSceneQueries(int a1, int a2)` and DROPS the BrnUpdateSet
    // third argument, which the declaration (and WorldModule::Update's call site) carries. The
    // body genuinely does not read it -- there is no `clrlwi` on r5 anywhere in the 19 instructions
    // -- so it is accepted and consumed here, not quietly removed from the signature.
    // [FLAG] the two PerfMonCpu calls: miGenerateSceneQueriesPM IS registered on this build
    // (BrnPhysicsModule.cpp:114), and WorldModule::Update already brackets this call with its own
    // miPhysicsModuleGenerateSceneQueriesPM monitor, so the inner pair would double-count one
    // region. Left out for the same reason the sibling stages leave theirs out.
    // =============================================================================================
    void PhysicsModule::GenerateSceneQueries(PhysicsModuleIO::OutputBuffer* lpOutputBuffer,
                                             BrnUpdateSet lUpdateSet)
    {
        (void)lUpdateSet;   // the console's r5; the body never reads it

        CGS_ASSERT(lpOutputBuffer != 0, "lpPhysicsModuleOutputBuffer != NULL");   // :195
        if (lpOutputBuffer == 0)
        {
            return;
        }

        lpOutputBuffer->LockForWrite();
        mVehicleManager.GenerateAboveGroundLineTests(
            lpOutputBuffer->GetVehicleOutputRequestInterface());
        lpOutputBuffer->UnlockForWrite();
    }
}

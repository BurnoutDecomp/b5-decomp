// ============================================================================
// GameSource/Physics/BrnPhysicsModuleUpdateFunctions.cpp
//
// BrnPhysics::PhysicsModule -- the per-frame Update helper functions' home TU
// (the console path baked into this TU's asserts is
// "...\gamesource\unity\../Physics/BrnPhysicsModuleUpdateFunctions.cpp").
// ⚠️ The dossier keys this function to BrnPhysicsModule.cpp; the baked :915 path
// above is the byte-grounded correction.
//
// This slice: FixUpVehicleContacts @ 0x825A6010 (1067 insns) -- the big-five
// opener of the PhysicsModule::Update subtree. Reconstructed from the
// BURNOUT_X360_ARTIST.XEX asm; the PS3 DecFIGS out-of-line build (@0x699058)
// corroborates the structure call-for-call (it keeps GetPhysicsEntityIDFrom-
// GlobalEntityID and the queue GetEvent out-of-line where the X360 inlines /
// out-of-lines differently).
//
// ⭐⭐ 2026-08-09 (CONDUCTOR WAVE): PhysicsModule::Update @0x825B0640 (1,999 insns)
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
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                  // gpDebugPrint / gxMessageFilterFlags (the PC boot guard)

namespace BrnPhysics
{
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
    // ⚠️ CATCHUP (lUpdateSet & 1): the console's network-catchup fast path -- locks the sim
    // output READ up front, skips the whole generation/step block, and calls the sim's
    // ProcessInput instead of the spy leg at the tail. Reconstructed as shipped even though
    // nothing sets the bit on PC yet.
    //
    // ⚠️ The streamed dev asserts (gpcMessageBuffer + StrStream hex ids) are LOWERED to
    // plain CGS_ASSERT with the console's message prefix -- the committed physics-TU
    // convention (see BrnPhysicsModuleBridgeFunctions.cpp:371).
    //
    // ⚠️⚠️ HONEST-CLOSURE NOTE: several callees below are LOUD one-shot inert gates this
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

        // ⚠⚠ PC-BUILD GUARD (conductor wave 2026-08-09). On the PC the world spine reaches
        // this function during BOOT/marketing frames too -- states in which the SIM TIMER has
        // never started, so both timer products are 0.0. The console never conducted physics
        // in that state (its boot flow doesn't drive WorldModule::Update -- the BF_LOADING
        // note), and the sim module's own shipped asserts prove it: a conducted frame with
        // timestep 0 fires `mfTimeStep > 0.0f` + `muMaxIterations > 0` EVERY FRAME (measured:
        // 934 assert dialogs in one 275s boot, flow never leaves BOOT). Until the sim timer
        // runs, behave exactly as the retired boot gate did -- log once, do nothing. GUARD
        // TESTS THE EXACT STATE THE CONSOLE NEVER ENTERED; delete when the boot flow stops
        // driving world updates through dead timers.
        lpPhysicsModuleInputBuffer->LockForRead();
        const bool lbSimTimerRunning =
            lpPhysicsModuleInputBuffer->GetTimerInterface()->GetSimTimerStatus()->IsRunning();
        lpPhysicsModuleInputBuffer->UnlockForRead();
        if (!lbSimTimerRunning)
        {
            static bool s_bLoggedNotRunning = false;
            if (!s_bLoggedNotRunning)
            {
                s_bLoggedNotRunning = true;
                if (CgsDev::Message::gxMessageFilterFlags & 1)
                    *CgsDev::Log::gpDebugPrint << "PhysicsModule::Update: sim timer not running -- "
                                                  "inert this frame [FLAG PC boot guard]\n";
            }
            return;
        }
        {
            static bool s_bLoggedConducting = false;
            if (!s_bLoggedConducting)
            {
                s_bLoggedConducting = true;
                if (CgsDev::Message::gxMessageFilterFlags & 1)
                    *CgsDev::Log::gpDebugPrint << "PhysicsModule::Update: CONDUCTING -- the sim "
                                                  "timer is live, the full per-frame pipeline runs "
                                                  "from here on\n";
            }
        }

        // The ten per-frame contact-spy container clears (mContactData.Clear() inlined on
        // the console -- see BrnContactSpyData.cpp), then the first state sweep.
        mContactData.Clear();
        mVehicleManager.CheckState();

        // ---- the five per-frame IO buffers ---------------------------------------------------
        // The X360 stack templates run each type's Construct after the alloc; the PC template
        // placement-news only, so Construct is explicit here (the WorldModule::Update precedent).
        CgsPhysics::PhysicsSimulationIO::InputBuffer*  lpSimInputBuffer  = 0;
        CgsPhysics::PhysicsSimulationIO::OutputBuffer* lpSimOutputBuffer = 0;
        Vehicle::VehicleManagerOutputBuffer*           lpVehManagerBuffer = 0;
        PhysicsModuleIO::PotentialContactInterface*    lpPotentialContacts = 0;
        Props::PropRaceCarContactBuffer*               lpPropRaceCarContacts = 0;

        lpInputBufferStack->CreateIOBuffer(&lpSimInputBuffer, "Simulation");            // @0x8259D940
        lpSimInputBuffer->Construct();
        lpOutputBufferStack->CreateIOBuffer(&lpSimOutputBuffer, "Simulation");          // @0x8259DCA0
        lpSimOutputBuffer->Construct();
        lpOutputBufferStack->CreateIOBuffer(&lpVehManagerBuffer, "VehManager");         // @0x8259DAF0
        lpVehManagerBuffer->Construct();
        lpOutputBufferStack->CreateIOBuffer(&lpPotentialContacts, "PotentialContacts"); // @0x825AC3C8
        lpPotentialContacts->Construct();
        lpOutputBufferStack->CreateIOBuffer(&lpPropRaceCarContacts, "PropRaceCarContacts"); // @0x825AC4A0
        lpPropRaceCarContacts->Construct();

        // ⭐ 2026-08-10 (root-cause wave): a const VIEW of the sim output buffer, nothing more.
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

        // ⚠ FLAG: gate-bodied this wave (BrnPhysicsConductorGates.cpp) -- the dispatch's own
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
            lpVehiclePrimAlloc->Construct();
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
            lpPropLinearAlloc->Construct();
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

            // ⚠ FLAG: gate-bodied. Console: the per-driver control dispatch.
            // ⚠ FLAG (storage->real seam cast, deliberate -- the driver-input span is still a
            // size-pinned opaque storage; retire when it adopts the real type):
            mVehicleManager.UpdateDrivers(
                lfSimTimerTimeStep,
                reinterpret_cast<const Vehicle::VehicleDriverInputInterface*>(
                    lpPhysicsModuleInputBuffer->GetVehicleDriverInterface()),
                lpVehManagerBuffer->GetVehicleOutputRequestInterface(),
                lpPhysicsModuleOutputBuffer->GetVehicleManagerOutputInterface(),
                &mDeformationInput,
                lpPhysicsModuleOutputBuffer->GetVehicleOutputInterface());

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
            // ⛔ 2026-08-10 (root-cause wave): this used to read `lpVehManagerBuffer->
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
            lpPotentialContacts->UnlockForRead();
            lpSimOutputBuffer->UnlockForWrite();

            BridgeUpdatedVehiclesToSimulation(lpSimInputBuffer, lpPhysicsModuleInputBuffer);
            lpSimInputBuffer->UnlockForWrite();

            // ---- THE SIMULATION STEP (console vtable slot 17 on mSimulationModule) -----------
            CgsDev::PerfMonCpu::StartMonitor(miPhysicsUpdateSimulationPM);               // +433100
            mSimulationModule.Update(lpInputBufferStack, lpOutputBufferStack,
                                     lpSimInputBuffer, lpSimOutputBuffer);
            CgsDev::PerfMonCpu::StopMonitor(miPhysicsUpdateSimulationPM);

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
            lpSimInputBuffer->Construct();

            lpSimOutputBuffer->LockForRead();
            lpVehManagerBuffer->LockForWrite();
            mVehicleManager.CheckState();

            // ---- read the stepped bodies back ------------------------------------------------
            CgsDev::PerfMonCpu::StartMonitor(miPhysicsUpdateReadUpdatedBodiesPM);        // +433104
            // ⛔ 2026-08-10 (root-cause wave): the sim output buffer is READ-locked here
            // (LockForRead above), so the mutable GetUpdateRigidBodyQueue() this used to select
            // fired "Not locked for writing" every frame. The console calls the CONST twin
            // @0x8259EFD0 at exactly this site; it is declared now, and both ReadUpdatedBodies
            // consumers already take a const queue pointer.
            mVehicleManager.ReadUpdatedBodies(
                lpConstSimOutputBuffer->GetUpdateRigidBodyQueue(),
                VecFloat{ lfSimTimerTimeStep, lfSimTimerTimeStep, lfSimTimerTimeStep, lfSimTimerTimeStep });
            CgsDev::PerfMonCpu::StopMonitor(miPhysicsUpdateReadUpdatedBodiesPM);

            mDeformationManager.VerifyPartIndices();

            mVehicleManager.UpdateVehiclePhysicsPostSimulation(
                lpPhysicsModuleInputBuffer->GetVehicleInputInterface(),
                lpSimOutputBuffer,
                lfSimTimerTimeStep,
                lpPhysicsModuleOutputBuffer->GetVehicleOutputInterface()->GetGameEventQueue());
            mVehicleManager.CheckState();

            CgsDev::PerfMonCpu::StartMonitor(miDeformationManagerPM);                    // +433132 (v518)
            mDeformationManager.UpdatePostPhysics(lpSimOutputBuffer, lpPhysicsModuleOutputBuffer,
                                                  &mContactData, lpInputBufferStack,
                                                  lpPotentialContacts);
            CgsDev::PerfMonCpu::StopMonitor(miDeformationManagerPM);
            mDeformationManager.VerifyPartIndices();
            mDeformationManager.VerifyPartIndices();

            // ⚠ FLAG (storage->real seam casts, deliberate): the output buffer's two
            // deformation seats are still size-pinned opaque storages; OutputData's DWARF
            // signature takes the real interface types. Same sanctioned seam as the
            // ProcessDeformationStates cast below -- retire both when the interfaces adopt
            // their real types in the buffer.
            mDeformationManager.OutputData(
                reinterpret_cast<Deformation::DeformationOutputInterfaceForEntityModules*>(
                    lpPhysicsModuleOutputBuffer->GetDeformationOutputInterfaceForEntityModules()),
                reinterpret_cast<Deformation::DeformationOutputInterface*>(
                    lpPhysicsModuleOutputBuffer->GetDeformationOutputInterface()));
            mDeformationManager.VerifyPartIndices();

            mVehicleManager.ProcessDeformationStates(
                reinterpret_cast<const Deformation::DeformationOutputInterface*>(
                    lpPhysicsModuleOutputBuffer->GetDeformationOutputInterface()));
            mDeformationManager.VerifyPartIndices();

            // ⚠ FLAG: gate-bodied (image-only address 0x8263C7C0).
            mVehicleManager.ProcessCrashingNetworkCars(
                reinterpret_cast<const Vehicle::VehicleDriverInputInterface*>(
                    lpPhysicsModuleInputBuffer->GetVehicleDriverInterface()),  // FLAG: storage->real seam cast
                lpVehManagerBuffer->GetVehicleOutputRequestInterface(),
                lpPhysicsModuleOutputBuffer->GetVehicleManagerOutputInterface(),
                &mDeformationInput,
                lpPhysicsModuleOutputBuffer->GetVehicleOutputInterface());
            mVehicleManager.CheckState();
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
}

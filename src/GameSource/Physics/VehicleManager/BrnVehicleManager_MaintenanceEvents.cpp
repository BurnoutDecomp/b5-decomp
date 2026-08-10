// =================================================================================================
// GameSource/Physics/VehicleManager/BrnVehicleManager_MaintenanceEvents.cpp
//
// VehicleManager::ProcessVehicleMaintenanceEvents @0x8264AB38 (118 insns), plus the five arms it
// dispatches and the traffic twin, as NAMED one-shot conductor gates.
//
// Slice TU (home BrnVehicleManager.cpp is still unmounted) -- the same shape as the sibling
// BrnVehicleManager_Prepare.cpp / _ReadUpdatedBodies.cpp / _TractionLineTests.cpp slices.
//
// -------------------------------------------------------------------------------------------------
// ⭐⭐ WHY THIS TU EXISTS NOW, AND WHY IT IS NOT THE CREATE PATH.
//
// The campaign brief named VehicleManager::ProcessCreateEvents @0x82616770 as the head of the
// list: it is the only writer in the whole XEX that SETS a bit in mUsedRaceCars (the tree's only
// other write is BrnVehicleManager_Construct.cpp's UnSetAll()), so until it runs the physics
// vehicle manager believes there are no cars. That is true. But it is not the head of the list,
// because ProcessCreateEvents HAS NO LIVE CALLER:
//
//     PhysicsModule::PostSceneUpdate @0x825ABC10     <- WorldLinkStubs boot gate until today
//       -> VehicleManager::ProcessVehicleMaintenanceEvents @0x8264AB38   <- absent until today
//            -> RecordNetworkRaceCarsAddedForCollision @0x825C7EA8 (321)
//            -> ProcessRemoveEvents                    @0x826160C8 (426)
//            -> ProcessCreateEvents                    @0x82616770 (1067)
//            -> ProcessValidationEvents                @0x825E9010 (65)
//            -> ProcessCollisionEvents                 @0x825E8F28 (export hole)
//            -> PhysicalTrafficManager::ProcessTrafficMaintenanceEvents @0x82649768 (246)
//
// `xrefs_to` on ProcessCreateEvents is a ONE-ELEMENT set: ProcessVehicleMaintenanceEvents. And
// `xrefs_to` on that is a one-element set: PostSceneUpdate. Landing 1,067 instructions of create
// body first would have produced a function nothing calls -- green gates, observable zero.
//
// -------------------------------------------------------------------------------------------------
// ⭐ THE PRODUCER SIDE IS ALREADY LIVE, WHICH IS WHY THE MEASUREMENT BELOW IS WORTH TAKING.
// VehicleInputInterface::CreateRaceCar @0x822CC1E8 is bodied and mounted
// (SharedIO/BrnVehicleInputInterface.cpp:55); its console caller ActiveRaceCar::AddHandlingModel
// @0x822D3EC8 is bodied; and THAT is called from the race-car promote site every boot
// (BrnRaceCarEntityModule.cpp:817). So create events have been posted into
// VehicleInputInterface::mCreateRaceCarEventQueue on every boot of this build and drained by
// nobody. The ProcessCreateEvents gate below PRINTS THAT QUEUE'S LENGTH -- the first direct
// measurement of the create path's input in this project, rather than an inference from the
// call graph.
//
// -------------------------------------------------------------------------------------------------
// ⛔⛔ AND WHY ProcessCreateEvents IS A GATE AND NOT A BODY -- THE MEASURED SPLIT POINT.
//
// Setting one bit of mUsedRaceCars is not bookkeeping on this build. It is the ON-SWITCH for four
// per-frame loops that are ALREADY MOUNTED AND ALREADY CALLED, every one of which walks that
// bitset and does nothing today only because it is empty:
//     BrnVehicleManager_ReadUpdatedBodies.cpp:100   gravity + ExternalPhysicsBody::IntegrateTransform
//     BrnVehicleManager_UpdateVehiclePhysics.cpp:383  ResetAboveGroundTestResult per car
//     BrnVehicleManager_UpdateVehiclePhysics.cpp:473  RaceCarPhysics::Update -- the 54 force leaves
//     BrnVehicleManagerContactGeneration.cpp:340      vehicle contact generation
// The first of those is the fall. It does not need the simulation and it does not need
// InAddRigidBody: PhysicsModule::Update reaches ReadUpdatedBodies every frame
// (BrnPhysicsModuleUpdateFunctions.cpp:650) and the only per-car guard is mbFrozen. So the create
// path cannot land before the traction-line chain -- exactly the order this repo's own
// BrnPhysicsConductorGates.cpp banner already states.
//
// -------------------------------------------------------------------------------------------------
// ⭐⭐⭐ WHAT THE MEASUREMENT ACTUALLY FOUND, 2026-08-10 -- READ THIS BEFORE PLANNING THE NEXT WAVE.
//
// The census below printed, on the first boot with this spine live, and never printed again:
//     [create-path] CreateRaceCarEvent queue length = 0
// It logs on CHANGE, so "printed once with 0" means the queue is EMPTY AT EVERY DRAIN, for the
// whole run -- including the thirteen frames on which the log shows
// "[PLACEONTRACK] Place on track request complete", i.e. frames on which the producer definitely
// ran. ⇒ **A fully reconstructed ProcessCreateEvents would drain ZERO events today.**
//
// Traced to the seam, not guessed. The producer chain is
//     RaceCarEntityModule::PrePhysicsUpdate -> PlaceOnTrackManager::PrePhysicsUpdate
//       -> RaceCarEntityModule::ResetActiveRaceCar( ..., lpOutput->GetVehicleInputInterface() )
//       -> ActiveRaceCar::AddHandlingModel -> VehicleInputInterface::CreateRaceCar @0x822CC1E8
//       -> mCreateRaceCarEventQueue.AddEvent
// so the events are staged in the RaceCarEntityModuleIO **OutputBuffer_PrePhysics**. The only
// thing that moves that buffer's interface into PhysicsModuleIO::InputBuffer is
//     WorldModule::BridgeEntityModulesToPhysicsModule_PrePhysics @0x827AAEC0 (271 insns)
// -- and that is a **WorldLinkStubs BOOT GATE, inert** (WorldLinkStubs.cpp:3199). Its PRE-SCENE
// sibling @0x827AADB8 IS real and DOES `VehicleInputInterface::Append` (which carries
// mCreateRaceCarEventQueue -- checked in the committed Append body, all fourteen queues), but the
// pre-scene race-car output is not where the create event is staged.
//
// ⇒ **THE NEXT HEAD OF THE LIST IS THE BRIDGE, NOT THE DRAIN.** Same silent-drop shape that ate
// the 396 world-collision events and the 28 triangle-cache events: a queue filled into a buffer
// whose only consumer is absent. Landing 1,067 instructions of create body before
// @0x827AAEC0 would produce a perfect function drinking from an empty pipe.
// ⚠ @0x827AAEC0 is not free: it also carries the traffic and prop PrePhysics interfaces and the
// vehicle-driver interface (its own asserts cite BrnVehicleDriverInputInterface.h:164/:260), so it
// is its own wave, not a line.
// -------------------------------------------------------------------------------------------------
//
// ⭐ The SECOND road to a falling body -- the one the brief named -- is already closed, and it is
// closed at a console function boundary rather than by anything invented here. ProcessCreateEvents
// posts its InAddRigidBody event into VehicleOutputRequestInterface::mRequiredRigidBodiesQueue
// (@+0x0000 of the "VehManager" stack IO buffer). The ONLY thing that ever moves that queue into
// the simulation is PhysicsModule::BridgeVehicleManagerToSimulation_PostScene @0x825AB408, which
// is itself a gate (BrnPhysicsConductorGates.cpp) -- and PostSceneUpdate destroys the VehManager
// buffer in the same call that creates it. See that gate's banner for the four queue offsets.
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h"  // GetCreateRaceCarEventQueue (the measurement)
#include "GameShared/GameClasses/Core/CgsAssert.h"                                // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                        // gpDebugPrint / gxMessageFilterFlags

namespace
{
    inline void MaintenanceGateLogOnce(bool& lrbLogged, const char* lpcMessage)
    {
        if (!lrbLogged)
        {
            lrbLogged = true;
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << lpcMessage;
        }
    }
}

#define BRN_MAINTENANCE_GATE(TAG)                                                          \
    do { static bool s_bLogged = false;                                                    \
         MaintenanceGateLogOnce(s_bLogged, "conductor gate: " TAG " inert [FLAG PC boot gate]\n"); } while (0)

namespace BrnPhysics
{
namespace Vehicle
{
    // ---------------------------------------------------------------------------------------------
    // ProcessVehicleMaintenanceEvents  @0x8264AB38  (118 insns; DWARF/asserts BrnVehicleManager.cpp
    // :1141..:1147)
    //
    // Read straight off the pseudocode AND the asm: seven null asserts in parameter order, then six
    // unconditional calls, no branch and no local state between them. The argument routing below is
    // the console's exactly -- note that the arms do NOT all take the same list:
    //     RecordNetworkRaceCarsAddedForCollision(this, in)
    //     ProcessRemoveEvents (this, in, out, mgrOut,        deform)
    //     ProcessCreateEvents (this, in, out, mgrOut,        deform)
    //     ProcessValidationEvents(this, in,                  deform)
    //     ProcessCollisionEvents (this, in,                  deform)
    //     PhysicalTrafficManager::ProcessTrafficMaintenanceEvents(this+44768, <all seven>)
    // The traffic call's r3 is `this + 44768`, which is &mPhysicalTrafficManager -- reached BY NAME
    // here, not by that offset.
    //
    // ⚠ The console returns the traffic call's result (r3 falls through). Nothing reads it: the one
    // caller, PhysicsModule::PostSceneUpdate, ignores the return. Declared void here, matching the
    // shape the caller actually uses; the discarded value is noted rather than fabricated into a
    // return type nobody consumes.
    // ---------------------------------------------------------------------------------------------
    void VehicleManager::ProcessVehicleMaintenanceEvents(
        CgsModule::IOBufferStack* lpInputBufferStack,
        CgsModule::IOBufferStack* lpOutputBufferStack,
        const VehicleInputInterface* lpInputInterface,
        VehicleOutputRequestInterface* lpOutputInterface,
        VehicleManagerOutputInterface* lpManagerOutputInterface,
        VehicleOutputInterface* lpVehicleOutputInterface,
        Deformation::DeformationInputInterface* lpDeformationInterface)
    {
        CGS_ASSERT(lpInputBufferStack        != 0, "lpInputBufferStack != NULL");        // :1141
        CGS_ASSERT(lpOutputBufferStack       != 0, "lpOutputBufferStack != NULL");       // :1142
        CGS_ASSERT(lpInputInterface          != 0, "lpInputInterface != NULL");          // :1143
        CGS_ASSERT(lpOutputInterface         != 0, "lpOutputInterface != NULL");         // :1144
        CGS_ASSERT(lpManagerOutputInterface  != 0, "lpManagerOutputInterface != NULL");  // :1145
        CGS_ASSERT(lpVehicleOutputInterface  != 0, "lpVehicleOutputInterface != NULL");  // :1146
        CGS_ASSERT(lpDeformationInterface    != 0, "lpDeformationInterface != NULL");    // :1147

        RecordNetworkRaceCarsAddedForCollision(lpInputInterface);

        ProcessRemoveEvents(lpInputInterface, lpOutputInterface,
                            lpManagerOutputInterface, lpDeformationInterface);

        ProcessCreateEvents(lpInputInterface, lpOutputInterface,
                            lpManagerOutputInterface, lpDeformationInterface);

        ProcessValidationEvents(lpInputInterface, lpDeformationInterface);

        ProcessCollisionEvents(lpInputInterface, lpDeformationInterface);

        mPhysicalTrafficManager.ProcessTrafficMaintenanceEvents(
            lpInputBufferStack, lpOutputBufferStack, lpInputInterface, lpOutputInterface,
            lpManagerOutputInterface, lpVehicleOutputInterface, lpDeformationInterface);
    }

    // ---- the five arms, as LOUD one-shot gates --------------------------------------------------
    // ⛔ NEVER silently no-op these: the log-once IS the loudness. Reconstruct each body in its own
    // TU and DELETE the gate here (duplicate-definition LNK2005 is the intended tripwire).

    void VehicleManager::RecordNetworkRaceCarsAddedForCollision(const VehicleInputInterface*)
    {
        BRN_MAINTENANCE_GATE("VehicleManager::RecordNetworkRaceCarsAddedForCollision @0x825C7EA8 (321)");
    }

    void VehicleManager::ProcessRemoveEvents(const VehicleInputInterface*,
                                             VehicleOutputRequestInterface*,
                                             VehicleManagerOutputInterface*,
                                             Deformation::DeformationInputInterface*)
    {
        BRN_MAINTENANCE_GATE("VehicleManager::ProcessRemoveEvents @0x826160C8 (426)");
    }

    // ⭐⭐ THE MEASUREMENT. This gate is the only one in the tree that reports a queue LENGTH
    // rather than just a name, because the length is the fact nobody has ever checked: does the
    // already-live producer chain (RaceCarEntityModule promote -> ActiveRaceCar::AddHandlingModel
    // @0x822D3EC8 -> VehicleInputInterface::CreateRaceCar @0x822CC1E8 -> mCreateRaceCarEventQueue
    // .AddEvent) actually put a create event in front of this function?
    //
    // It is a READ under the caller's existing read lock, through the CONST accessor, of a length
    // this build already maintains -- it constructs nothing, consumes nothing and changes no state.
    // The count is logged only when it CHANGES, so a queue that fills every frame does not flood
    // the log and a queue that is always empty prints exactly once.
    //
    // ⛔ WHAT MUST HAPPEN BEFORE THE 1,067-instruction body replaces this gate, measured, in order:
    //   1. the traction-line chain (~2,500 insns / 13 fns + the triangle-cache FILL worker
    //      ~1,183/11) -- otherwise the first registered car free-falls inside ReadUpdatedBodies;
    //   2. the create body's own absent callees: VehicleAttribs::Construct (export hole),
    //      sub_825BDB88 (42), physicsvehiclehandling ctor (export hole; the generated header
    //      DOES exist -- the old "does not exist in the tree" banner is STALE and retracted),
    //      StreamedDeformationSpec::TransformToNewCOMSpace (106) and ::GetBoundingBox (28),
    //      VehicleManager::AddRaceCarDeformationModel (153), SetAllNetworkRaceCarsHidden (175);
    //   3. the result consumer RaceCarEntityModule::ProcessCreateVehicleEvents @0x822FF620 (182),
    //      which today is stood in for by PublishNewVehicleToDirectorWithoutPhysicsBringUp.
    void VehicleManager::ProcessCreateEvents(const VehicleInputInterface* lpInputInterface,
                                             VehicleOutputRequestInterface*,
                                             VehicleManagerOutputInterface*,
                                             Deformation::DeformationInputInterface*)
    {
        BRN_MAINTENANCE_GATE("VehicleManager::ProcessCreateEvents @0x82616770 (1067) -- "
                             "the ONLY setter of mUsedRaceCars");

        // The create-queue census (see the banner above).
        static s32 s_iLastReportedLength = -1;
        if (lpInputInterface != 0)
        {
            const s32 liPending = lpInputInterface->GetCreateRaceCarEventQueue()->GetLength();
            if (liPending != s_iLastReportedLength)
            {
                s_iLastReportedLength = liPending;
                if (CgsDev::Message::gxMessageFilterFlags & 1)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[create-path] CreateRaceCarEvent queue length = " << liPending
                        << " (undrained; ProcessCreateEvents is a gate) [FLAG PC boot gate]\n";
                }
            }
        }
    }

    void VehicleManager::ProcessValidationEvents(const VehicleInputInterface*,
                                                 Deformation::DeformationInputInterface*)
    {
        BRN_MAINTENANCE_GATE("VehicleManager::ProcessValidationEvents @0x825E9010 (65)");
    }

    // ⚠️ @0x825E8F28 is a HOLE in the IDA export set -- it has no per-function JSON and is known
    // only by the name IDA prints at this one call site inside ProcessVehicleMaintenanceEvents.
    // Per the standing rule (missing-from-JSON != nonexistent) that is recorded, not treated as
    // proof of absence; the insn count is genuinely unknown and is left unstated rather than
    // guessed.
    void VehicleManager::ProcessCollisionEvents(const VehicleInputInterface*,
                                                Deformation::DeformationInputInterface*)
    {
        BRN_MAINTENANCE_GATE("VehicleManager::ProcessCollisionEvents @0x825E8F28 (export hole)");
    }

    void PhysicalTrafficManager::ProcessTrafficMaintenanceEvents(
        CgsModule::IOBufferStack*, CgsModule::IOBufferStack*,
        const VehicleInputInterface*, VehicleOutputRequestInterface*,
        VehicleManagerOutputInterface*, VehicleOutputInterface*,
        BrnPhysics::Deformation::DeformationInputInterface*)
    {
        BRN_MAINTENANCE_GATE("PhysicalTrafficManager::ProcessTrafficMaintenanceEvents @0x82649768 (246)");
    }
}
}

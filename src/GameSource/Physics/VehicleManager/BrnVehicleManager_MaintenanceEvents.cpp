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

    // ⛔⛔ STILL A GATE, AND THE REASON CHANGED ON 2026-08-11 (create-drain wave 2). The 426-insn
    // body was WRITTEN this wave, compiled, and then DELETED rather than shipped, because writing
    // it surfaced a blocker no previous wave had located -- see the "THE HANDLING-BODY HANDLE IS
    // EIGHT BYTES" block in BrnVehicleManager_CreateRemoveEvents.cpp. In one line: this body's
    // three deformation calls pass `maRaceCarHandlingBodyIDs[i]` (a u64 whose ENTIRE value lives in
    // its high dword) through `BrnPhysics::Deformation::RigidBodyId`, which this tree still
    // resolves to the 4-byte global stand-in in BrnCommonTypes.h:28. Every id would arrive as
    // ZERO, the deformation manager would find no model, and nothing would assert -- the exact
    // [[silent-drop-stubs]] shape. Landing it would have been worse than not landing it.

    void VehicleManager::ProcessRemoveEvents(const VehicleInputInterface*,
                                             VehicleOutputRequestInterface*,
                                             VehicleManagerOutputInterface*,
                                             Deformation::DeformationInputInterface*)
    {
        BRN_MAINTENANCE_GATE("VehicleManager::ProcessRemoveEvents @0x826160C8 (426) -- "
                             "BLOCKED on the 4-byte Deformation::RigidBodyId fork, not on recovery");
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
    // =============================================================================================
    // ⭐⭐⭐ RE-MEASURED 2026-08-11 (create-path wave 3). THIS REPLACES the old "what must happen
    // first" list, which was stale on four counts. Read it before planning the drain.
    //
    // (1) THE CLOSURE, machine-walked over all 30,084 X360 export JSONs BY ADDRESS (a name walk
    //     omits, with no diagnostic, the callees whose names collide), checked against the tree AND
    //     build_game_exe.bat: transitive closure of @0x82616770 = 4,370 insns / 47 nodes,
    //     CRT + DebugUI filtered.
    //     ALREADY BODIED **AND MOUNTED**: VehicleAttribs::SetupAttribs @0x825F4CD8 (770);
    //     VehicleAttribs::Construct @0x825F3FB8 (VehicleAttribs.cpp:439 -- the old banner's
    //     "export hole" is RETRACTED); EngineAttribs::InitializeFromAttribs (296);
    //     Wheel::TireAttribs::PrepareFrontTire / PrepareRearTire (192 + 192);
    //     AddRaceCarDeformationModel (153); SetNetworkRaceCarHidden (118);
    //     StreamedDeformationSpec::TransformToNewCOMSpace (106) and ::GetBoundingBox (28); and the
    //     whole AttribSys layer -- including the INLINE physicsvehiclehandling ctor
    //     (physicsvehiclehandling.h:170), its out-of-line copy ctor (`sub_825BDB88`), all eight
    //     Attrib::Gen::physicsvehicle*attribs ctors, and Attrib::AssertOnClassCheck.
    //     ⇒ GENUINELY MISSING: this 1,067-insn body plus VehicleManager::SetAllNetworkRaceCarsHidden
    //       @0x825E9380 (175). That is all of it -- 1,242 instructions, two functions.
    //     The four event-queue instantiations the body reaches (GetEvent @0x825BB7F0, the truncated
    //     export "BrnPhysics::Vehic", console element stride 0xA0; CreateVehicleResult::AddEvent;
    //     AddDeformationModelEvent::AddEvent; InAddRigidBody::AddEvent) are MOUNTED as of this wave
    //     and link clean -- the closure is already enforced.
    //
    // (2) ⭐⭐ THE TWO PRECONDITIONS THAT MADE THIS BODY IMPOSSIBLE ARE FIXED (this wave), and both
    //     were invisible until somebody PRINTED the event instead of reasoning about it. Measured
    //     BEFORE, read-only dump of create event 0 on a real boot:
    //         owner=0 idx=0 pos=(2986.933105,-3.525000,-2011.417969) model=0 gfx=0 ... strength=5
    //     * model=0 / gfx=0: the body does `lwzx r11,maRaceCarModelHandles[i] ; lwz r28,0(r11)`
    //       (@0x82616CF4 / @0x82616CFC) and then dereferences r28 as the StreamedDeformationSpec
    //       for the wheel loop, TransformToNewCOMSpace and GetBoundingBox. A null handle is an AV,
    //       not a soft failure. ROOT CAUSE: BaseResourcePtr::GetResourceHandle() returned mHandle
    //       (+0x04/+0x08) -- the CACHED IDENTITY of the resolved resource, which for a
    //       main-memory-only resource is legitimately {0,0}. The handle is the +0x14/+0x18 pair.
    //       FIXED in CgsBaseResourcePtr.cpp; four independent witnesses in its banner.
    //     * owner=0: ActiveRaceCar::Attach was skipping the console's mHandlingBodyVolumeId seed,
    //       so the id this body takes BOTH its owner assert (:1303) and its race-car SLOT INDEX
    //       (`extrwi r27,r9,14,8`) from was all zeroes. FIXED in BrnActiveRaceCar.cpp.
    //     Measured AFTER, same probe, same flow: owner=1 idx=0 ... model=1 gfx=1.
    //     ⇒ THE INPUT IS NOW COMPLETE, and the event carries a real Junkyard world position whose
    //       y (-3.525000) is the junkyard floor plane the traction line already returns hits on.
    //
    // (3) THE GROUND IS READY -- re-confirmed on this build, not inherited: world collision
    //     (23,645 leaves), the 2,937,088-byte arena, 28 cache slots, the fill worker, the
    //     traction-line producer lifetime and the drain all run every frame, and everything from a
    //     result record to Wheel::SetRoadContact -> mbIsOnGround is mounted.
    //
    // (4) WHAT THE NEXT WAVE HAS TO WRITE, in the body's own order (asm from @0x82616770):
    //       head      fetch event i, copy the 0xA0-byte record to a local, store both handles into
    //                 maRaceCarModelHandles / maRaceCarGraphicsModelHandles (@0x82616A64/A68, the
    //                 two stdx at this+43616+8i and this+43680+8i), three asserts.
    //                 ⚠️ those two arrays are still the opaque `mPadAA60[128]` in
    //                 BrnVehicleManager.h and MUST be split into real ResourceHandle[8] pairs --
    //                 which GROWS them 64 -> 128 bytes each on x64, so the layout gate's host-drift
    //                 constants move with them.
    //       attribs   Attrib::FindCollection(hash64("burnoutcarasset") == 0x52B81656F3ADF675,
    //                 event.mCarAssetAttribKey) -> Instance -> DefaultDataArea(0x228) ->
    //                 RefSpec::GetCollection(+0x158) -> physicsvehiclehandling ->
    //                 VehicleAttribs::Construct -> copy ctor -> SetupAttribs.
    //       wheels    TransformToNewCOMSpace, then a four-iteration loop over the spec's WheelSpecs
    //                 accumulating the COM vector (v125) and four scalars. SKIPPED ENTIRELY when
    //                 event.mbDisablePhysicsStateReset (@0x82616D38).
    //       inertia   ~200 insns of vrefp / vmaddfp Newton-Raphson building the mass + inertia
    //                 triple, then memcpy(0xC0) -> InAddRigidBody::AddEvent. ⚠️ its only consumer
    //                 is PhysicsModule::BridgeVehicleManagerToSimulation_PostScene @0x825AB408, a
    //                 DELIBERATELY INERT gate, and PostSceneUpdate destroys the VehManager buffer
    //                 in the same call -- so on this build that post is a producer into a buffer
    //                 nothing drains BY THE CONSOLE'S OWN DESIGN. ⛔ BUT DO NOT SUB-GATE THE MATH:
    //                 three of its outputs (the stack slots the asm calls var_900 / var_908 /
    //                 var_910..var_920) are ALSO arguments to the Prepare vcall below.
    //       seat      GetBoundingBox -> vcall vtable slot +0x30 == RaceCarPhysics::Prepare
    //                 @0x82639CB8 (EXPORT HOLE; its 40 instructions were lifted in create2_log).
    //                 ⚠️ NOT DECLARED ANYWHERE IN THIS TREE YET, and it is the ONLY thing that
    //                 seats mTransform / mHalfExtent / mSimpleAttribs -- every observable except
    //                 the bitset hangs off this one vcall. Also skipped when
    //                 mbDisablePhysicsStateReset.
    //       register  DebugComponent::Register, then the INLINED mUsedRaceCars.SetBit
    //                 (@0x826173C8 ldx / or / stdx) -- the observable this gate is named for.
    //       publish   CreateVehicleResult::AddEvent, AddRaceCarDeformationModel, the per-car scalar
    //                 seeds at stride 0xE0, SetNetworkRaceCarHidden / SetAllNetworkRaceCarsHidden,
    //                 two Attrib::Instance dtors.
    //
    // (5) and the result consumer RaceCarEntityModule::ProcessCreateVehicleEvents @0x822FF620 (182)
    //     -- today stood in for by PublishNewVehicleToDirectorWithoutPhysicsBringUp -- is still the
    //     only thing that would make RaceCarState::mEntityId non-zero.
    //
    // =============================================================================================
    // ⭐⭐⭐ RE-MEASURED AGAIN 2026-08-11 (create-drain wave). THREE CORRECTIONS TO (1)-(4) ABOVE,
    // each checked here rather than inherited. Read this block, not the one above it, when planning.
    //
    // (A) ⛔ THE COST IN (1) IS LOW BY 1.85x, AND IT IS LOW FOR TWO INDEPENDENT REASONS.
    //     * `VehicleManager::AddRaceCarDeformationModel` @0x825E9118 (153) is listed above as
    //       "ALREADY BODIED AND MOUNTED". It is **ABSENT** -- `grep -rn` over the whole tree finds
    //       the name in exactly two places, both of them comment lines in this banner. Its
    //       `xrefs_to` is a one-element set: this function.
    //     * The `Prepare` vcall chain contributes **ZERO** to the 4,370-instruction closure, because
    //       it is an INDIRECT call through vtable slot +0x30 and no `xrefs_from` walk can see it.
    //       Costed separately (seed @0x82637C80): 4,147 insns / 35 nodes, of which everything is
    //       already mounted EXCEPT the three Prepare bodies themselves.
    //     MEASURED total for a complete drain, by address, presence-checked against the tree AND
    //     build_game_exe.bat:
    //         ProcessCreateEvents            @0x82616770  1067   absent
    //         SetAllNetworkRaceCarsHidden    @0x825E9380   175   absent
    //         AddRaceCarDeformationModel     @0x825E9118   153   absent   <- the correction
    //         RaceCarPhysics::Prepare        @0x82639CB8  ~40    absent (X360 export hole)
    //         VehiclePhysics::Prepare        @0x82637C80   306   absent (declare-only)
    //         SimpleVehiclePhysics::Prepare  @0x8262F620   554   ⭐ LANDED THIS WAVE
    //     = 2,295 insns / 6 functions, of which 554 are now done and 1,741 remain.
    //
    // (B) ⭐⭐ THE "EXPORT HOLE WITH NO DECLARATION ANYWHERE" IS CLOSED, AND IT DID NOT NEED THE
    //     IMAGE. A name index over all 35,902 DecFIGS PS3 export JSONs carries all three Prepares
    //     mangled, with parameter names: RaceCarPhysics 0x73648C (89 insns), VehiclePhysics
    //     0x735DEC (424), SimpleVehiclePhysics 0x734D58 (541). See RaceCarPhysics.h's new banner for
    //     the eleven-parameter signature and the register-by-register agreement with this function's
    //     own call site @0x826171D8..0x8261724C. Every one of the eleven arguments now has a named
    //     producer, including `lInertia` (six doublewords == 48 bytes == the inertia block's output)
    //     and `lHandlingBodyOffset` (== v125, the four-wheel COM accumulator).
    //     ⛔ En route: VehiclePhysics::Prepare's committed declaration was a FIVE-parameter
    //     arity+type FORK against the real nine. Corrected; see VehiclePhysics.h:610.
    //
    // (C) ⭐⭐ RUN THE PATH FIRST -- a read-only probe at this exact point, three drains on one boot
    //     (2026-08-11T12:22:28, 1211 frames, both gates PASS, asserts=3), verbatim:
    //         PROBE drA idx=0 usedBits=0x0 sizeofVM=172928 offPad=43616 offUsed=44224
    //                       offCars=1856 sizeofRH=16 sizeofRCP=5216
    //         PROBE drA car vptr=951984048 pos=(0,0,0) half=(0,0,0) frozen=0 simpleValid=0
    //         PROBE drA spec=86050768 gfxMem=89927120
    //         PROBE drA mesh=(-0.000000,0.740575,-0.170226)
    //         PROBE drA wheel0 pos=(-0.829000,-0.409029,1.342442) scale=(0.259968,0.662665,0.662665)
    //         PROBE drA coll=417505728
    //     * `coll` non-null: Attrib::FindCollection(hash64("burnoutcarasset"), event.key) RESOLVES
    //       on this build. The attribs act will run; that was the largest untested assumption.
    //     * `spec`/`gfxMem` non-null and the spec's geometry is the real Hunter Cavalry data.
    //     * the car object IS constructed (non-null vptr) and nothing is seated yet.
    //     * `offPad=43616` -- the host layout matches the console byte for byte through mPadAA60
    //       (1856 + 8*5216 + 32), and `sizeofRH=16`, so splitting the pad into the two real
    //       ResourceHandle[8] arrays costs **+128 bytes** and moves every host-drift constant past
    //       it. Measured, so the next wave does not have to guess at it.
    //     * ⚠️ `frozen=0`: the car is NOT frozen, so the already-mounted gravity+integrate loop
    //       starts the instant the bit is set.
    //
    // (D) ⛔⛔ A HAZARD NO PREVIOUS WAVE RECORDED. The three drains report **idx=0, then 1, then 2**
    //     -- the create events name THREE DIFFERENT race-car slots over one boot, not one car three
    //     times. A drain that only creates would leave three live cars in mUsedRaceCars at the same
    //     world point. On console ProcessRemoveEvents @0x826160C8 (426, the gate above) frees the
    //     previous slot first, and this body's own assert list contains "Race Car Index Already
    //     Used" -- the console expects the slot to be free. ⇒ **ProcessRemoveEvents is part of the
    //     drain wave, not a later one.** That is a seventh function and it is not in the 2,295.
    //
    // (E) ⭐⭐ 2026-08-11 (VehiclePhysics::Prepare wave) -- THE CLOSURE IS DOWN TO **ONE** ABSENT
    //     CALLEE, AND IT IS THE ONE THAT MAY NOT LAND YET. Re-costed by address at this seed:
    //     4,370 insns / 47 nodes, and the ONLY genuinely-absent body left in the whole walk is
    //         AddRaceCarDeformationModel  @0x825E9118  (153)   [called from @0x82617400]
    //     -- the function held back because the handling-body handle is EIGHT bytes and routing
    //     the id through the tree's 4-byte ::RigidBodyId stand-in delivers a silent ZERO (a race
    //     car's LOW dword is identically zero). That widening is a decided question owed its own
    //     wave in the mounted DeformationManager; it must NOT be smuggled in here.
    //     Everything else the walk reports as absent is a false negative of the name matcher: the
    //     eight `Attrib::Gen::physicsvehicle*attribs` ctors, the checked copy @0x825BDB88 and the
    //     `Attrib::Class::TablePolicy` helpers are all exercised TODAY by the committed
    //     VehiclePhysics::SetAttributes(), which links and runs. ⇒ the drain's real remaining cost
    //     is **1,067 insns of this body plus a loud sub-gate on one call**, not a closure problem.
    //     The whole Prepare chain is now BODIED AND MOUNTED (VehiclePhysics::Prepare @0x82637C80
    //     landed this wave together with VehiclePhysics::SetAttributes @0x8262E140, the former
    //     sub_8262E140).
    //
    // (F) ⛔⛔ AND A RISK THE BRIEFS HAVE HAD BACKWARDS -- MEASURED, NOT ASSUMED. Every recent
    //     brief warns that draining here "arms the compensating pair": that seating
    //     VehiclePhysics::mTransform risks a 0.740575 m VISIBLE jump against the two golden frames.
    //     **It cannot, on this build, and the reason is a missing function rather than an argument.**
    //     The rendered pose comes from ActiveRaceCar::mRenderParams.mBodyTransform, whose ONLY
    //     console producer is ActiveRaceCar::UpdatePhysicsState @0x822D4418, whose ONLY caller is
    //     RaceCarEntityModule::ReadUpdatedActiveRaceCarDataFromPhysics @0x822E87B8 -- **which is
    //     absent from this build**. Today that pose is published solely by the bring-up stand-in
    //     PublishRenderPoseWithoutPhysicsBringUp, off ActiveRaceCar::mPhysicsState.mTransform,
    //     which only SeedPhysicsStateFromCreateEventBringUp ever writes (BrnPhysicsModuleUpdate-
    //     Functions.cpp:1033-1038 states exactly this, and the 663-dialog mEntityId==0 guard there
    //     is the shipped consequence of it).
    //     ⇒ VehiclePhysics::mTransform and the render pose live in DIFFERENT OBJECTS with the
    //     bridge between them missing. Draining here cannot move the car on screen.
    //     ⚠️⚠️ THE CONVERSE IS THE TRAP, and it is the one to carry forward: **observable "the
    //     car's height is unchanged after 275 s" CANNOT BE READ OFF THE SCREEN.** A body falling
    //     forever in maRaceCarPhysics[] would be completely invisible in both goldens. Whoever
    //     lands the drain must measure the fall with a probe on the VehicleManager side and must
    //     NOT read a stable-looking frame as evidence that the car is resting.
    //     DELETE-WHEN ReadUpdatedActiveRaceCarDataFromPhysics lands -- at THAT point, and only
    //     then, the compensating pair becomes real and the warning above becomes true.
    // =============================================================================================
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

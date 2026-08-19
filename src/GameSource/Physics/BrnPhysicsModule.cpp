#include "GameSource/Physics/BrnPhysicsModule.h"
#include "GameSource/Physics/BrnPhysicsModuleIO.h"                             // PhysicsModuleIO::InputBuffer (PropPrepareTypes)

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // CgsDev::PerfMonCpu::AddMonitor
#include "GameSource/Resource/SharedIO/BrnGameDataAllocatorList.h"        // GetRWLinearResourceAllocator (Prepare stage 3)
// The AllocatorList header only FORWARD-DECLARES rw::LinearResourceAllocator, so without this
// the derived->base conversion to rw::IResourceAllocator* that every stage-3..6 call needs is
// ill-formed (both types incomplete). The error names two types that look perfectly compatible.
#include "rw/rwcore_structs.h"                                            // rw::LinearResourceAllocator : IResourceAllocator

// ================================================================================================
// BrnPhysics::PhysicsModule -- constructor (X360 @0x827E5400) + Construct (X360 @0x825AE308).
//
// ⭐⭐ Construct IS BODIED as of 2026-08-03 (task #123). It was a no-op stub in WorldLinkStubs.cpp
// for the whole campaign, NOT because of link closure (that has been green since 54e1868d) but
// because the class layout ended 26,012 bytes before the last store. BrnPhysicsModule.h is
// re-seated now -- all five formerly-opaque sub-objects are real typed members and the trailing
// state/perf-monitor block is modelled from the DWARF -- so every store below lands on a NAMED
// member and every sub-Construct gets a properly-formed object instead of a raw offset into a
// `u8[]`. See that header's banner for the derivation and BrnPhysicsModule_layout_check.cpp for
// the gate.
//
// REACHABILITY: Construct is a virtual override reached at boot through the wired
// WorldModule::Construct @0x827CF540 fleet cascade, so it is not /OPT:REF-able and it really runs.
// ⭐ PROVEN, not assumed. A temporary one-shot witness was compiled into this body, run, observed
// and removed (2026-08-03):
//   [t123] PhysicsModule::Construct RAN; sizeof=429968 simMod=672 vehMgr=20288 contact=187312
//          defMgr=309856 defIn=387408 defOut=392768 propMgr=403776 tail=429824
//          simPM=109 propPM=113 fixupPM=103
// Every host offset is exactly the previous offset plus that member's host sizeof, with no gaps,
// and 429824 + 136 + 1 -> 16-align == 429968 == sizeof. The three sampled perf-monitor handles are
// valid distinct registry indices, so the twenty-one AddMonitor calls in the middle of this body
// really executed -- the function ran to completion, it did not merely get entered or linked.
// ================================================================================================

namespace BrnPhysics
{
    // The CPU budget every monitor but one is registered with (X360 flt_82004A20 == 10.0f, the
    // same literal BrnVehicleManager_Construct.cpp calls KF_VMAN_PERFMON_BUDGET).
    static const f32 KF_PHYS_PERFMON_BUDGET      = 10.0f;
    // ...and the one exception: "    Prop Manager" is registered against flt_82001D9C == 2.0f.
    static const f32 KF_PROPMANAGER_PERFMON_BUDGET = 2.0f;

    // --------------------------------------------------------------------------------------------
    // Default constructor -- X360 @0x827E5400.
    //
    // X360 store-for-store (asm spine):
    //   *this              = &off_820CE500          // base ModuleSingleBuffered vtable
    //   RWMutex(this+0x10,  0, 1)                   // base mInputMutex  (initial owner 0, lock-count 1)
    //   RWMutex(this+0x118, 0, 1)                   // base mOutputMutex
    //   *this              = off_820D12E8           // derived PhysicsModule vtable
    //   PhysicsSimulationModule::ctor(this+0x230)   // embedded mSimulationModule
    //   VehicleManager::ctor(this+0x4AA0)           // embedded mVehicleManager
    //   *(this+0x63630)    = off_820CDF60           // <- mPropManager.mDebugComponent's vptr
    //   *(this+0x63684 +0/+4/+8/+0) = 0             // <- mPropManager.mpPhysicsData: BaseResourcePtr()
    //   *(this+0x63684 +C/+10/+14)  = this+0x63684  //    self-circular alias ring
    //   *(this+0x63684 +18)         = 0             //    muThreadId
    //
    // ⭐⭐ THE TRAILING EIGHT STORES ARE NOT THIS CONSTRUCTOR'S OWN WORK -- RESOLVED 2026-08-03.
    // They used to be reproduced here by hand against a fabricated `ContainedListInterface
    // mContainedList` member, described as "a trailing contained interface-with-intrusive-list
    // sub-object". There is no such sub-object. +0x63630 is mPropManager, and:
    //   * the vtable stamp is mPropManager.mDebugComponent's vptr -- PropDebugComponent derives
    //     from CgsDev::DebugComponent and has virtuals, so the compiler emits that store itself;
    //   * +0x63684 is mPropManager +0x54 == `CgsResource::ResourcePtr<PropPhysicsDataHeader>
    //     mpPhysicsData`, and CgsBaseResourcePtr.cpp documents BaseResourcePtr() @0x82204E20 as
    //     exactly "stw 0 ->+0,+4,+8 (then a redundant 0 ->+0), stw this ->+0xC,+0x10,+0x14,
    //     stw 0 ->+0x18" -- instruction for instruction what is inlined here, including the
    //     redundant re-store of +0.
    // Both are therefore the IMPLICIT MEMBER CONSTRUCTION of mPropManager. Declaring the real
    // member reproduces them through the real constructors; the hand-rolled copies are deleted,
    // not dropped. (The old hand-rolled version was also strictly worse: it stamped a NULL vtable
    // pointer because off_820CDF60 was unreconstructed.)
    //
    // FLAG -- still DEFERRED: mVehicleManager's own constructor (X360 @0x827E4D58) is deferred in
    // its home TU (BrnVehicleManagerPlayerStats.cpp) because that class is padding-modelled; the
    // embed therefore default-constructs trivially here. It folds in when that pass lands.
    // --------------------------------------------------------------------------------------------
    PhysicsModule::PhysicsModule()
    {
        // Everything this constructor does is implicit member construction:
        //   - the ModuleSingleBuffered base (its vtable + mInputMutex/mOutputMutex),
        //   - the derived vtable,
        //   - mSimulationModule  (PhysicsSimulationModule::PhysicsSimulationModule @0x827DF1E0),
        //   - mVehicleManager    (deferred, see above),
        //   - mPropManager       (its mDebugComponent vptr + mpPhysicsData's BaseResourcePtr()).
        // There is no explicit body on the console beyond those chained calls.
    }

    // --------------------------------------------------------------------------------------------
    // Construct -- X360 @0x825AE308. Virtual override; reached through the module fleet cascade.
    //
    // Order below is the ASM's order, which is NOT the member declaration order: the compiler
    // issued the twenty-one AddMonitor calls in source order and the members happen to be declared
    // in a different one. Every store is by name; the offsets in the comments are the X360 seats.
    // --------------------------------------------------------------------------------------------
    void PhysicsModule::Construct()
    {
        ModuleSingleBuffered::Construct();

        mePrepareStage = E_PREPARESTAGE_START;   // +433072 stwx 0
        meReleaseStage = E_RELEASESTAGE_DONE;    // +433076 stwx 7

        miPhysicsPreSceneUpdatePM = CgsDev::PerfMonCpu::AddMonitor(
            "    Physics Pre Scene Update", CgsDev::E_PMP_6, false, KF_PHYS_PERFMON_BUDGET, true);
        CGS_ASSERT(miPhysicsPreSceneUpdatePM >= 0, "miPhysicsPreSceneUpdatePM >= 0");        // :63

        miGenerateSceneQueriesPM = CgsDev::PerfMonCpu::AddMonitor(
            "    Generate Scene Queries", CgsDev::E_PMP_6, false, KF_PHYS_PERFMON_BUDGET, true);
        CGS_ASSERT(miGenerateSceneQueriesPM >= 0, "miGenerateSceneQueriesPM >= 0");          // :66

        miDeformationManagerPM = CgsDev::PerfMonCpu::AddMonitor(
            "    DeformationManager", CgsDev::E_PMP_6, false, KF_PHYS_PERFMON_BUDGET, true);
        CGS_ASSERT(miDeformationManagerPM >= 0, "miDeformationManagerPM >= 0");              // :69

        miPhysicsBridgeContactsPM = CgsDev::PerfMonCpu::AddMonitor(
            "    Bridge Contacts To Sim", CgsDev::E_PMP_6, false, KF_PHYS_PERFMON_BUDGET, true);
        CGS_ASSERT(miPhysicsBridgeContactsPM >= 0, "miPhysicsBridgeContactsPM >= 0");        // :72

        miPhysicsUpdateContactGenAsyncPM = CgsDev::PerfMonCpu::AddMonitor(
            "    Asyncronous contact gen", CgsDev::E_PMP_6, false, KF_PHYS_PERFMON_BUDGET, true);
        CGS_ASSERT(miPhysicsUpdateContactGenAsyncPM >= 0,
                   "miPhysicsUpdateContactGenAsyncPM >= 0");                                 // :75

        miPhysicsUpdateDoVehicleContactGenStartPM = CgsDev::PerfMonCpu::AddMonitor(
            "        Veh Cont Gen Start", CgsDev::E_PMP_6, false, KF_PHYS_PERFMON_BUDGET, true);
        CGS_ASSERT(miPhysicsUpdateDoVehicleContactGenStartPM >= 0,
                   "miPhysicsUpdateDoVehicleContactGenStartPM >= 0");                        // :78

        miPhysicsUpdateDoPartContactGenStartPM = CgsDev::PerfMonCpu::AddMonitor(
            "        Part Cont Gen Start", CgsDev::E_PMP_6, false, KF_PHYS_PERFMON_BUDGET, true);
        CGS_ASSERT(miPhysicsUpdateDoPartContactGenStartPM >= 0,
                   "miPhysicsUpdateDoPartContactGenStartPM >= 0");                           // :81

        miPhysicsUpdateDoPropContactGenStartPM = CgsDev::PerfMonCpu::AddMonitor(
            "        Prop Cont Gen Start", CgsDev::E_PMP_6, false, KF_PHYS_PERFMON_BUDGET, true);
        CGS_ASSERT(miPhysicsUpdateDoPropContactGenStartPM >= 0,
                   "miPhysicsUpdateDoPropContactGenStartPM >= 0");                           // :84

        miPhysicsUpdateDoVehicleContactGenEndPM = CgsDev::PerfMonCpu::AddMonitor(
            "        Vehicle Cont Gen End", CgsDev::E_PMP_6, false, KF_PHYS_PERFMON_BUDGET, true);
        CGS_ASSERT(miPhysicsUpdateDoVehicleContactGenEndPM >= 0,
                   "miPhysicsUpdateDoVehicleContactGenEndPM >= 0");                          // :87

        miPhysicsUpdateValidateRaceCarWorldContactPM = CgsDev::PerfMonCpu::AddMonitor(
            "        Validate RC-world conts", CgsDev::E_PMP_6, false, KF_PHYS_PERFMON_BUDGET, true);
        CGS_ASSERT(miPhysicsUpdateValidateRaceCarWorldContactPM >= 0,
                   "miPhysicsUpdateValidateRaceCarWorldContactPM >= 0");                     // :90

        // ⚠️⚠️ REPRODUCED BUG, NOT A TRANSCRIPTION SLIP -- the shipped console source asserts the
        // WRONG member here. The asm is unambiguous (0x825AE674 `stwx r3,r31,0x69C30` stores the
        // handle into miPhysicsUpdateFixUpVehContactsPM @+433200, then 0x825AE678 `lwz r11,0(r30)`
        // with r30 == this+0x69BD8 == +433112 reads miPhysicsUpdateCrashPredictionPM and asserts on
        // THAT, at BrnPhysicsModule.cpp:93 -- a member which is not assigned until the very next
        // registration, three instructions later). A copy-paste bug in the original at line 92/93.
        // It is harmless here: the module lives in zero-initialised storage, so the unassigned
        // member reads 0 and the assert passes -- which is also why it never fired on the console.
        miPhysicsUpdateFixUpVehContactsPM = CgsDev::PerfMonCpu::AddMonitor(
            "        Fix up vehicle conts", CgsDev::E_PMP_6, false, KF_PHYS_PERFMON_BUDGET, true);
        CGS_ASSERT(miPhysicsUpdateCrashPredictionPM >= 0,
                   "miPhysicsUpdateCrashPredictionPM >= 0");                                 // :93

        miPhysicsUpdateCrashPredictionPM = CgsDev::PerfMonCpu::AddMonitor(
            "        Crash Prediction", CgsDev::E_PMP_6, false, KF_PHYS_PERFMON_BUDGET, true);
        CGS_ASSERT(miPhysicsUpdateCrashPredictionPM >= 0,
                   "miPhysicsUpdateCrashPredictionPM >= 0");                                 // :96

        miPhysicsUpdateVehiclePhysicsPM = CgsDev::PerfMonCpu::AddMonitor(
            "        Vehicle Physics", CgsDev::E_PMP_6, false, KF_PHYS_PERFMON_BUDGET, true);
        CGS_ASSERT(miPhysicsUpdateVehiclePhysicsPM >= 0,
                   "miPhysicsUpdateVehiclePhysicsPM >= 0");                                  // :99

        miPhysicsUpdateDoPartContactGenEndPM = CgsDev::PerfMonCpu::AddMonitor(
            "        Part Cont Gen End", CgsDev::E_PMP_6, false, KF_PHYS_PERFMON_BUDGET, true);
        CGS_ASSERT(miPhysicsUpdateDoPartContactGenEndPM >= 0,
                   "miPhysicsUpdateDoPartContactGenEndPM >= 0");                             // :102

        miPhysicsUpdateDoPropContactGenEndPM = CgsDev::PerfMonCpu::AddMonitor(
            "        Prop Cont Gen End", CgsDev::E_PMP_6, false, KF_PHYS_PERFMON_BUDGET, true);
        CGS_ASSERT(miPhysicsUpdateDoPropContactGenEndPM >= 0,
                   "miPhysicsUpdateDoPropContactGenEndPM >= 0");                             // :105

        miPhysicsProcessRaceCarContactsPM = CgsDev::PerfMonCpu::AddMonitor(
            "    Process Race Car Contacts", CgsDev::E_PMP_6, false, KF_PHYS_PERFMON_BUDGET, true);
        CGS_ASSERT(miPhysicsProcessRaceCarContactsPM >= 0,
                   "miPhysicsProcessRaceCarContactsPM >= 0");                                // :108

        miPhysicsUpdateSimulationPM = CgsDev::PerfMonCpu::AddMonitor(
            "    Simulation", CgsDev::E_PMP_6, false, KF_PHYS_PERFMON_BUDGET, true);
        CGS_ASSERT(miPhysicsUpdateSimulationPM >= 0, "miPhysicsUpdateSimulationPM >= 0");    // :111

        miPhysicsUpdateReadUpdatedBodiesPM = CgsDev::PerfMonCpu::AddMonitor(
            "    ReadUpdatedBodies", CgsDev::E_PMP_6, false, KF_PHYS_PERFMON_BUDGET, true);
        CGS_ASSERT(miPhysicsUpdateReadUpdatedBodiesPM >= 0,
                   "miPhysicsUpdateReadUpdatedBodiesPM >= 0");                               // :114

        miDeformationMaintenancePM = CgsDev::PerfMonCpu::AddMonitor(
            "    Deformation Maintenance", CgsDev::E_PMP_6, false, KF_PHYS_PERFMON_BUDGET, true);
        CGS_ASSERT(miDeformationMaintenancePM >= 0, "miDeformationMaintenancePM >= 0");      // :117

        miContactSpyListGenerationPM = CgsDev::PerfMonCpu::AddMonitor(
            "    Contact Spy generation", CgsDev::E_PMP_6, false, KF_PHYS_PERFMON_BUDGET, true);
        CGS_ASSERT(miContactSpyListGenerationPM >= 0, "miContactSpyListGenerationPM >= 0");  // :120

        // The one monitor registered against a 2.0f budget (X360 flt_82001D9C), not 10.0f.
        miPropManagerPM = CgsDev::PerfMonCpu::AddMonitor(
            "    Prop Manager", CgsDev::E_PMP_6, false, KF_PROPMANAGER_PERFMON_BUDGET, true);
        CGS_ASSERT(miPropManagerPM >= 0, "miPropManagerPM >= 0");                            // :123

        // The six prop-manager monitor ids the module owns but does NOT register: it zeroes its
        // own copy and delegates the real registration to the prop manager, whose two
        // Construct*PerfMonitors bodies zero the ids that live inside PropManager itself.
        miPropManagerPreScenePM = 0;                              // +433176
        mPropManager.ConstructPreScenePerfMonitors();

        miPropManagerWorldContactGenPM = 0;                       // +433180
        mPropManager.ConstructContactGenerationPerfMonitors();

        miPropManagerProcessInputsPM     = 0;                     // +433184
        miPropManagerReadUpdatedBodiesPM = 0;                     // +433188
        miPropManagerOutputUpdatedPropsPM = 0;                    // +433192
        miPropManagerApplyShockwavePM    = 0;                     // +433196

        // The console dispatches this one through mSimulationModule's vtable slot 0 (it did not
        // devirtualise the call); the dynamic type is exactly PhysicsSimulationModule, so a direct
        // member call is the same source and the same behaviour.
        mSimulationModule.Construct();       // +560

        mVehicleManager.Construct();         // +19104
        mDeformationManager.Construct();     // +314272
        mPropManager.Construct();            // +407088
        mDeformationInput.Construct();       // +391024
        mDeformationOutput.Construct();      // +396096
        mContactData.Construct();            // +191728

        mWorldEntityId                 = CgsSceneManager::K_INVALID_ENTITY_ID;  // +433088 (dword_82F2A07C)
        mbIsOnlineGameMode             = false;                                 // +433208 stbx
        miFramesToForceSuperSlowMotion = 0;                                     // +433204
        meCurrentGameMode              = BrnGameState::GameStateModuleIO::E_MODE_NONE; // +433092 (-1)

        // Base member (CgsModule::Module::mbIsNewModule, console +4). ModuleSingleBuffered::Construct
        // cleared it at the top of this function; the derived Construct sets it back. `stb r10,4(r31)`.
        mbIsNewModule = true;
    }

    // ================================================================================================
    // PhysicsModule::PropPrepareTypes -- X360 @0x825A14A8 (23 insns), store for store. The prop
    // stage tail of WorldModule::Prepare @0x827D53B0 calls it with the physics input buffer the
    // world just filled (BridgePropModuleToPhysics_Prepare appended the prop module's
    // PropInputInterface, carrying the prop-physics data ResourceHandle PropEntityModule::Prepare
    // posted). LANDED 2026-08-19 (wave Q5 round-3 integration; was a WorldLinkStubs boot gate --
    // the first car-vs-prop contact died in PropManager::ProcessAddPropInstanceEvents because
    // mpPhysicsData had never been bound):
    //   0x825A14C8  bl IOBuffer::LockForRead(lpInputBuffer)
    //   0x825A14D0  bl PhysicsModuleIO::InputBuffer::GetPropManagerInputInterface (const, 0x8259FDE0)
    //   0x825A14E0  bl PropManager::ProcessInputs_Prepare(this+407088 == &mPropManager, iface)
    //   0x825A14E8  bl IOBuffer::UnlockForRead(lpInputBuffer)
    void PhysicsModule::PropPrepareTypes( PhysicsModuleIO::InputBuffer* lpInputBuffer )
    {
        lpInputBuffer->LockForRead();
        const PhysicsModuleIO::InputBuffer* lpConstInput = lpInputBuffer;   // the CONST accessor @0x8259FDE0
        mPropManager.ProcessInputs_Prepare( lpConstInput->GetPropManagerInputInterface() );
        lpInputBuffer->UnlockForRead();
    }

    // PhysicsModule::Prepare -- X360 @0x825ADB68 (BrnPhysicsModule.cpp:192).
    //
    // ⭐⭐ THE POINT OF THIS FUNCTION, for this campaign, IS STAGE 3. Until 2026-08-04 the whole
    // body was a one-line `return true` stub in WorldLinkStubs.cpp, so the simulation module's own
    // Prepare -- the ONLY assignment to CgsPhysics::PhysicsSimulationModule::mpSimulation anywhere
    // in the tree -- was never called, and every reconstructed rw::physics solver object linked and
    // was unreachable. Stage 3 is what closes that.
    //
    // A resumable ten-stage fall-through FSM over mePrepareStage. Each arm stamps its own stage
    // number FIRST (so a `false` return leaves the cursor exactly where the work stopped and the
    // world spine re-enters next frame), then does its work; the last arm resets the cursor to
    // MANAGER and reports success. `default` fires the console's "Invalid Stage\n" assert.
    //
    // ⚠️ STAGE 7 (CREATE_PLAYER_VEHICLE) HAS NO ARM OF ITS OWN. The console's `case 7` jumps
    // straight to LABEL_17, which stamps stage 8 and falls into the world-rigid-body arm. That is
    // not an omission here: the stage exists in the enum and is reachable as a resume point, but no
    // code runs for it.
    //
    // ⛔⛔ NOT REPRODUCED, DELIBERATELY AND VISIBLY -- three groups. None of them is reachable
    // work today (each one's target subsystem is itself inert), but every one of them is a REAL
    // console store or call and this comment is the whole record of that:
    //
    //   (a) stage 4's fourteen `stw 0` after DeformationManager::Prepare succeeds, at X360
    //       +391032, +394248, +394424, +394888, +395160, +395624, +396080, +396208, +396220,
    //       +397032, +398648, +398984, +403000, +406848. They land inside mDeformationInput
    //       (+391024) and mDeformationOutput (+396096) -- the deformation IO queue counters --
    //       and NEITHER class has a reconstructed member map, so naming them is impossible and
    //       writing them by raw offset would be an offset hack. They stay out until those two
    //       interfaces are reconstructed. Harmless *only* because the module lives in
    //       zero-initialised storage and nothing has written those fields yet; the moment
    //       deformation IO goes live this becomes a real dropped clear.
    //
    //   (b) the three sibling sub-Prepares -- DeformationManager (stage 4), PropManager (stage 5)
    //       and VehicleManager (stage 6). Each is a named LINK STUB in WorldLinkStubs.cpp, so the
    //       drop is one greppable symbol per subsystem instead of one silent `return true` for
    //       the whole module. DeformationManager::Prepare actually EXISTS (bodied in the unmounted
    //       BrnDeformationManager.cpp); mounting that TU is its own closure job.
    //
    //   (c) stage 8's body: `CreateIOBuffer<PhysicsSimulationIO::InputBuffer>("Simulation")`,
    //       PrepareWorldRigidBody @0x825A9834, `mWorldRigidBodyId = <that id>`, DestroyIOBuffer.
    //       PrepareWorldRigidBody is not reconstructed, so creating a multi-kilobyte IO buffer on
    //       the stack purely to hand it to nothing would be pure risk for zero observable. The
    //       stage stamps its cursor and falls through, as the console's does.
    // ================================================================================================
    bool PhysicsModule::Prepare( CgsModule::IOBufferStack* lpInputBufferStack,
                                 CgsModule::IOBufferStack* lpOutputBufferStack,
                                 CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer,
                                 BrnResource::GameDataIO::AllocatorList* lpAllocatorList )
    {
        CGS_ASSERT( lpInputBufferStack  != 0, "lpInputBufferStack != NULL" );        // :194
        CGS_ASSERT( lpOutputBufferStack != 0, "lpOutputBufferStack != NULL" );       // :195
        CGS_ASSERT( lpSceneInputBuffer  != 0, "lpSceneInputBuffer_Update != NULL" ); // :196
        CGS_ASSERT( lpAllocatorList     != 0, "lpAllocatorList != NULL" );           // :197

        (void)lpInputBufferStack;      // consumed by stage 8's CreateIOBuffer -- see (c) above
        (void)lpOutputBufferStack;     // asserted only; the console never uses it either
        (void)lpSceneInputBuffer;      // consumed by stage 6's VehicleManager::Prepare

        switch ( mePrepareStage )
        {
        default:
            // `FireAssert("Invalid Stage\n", ..., 332)`
            CGS_ASSERT( false, "Invalid Stage\n" );
            return false;

        case E_PREPARESTAGE_START:
        case E_PREPARESTAGE_MANAGER:
            mePrepareStage                 = E_PREPARESTAGE_MANAGER;   // *v11 = 1
            miFramesToForceSuperSlowMotion = 0;                        // +433204
            if ( !ModuleSingleBuffered::Prepare() )
                return false;
            // fall through

        case E_PREPARESTAGE_CONTACTMAPPER:
            // The console's `case 2` / LABEL_11 arm: it stamps the stage and nothing else.
            mePrepareStage = E_PREPARESTAGE_CONTACTMAPPER;             // *v11 = 2
            // fall through

        case E_PREPARESTAGE_SIMULATIONMODULE:
        {
            mePrepareStage = E_PREPARESTAGE_SIMULATIONMODULE;          // *v11 = 3

            // The four SimulationParams fields, built on the stack. All four are literals
            // Hex-Rays recovered directly (v20[1] = -9.8100004, v22 = 0.1, v23 = 0.016666668,
            // v24 = 2); the gravity vector is moved into the params block with one lvx/stvx pair.
            //
            // ⚠️ THIS -9.81 IS NOT rw::physics::Simulation::Initialize's DEFAULT GRAVITY. That
            // one is -600.0f (read out of the X360 .rdata this wave). Initialize seeds the SDK
            // default and this call overwrites it with the GAME's; both numbers are real and they
            // are not the same number.
            CgsPhysics::PhysicsSimulationModule::SimulationParams lSimulationParams;
            lSimulationParams.mafGravity[0]    = 0.0f;
            lSimulationParams.mafGravity[1]    = -9.8100004f;
            lSimulationParams.mafGravity[2]    = 0.0f;
            lSimulationParams.mafGravity[3]    = 0.0f;
            lSimulationParams.mfFreezingEnergy = 0.1f;
            lSimulationParams.mfTimeStep       = 0.016666668f;          // 1/60
            lSimulationParams.muMaxIterations  = 2u;

            // Bank 23 is the physics resource allocator. WorldModule::Prepare's SCENE stage has
            // already refused to advance unless banks 49/23/61 are all live, so it is non-null
            // here by construction.
            rw::LinearResourceAllocator* lpAllocator =
                lpAllocatorList->GetRWLinearResourceAllocator( 23 );

            // The console dispatches through mSimulationModule's vtable slot 16
            // (`lwz r11,0x40(r11)`); the dynamic type is exactly PhysicsSimulationModule, so a
            // direct member call is the same source and the same behaviour.
            if ( !mSimulationModule.Prepare( lpAllocator, lSimulationParams ) )
                return false;
            // fall through
        }

        case E_PREPARESTAGE_DEFORMATIONMANAGER:
            mePrepareStage = E_PREPARESTAGE_DEFORMATIONMANAGER;        // *v11 = 4
            if ( !mDeformationManager.Prepare( lpAllocatorList->GetRWLinearResourceAllocator( 23 ) ) )
                return false;
            // ⛔ the fourteen deformation-IO clears go here -- see (a) in the banner.
            // fall through

        case E_PREPARESTAGE_PROPMANAGER:
            mePrepareStage = E_PREPARESTAGE_PROPMANAGER;               // *v11 = 5
            if ( !mPropManager.Prepare( lpAllocatorList->GetRWLinearResourceAllocator( 23 ) ) )
                return false;
            // fall through

        case E_PREPARESTAGE_VEHICLEMODULE:
            mePrepareStage = E_PREPARESTAGE_VEHICLEMODULE;             // *v11 = 6
            if ( !mVehicleManager.Prepare( lpAllocatorList->GetRWLinearResourceAllocator( 23 ),
                                           lpSceneInputBuffer ) )
                return false;
            // fall through

        case E_PREPARESTAGE_CREATE_PLAYER_VEHICLE:
            // No arm of its own -- see the banner. The console's `case 7` lands on LABEL_17.
            // fall through

        case E_PREPARESTAGE_CREATE_WORLD_RIGIDBODY:
            mePrepareStage = E_PREPARESTAGE_CREATE_WORLD_RIGIDBODY;    // *v11 = 8
            // ⛔ PrepareWorldRigidBody and its IO buffer go here -- see (c) in the banner.
            // fall through

        case E_PREPARESTAGE_DONE:
            mePrepareStage = E_PREPARESTAGE_DONE;                      // *v11 = 9
            // LABEL_19: the console immediately rewinds the cursor to MANAGER and clears the
            // release cursor, so a later Release() starts from its own stage 0.
            mePrepareStage = E_PREPARESTAGE_MANAGER;                   // *v11 = 1
            meReleaseStage = E_RELEASESTAGE_START;                     // +433076 = 0
            return true;
        }
    }

    // ================================================================================================
    // PhysicsModule::UpdateCachedPositions  @0x8259C370  (34 insns)
    // ⭐⭐ BODIED 2026-08-11 (lifetime wave). Its WorldLinkStubs boot gate is DELETED.
    //
    // WHY IT MATTERS, and it is not the 34 instructions: this is the ONLY writer of a triangle-cache
    // slot's SPHERE CENTRE anywhere in the XEX. Until it runs, all 28 claimed slots sit at the WORLD
    // ORIGIN, and the fill worker caches whatever geometry is near (0,0,0). A car at the Junkyard
    // would then be traction-tested against triangles three kilometres away -- valid, green, and
    // wrong. That is why it lands WITH the traction-line lifetime rather than after it.
    //
    // ⚠️ THREE ARMS, NOT SIX -- read off the asm at 0x8259C3BC / CC / DC, because three earlier logs
    // in this tree say six. The other three (PhysicalTrafficManager, DetachedPartManager,
    // DetachedWheelManager) are CALLEES of these, not siblings. The member offsets the console folds
    // into each `addi` are this tree's members by name and they check out exactly:
    //     this + 0x4AA0  == 19104  == mVehicleManager
    //     this + 0x63630 == 407088 == mPropManager
    //     this + 0x4CBA0 == 314272 == mDeformationManager
    //
    // ⚠️ AND ITS ONE CALLER IS WorldModule::Update @0x827D63E8, NOT PhysicsModule::Update -- `xrefs_to`
    // is a one-element set. It is already wired that way (BrnWorldModule.cpp, under LockForWrite).
    //
    // ⚠️ ARMS 2 AND 3 ARE NAMED GATES THIS WAVE (BrnPhysicsConductorGates.cpp). That is a matched
    // split, not a partial: the five per-manager producers write INDEPENDENT per-slot events into one
    // queue whose consumer is per-slot, and props/deformation own ZERO cache slots today
    // (usedSlots == 28 == 8 race cars + 20 traffic, runtime-witnessed). A manager that does not push
    // leaves its own unclaimed slots exactly where they already are.
    // ================================================================================================
    void PhysicsModule::UpdateCachedPositions(
        CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer )
    {
        CGS_ASSERT( lpSceneInputBuffer != NULL, "lpSceneInputBuffer_Update != NULL" );   // :617

        mVehicleManager.UpdateTriangleCache( lpSceneInputBuffer );
        mPropManager.UpdateTriangleCache( lpSceneInputBuffer );
        mDeformationManager.UpdateTriangleCache( lpSceneInputBuffer );
    }
}

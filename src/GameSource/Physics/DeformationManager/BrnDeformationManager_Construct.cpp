#include "GameSource/Physics/DeformationManager/BrnDeformationManager.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"   // CgsDev::PerfMonCpu::AddMonitor
#include "GameSource/Physics/DeformationManager/BrnDeformationDebugComponent.h"  // complete type for the static mDebugComponent

// ==================================================================================================
// BrnPhysics::Deformation::DeformationManager::Construct -- SPLIT OUT of BrnDeformationManager.cpp
// on 2026-08-03 (task #116). BUILD-MECHANICS SPLIT ONLY: the body and its banner comment were
// MOVED verbatim, not retyped or re-derived. Same precedent as RaceCarPhysics_Construct.cpp /
// TrafficPhysics_Construct.cpp / BrnVehicleManager_Construct.cpp.
//
// == WHY THE SPLIT ==
// BrnPhysics::PhysicsModule::Construct @0x825AE308 calls this function, and PhysicsModule::Construct
// was a LIVE EMPTY STUB in WorldLinkStubs.cpp -- a quiet no-op reached every boot. Un-stubbing it
// needs this symbol linkable. BrnDeformationManager.cpp AS A WHOLE cannot be mounted: a MEASURED
// trial link (task #116, M1) put it at 25 unresolved externals, of which 19 come from
// Prepare / Release / Destruct / OutputData / ProcessEvents / the Process*Events family -- pool
// allocation through the rw allocator, resource-handle resolution, the DetachedPartManager output
// path and the DeformableObject ctor. /OPT:REF does not suppress LNK2019, so the whole TU's
// reference graph is the cost.
//
// Construct's own share of that 25 was SIX symbols, all closed by BrnDeformationConstructShims.cpp:
//     DetachedPartManager::Construct
//     DeformationDebugComponent_Construct
//     DeformableObject_ConstructUpdatePerformanceMonitors
//     DeformableObject_ConstructUpdateIKAndLocatorsPerformanceMonitors
//     DeformableObject_ConstructPostPhysicsPerformanceMonitors
//     DeformationManager::mDebugComponent   (the static member had no definition anywhere)
//
// TO RE-MERGE: close the other 19, mount BrnDeformationManager.cpp, move this text back.
// ==================================================================================================

namespace BrnPhysics
{
namespace Deformation
{
    // The Construct memset loop bounds (duplicated from BrnDeformationManager.cpp; `static const`
    // so each TU keeps its own copy -- no ODR surface). Values are the frozen-header array extents.
    static const s32 KI_MAX_NUM_RACE_CARS        = 8;    // Vehicle::ku8MaxNumRaceCars
    static const s32 KI_MAX_NUM_PHYSICAL_TRAFFIC = 20;   // Vehicle::ku8TotalMaxNumPhysicalTraffic
    static const s32 KI_MAX_TOTAL_TRAFFIC        = 600;  // BrnTraffic::KU_MAX_TOTAL_TRAFFIC

    // DECLARE-ONLY cross-TU surface this body calls (same stand-ins as BrnDeformationManager.cpp
    // declares; see the banner there). Bodied by BrnDeformationConstructShims.cpp.
    void DeformationDebugComponent_Construct(DeformationDebugComponent* lpComponent,
                                             DeformationManager* lpManager);
    void DeformableObject_ConstructUpdatePerformanceMonitors();
    void DeformableObject_ConstructUpdateIKAndLocatorsPerformanceMonitors();
    void DeformableObject_ConstructPostPhysicsPerformanceMonitors();

    // -----------------------------------------------------------------------------------
    // Construct  @0x82621510   [EXECUTED in goal trace]
    //
    // One-time manager construction: build the static debug component, construct the embedded
    // detached-part pool, seed the private RNG, mark no models live + no player, clear the
    // race-car / traffic / global-traffic index tables to -1, and register the ~24 CPU
    // performance monitors in their exact order.
    // -----------------------------------------------------------------------------------
    void DeformationManager::Construct()
    {
        // The static debug component (constructed at the fixed address off_82F2A440, passed
        // `this`). Modelled via the file-scope-static mDebugComponent member.
        DeformationDebugComponent_Construct(&mDebugComponent, this);

        // The embedded detached-PART pool: 50 PhysicalBodyPart slots (the X360 inlines the pool
        // construct loop -- 50 iterations, 496-byte stride, then the pool count/ptr init). The
        // detached-WHEEL manager is NOT constructed here (the asm constructs only the part pool).
        mDetachedPartManager.Construct();

        // Seed the private RNG (the X360 inlines Random::Construct -- the 0x..1AD0891B default seed
        // + the 8-slot float ring fill + the index advance).
        mRandom.Construct();

        // Clear the per-model live-slot set (the X360 zeroes the BitArray field; done first, with
        // the RNG seed, in the asm).
        mModelsAdded.UnSetAll();

        // Index tables start fully unmapped (-1 == no model). The X360 memsets the three byte
        // tables (8 / 20 / 600 bytes -- the global-traffic table is sized 601 but the loop writes
        // 600; the trailing byte is initialised by the table's own clear elsewhere). Modelled as
        // the by-name table clears across each array's extent.
        for (s32 li = 0; li < KI_MAX_NUM_RACE_CARS; ++li)
            ma8RaceCarToModelIndex[li] = -1;
        for (s32 li = 0; li < KI_MAX_NUM_PHYSICAL_TRAFFIC; ++li)
            ma8TrafficToModelIndex[li] = -1;
        for (s32 li = 0; li < KI_MAX_TOTAL_TRAFFIC; ++li)
            ma8GlobalTrafficToModelIndex[li] = -1;

        // No live models, no player model yet, no part-IK round-robin progress.
        miNumUsedModels          = 0;   // X360 *(this+76036) = 0
        miPlayerModelIndex       = -1;  // X360 *(this+76040) = -1
        miLastBodyToHaveIKUpdate = 0;   // X360 *(this+76672) = 0

        // Register the CPU performance monitors in the X360 order. The X360 ARTIST build uses the
        // 6-parameter AddMonitor form (name, colour=15, minimum=0, budget=10.0, parentHandle, flags=1);
        // the parent-handle arg the X360 threaded is unrecovered per-call (register-allocated, no
        // symbol) so the parent linkage is passed as the unparented root (-1). FLAG: parent handles
        // unrecovered -- the registration order + names + colour/budget/flags are faithful.
        miTotalDeformationPerfMon                                = CgsDev::PerfMonCpu::AddMonitor("Total deformation",            15, 0, 10.0, -1, 1);
        miPostSceneUpdatePerfMon                                 = CgsDev::PerfMonCpu::AddMonitor("  Post scene update",          15, 0, 10.0, -1, 1);
        miUpdateSensorDisplPerfMon                               = CgsDev::PerfMonCpu::AddMonitor("  Update sensor displ.",       15, 0, 10.0, -1, 1);
        miUpdatePerfMon                                          = CgsDev::PerfMonCpu::AddMonitor("  Update",                     15, 0, 10.0, -1, 1);
        miUpdateModelsPerfMon                                    = CgsDev::PerfMonCpu::AddMonitor("     Update models",           15, 0, 10.0, -1, 1);
        DeformableObject_ConstructUpdatePerformanceMonitors();
        miUpdateIkAndDetachingPerfMon                            = CgsDev::PerfMonCpu::AddMonitor("     Upd.IK & detaching",      15, 0, 10.0, -1, 1);
        DeformableObject_ConstructUpdateIKAndLocatorsPerformanceMonitors();
        miUpdateDetachedPartsPerfMon                             = CgsDev::PerfMonCpu::AddMonitor("    Update detached parts",    15, 0, 10.0, -1, 1);
        miUpdateSkinnedJointsPerfMon                             = CgsDev::PerfMonCpu::AddMonitor("    Update skinned joints",    15, 0, 10.0, -1, 1);
        miUpdatePostPhysicsPerfMon                               = CgsDev::PerfMonCpu::AddMonitor("  Post physics update",        15, 0, 10.0, -1, 1);
        miPostPhysicsUpdateModelsPerfMon                         = CgsDev::PerfMonCpu::AddMonitor("     Update Models",           15, 0, 10.0, -1, 1);
        DeformableObject_ConstructPostPhysicsPerformanceMonitors();
        miPostPhysicsUpdateDetachedPartsManPerfMon               = CgsDev::PerfMonCpu::AddMonitor("     Update Detached Parts Man", 15, 0, 10.0, -1, 1);
        miPostPhysicsProcessJointSpiesPerfMon                    = CgsDev::PerfMonCpu::AddMonitor("     Process Joint Spies",     15, 0, 10.0, -1, 1);
        miPostPhysicsUpdateAddContactsToPenSolverPerfMon         = CgsDev::PerfMonCpu::AddMonitor("       Add conts to PenSlvr",  15, 0, 10.0, -1, 1);
        miPostPhysicsUpdateSolvePenetrationPerfMon               = CgsDev::PerfMonCpu::AddMonitor("       Solve Penetrations",    15, 0, 10.0, -1, 1);
        miPostPhysicsUpdateReadTransformsFromPenSolverPerfMon    = CgsDev::PerfMonCpu::AddMonitor("       Read from Pen Solver",  15, 0, 10.0, -1, 1);
        miFixUpRaceCarTrafficContact                             = CgsDev::PerfMonCpu::AddMonitor("Fixup RaceCar Traf contact",   15, 0, 10.0, -1, 1);

        // (X360: a one-shot file-static init guard `if (dword_82F2A338 == -1) dword_82F2A338 = 0;`
        // -- the debug component's registration latch. Modelled inside the debug component's own
        // home; no observable state on the manager.)
    }
}
}

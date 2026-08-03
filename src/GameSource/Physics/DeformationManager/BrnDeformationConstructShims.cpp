#include "GameSource/Physics/DeformationManager/BrnDeformationManager.h"
#include "GameSource/Physics/DeformationManager/BrnDeformationDebugComponent.h"
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDetachedPartManager.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// ==================================================================================================
// BrnDeformationConstructShims.cpp -- the link closure of DeformationManager::Construct @0x82621510,
// and nothing else. NEW on 2026-08-03 (task #116).
//
// == WHY THIS TU EXISTS ==
// BrnPhysics::PhysicsModule::Construct @0x825AE308 had been a LIVE EMPTY STUB in WorldLinkStubs.cpp
// since 2026-07-26 -- a quiet no-op reached every boot by the WorldModule::Construct cascade, so
// nothing in the physics module was ever constructed. Un-stubbing it requires its ten X360 callees
// to link; nine were already reachable, and the tenth (DeformationManager::Construct) needed the
// six symbols below. Each one already had a home TU that cannot yet be mounted, so this TU is the
// SPLIT of exactly those six -- the same precedent as RaceCarPhysics_Construct.cpp /
// TrafficPhysics_Construct.cpp / BrnVehicleManager_Construct.cpp.
//
//   1. DeformationManager::mDebugComponent  -- the static member had NO definition anywhere in the
//      tree. Not a split: a genuine gap.
//   2. DeformationDebugComponent_Construct  -- the declare-only stand-in BrnDeformationManager.cpp
//      calls; forwards to the real DeformationDebugComponent::Construct.
//   3-5. The three DeformableObject perfmon registrations, MOVED verbatim out of
//      BrnDeformableObject_Lifecycle.cpp (which cannot be mounted: its Update/Render spine is a much
//      larger closure). Their ten si* handles moved with them and are now EXTERNAL-linkage globals
//      with exactly ONE definition -- see the warning below.
//   6. DetachedPartManager::Construct       -- one line, and it had no body anywhere.
// ==================================================================================================

namespace BrnPhysics
{
namespace Deformation
{
    // ----------------------------------------------------------------------------------------------
    // (1) The static debug component.
    //
    // BrnDeformationManager.h declares `static DeformationDebugComponent mDebugComponent;`. The X360
    // reaches it as the fixed .bss address off_82F2A440 and passes its address to
    // DeformationDebugComponent::Construct with `this`. It had no out-of-line definition anywhere in
    // the tree, so every TU that touched it was an unresolved external. This is that definition.
    // ----------------------------------------------------------------------------------------------
    DeformationDebugComponent DeformationManager::mDebugComponent;

    // ----------------------------------------------------------------------------------------------
    // kbAllowDeformationDebug (X360 .bss byte). OnSelectedRigChange SETS it; BrnRaceCarEntityModule_
    // Render.cpp READS it to gate the deformation verlet block. Every TU in the tree that touches it
    // declares it `extern` -- and NO TU defined it, so it was an unresolved external the moment any
    // of them was mounted. Its documented home is the deformation-physics group
    // (BrnDeformableObject_*.cpp, all still unmounted); it is defined here because this TU is the one
    // that had to exist anyway.
    // ⚠️ WHEN a BrnDeformableObject_*.cpp lands and defines it, DELETE THIS LINE (duplicate symbol).
    // Seeded false: the X360 .bss default, and OnSelectedRigChange is the only writer.
    // ----------------------------------------------------------------------------------------------
    bool kbAllowDeformationDebug = false;

    // ----------------------------------------------------------------------------------------------
    // (3-5) Module-scope perf-monitor handle ids (asm .bss dword_82F2A348..0x36C).
    //
    // ⚠️⚠️ THESE ARE DELIBERATELY EXTERNAL-LINKAGE, AND THIS IS THEIR ONLY DEFINITION.
    // They used to be `static s32` inside BrnDeformableObject_Lifecycle.cpp, which is where the three
    // registration functions below also lived. Splitting the registrations out while leaving the
    // handles `static` would have produced TWO independent copies: the shim would register into its
    // copy and Lifecycle's thirty-three read sites would keep reading -1 forever -- a textbook
    // silent-drop stub that compiles, links, boots and reports ready. Lifecycle.cpp now `extern`-
    // declares them instead of defining them, so there is exactly one set of handles either way.
    // Names are verbatim from the asm FireAssert expression strings. NOT DeformableObject members.
    // ----------------------------------------------------------------------------------------------
    s32 siSortContactsPerfMon         = -1;   // dword_82F2A348
    s32 siSolveContactsPerfMon        = -1;   // dword_82F2A34C
    s32 siUpdateWheelsAndGlassPerfMon = -1;   // dword_82F2A350
    s32 siUpdateSweptSpherePerfMon    = -1;   // dword_82F2A354
    s32 siUpdateWorldSpheres          = -1;   // dword_82F2A358
    s32 siCheckDetaching              = -1;   // dword_82F2A35C
    s32 siUpdateSkinningOffsets       = -1;   // dword_82F2A360
    s32 siUpdateIK                    = -1;   // dword_82F2A364
    s32 siUpdateSuspensionIK          = -1;   // dword_82F2A368
    s32 siUpdateLocators              = -1;   // dword_82F2A36C

    // ----------------------------------------------------------------------------------------------
    // (2) DeformationDebugComponent_Construct -- the declare-only stand-in that
    // BrnDeformationManager.cpp / BrnDeformationManager_Construct.cpp call by name.
    //
    // X360: `DeformationDebugComponent::Construct(&off_82F2A440, this)`. The stand-in exists because
    // BrnDeformationManager.cpp only forward-declares the component type; this TU includes the real
    // header, so the forward is a one-line call.
    // ----------------------------------------------------------------------------------------------
    void DeformationDebugComponent_Construct(DeformationDebugComponent* lpComponent,
                                             DeformationManager* lpManager)
    {
        lpComponent->Construct(lpManager);
    }

    // ==============================================================================================
    // (3) ConstructUpdatePerformanceMonitors @ 0x825B99A0  (87 instructions)
    //   Register the four per-frame Update-stage CPU perf monitors (Sort contacts, Solve contacts,
    //   Upd. Suspension IK, Upd. Locators), each asserting the returned handle id is >= 0. The
    //   AddMonitor args (group 15, parent 0, budget 10.0 ms, the per-call cookie, enabled 1) are
    //   verbatim from the asm.
    //
    // MOVED VERBATIM from BrnDeformableObject_Lifecycle.cpp:759. The X360 calls all three of these
    // with NO object (r3 is never seated -- Hex-Rays' `a1` is garbage), i.e. they are effectively
    // static; BrnDeformableObject.h declares them as instance methods and BrnDeformationManager.cpp
    // therefore reaches them through free-function stand-ins. That FLAG is unchanged by this split:
    // the bodies below ARE the stand-ins, and they touch no `this`.
    // ==============================================================================================
    void DeformableObject_ConstructUpdatePerformanceMonitors()
    {
        siSortContactsPerfMon = CgsDev::PerfMonCpu::AddMonitor("          Sort contacts", 15, 0, 10.0, 0, 1);
        CGS_ASSERT(siSortContactsPerfMon >= 0, "siSortContactsPerfMon >= 0");

        siSolveContactsPerfMon = CgsDev::PerfMonCpu::AddMonitor("          Solve contacts", 15, 0, 10.0, 0, 1);
        CGS_ASSERT(siSolveContactsPerfMon >= 0, "siSolveContactsPerfMon >= 0");

        siUpdateSuspensionIK = CgsDev::PerfMonCpu::AddMonitor("          Upd. Suspension IK", 15, 0, 10.0, 0, 1);
        CGS_ASSERT(siUpdateSuspensionIK >= 0, "siUpdateSuspensionIK >= 0");

        siUpdateLocators = CgsDev::PerfMonCpu::AddMonitor("          Upd. Locators", 15, 0, 10.0, 0, 1);
        CGS_ASSERT(siUpdateLocators >= 0, "siUpdateLocators >= 0");
    }

    // ==============================================================================================
    // (4) ConstructUpdateIKAndLocatorsPerformanceMonitors @ 0x825B9B00  (98 instructions)
    //   Register the IK/locators-stage perf monitors. NOTE the asm asserts siSolveContactsPerfMon >= 0
    //   FIRST (it was registered by ConstructUpdatePerformanceMonitors; this is a dependency tripwire,
    //   it does NOT register it), then adds Check Detaching, Upd. Skinning Offs, Upd. IK, and
    //   Upd. wheels & glass.
    // MOVED VERBATIM from BrnDeformableObject_Lifecycle.cpp:781.
    // ==============================================================================================
    void DeformableObject_ConstructUpdateIKAndLocatorsPerformanceMonitors()
    {
        // Dependency tripwire (asm: tests the already-registered solve-contacts handle, no AddMonitor).
        CGS_ASSERT(siSolveContactsPerfMon >= 0, "siSolveContactsPerfMon >= 0");

        siCheckDetaching = CgsDev::PerfMonCpu::AddMonitor("          Check Detaching", 15, 0, 10.0, 0, 1);
        CGS_ASSERT(siCheckDetaching >= 0, "siCheckDetaching >= 0");

        siUpdateSkinningOffsets = CgsDev::PerfMonCpu::AddMonitor("          Upd. Skinning Offs", 15, 0, 10.0, 0, 1);
        CGS_ASSERT(siUpdateSkinningOffsets >= 0, "siUpdateSkinningOffsets >= 0");

        siUpdateIK = CgsDev::PerfMonCpu::AddMonitor("          Upd. IK", 15, 0, 10.0, 0, 1);
        CGS_ASSERT(siUpdateIK >= 0, "siUpdateIK >= 0");

        siUpdateWheelsAndGlassPerfMon = CgsDev::PerfMonCpu::AddMonitor("          Upd.wheels & glass", 15, 0, 10.0, 0, 1);
        CGS_ASSERT(siUpdateWheelsAndGlassPerfMon >= 0, "siUpdateWheelsAndGlassPerfMon >= 0");
    }

    // ==============================================================================================
    // (5) ConstructPostPhysicsPerformanceMonitors @ 0x825B9C88  (51 instructions)
    //   Register the two post-physics sphere-update monitors.
    // MOVED VERBATIM from BrnDeformableObject_Lifecycle.cpp:805.
    // ==============================================================================================
    void DeformableObject_ConstructPostPhysicsPerformanceMonitors()
    {
        siUpdateWorldSpheres = CgsDev::PerfMonCpu::AddMonitor("        Update World Spheres", 15, 0, 10.0, 0, 1);
        CGS_ASSERT(siUpdateWorldSpheres >= 0, "siUpdateWorldSpheres >= 0");

        siUpdateSweptSpherePerfMon = CgsDev::PerfMonCpu::AddMonitor("        Update Swept Spheres", 15, 0, 10.0, 0, 1);
        CGS_ASSERT(siUpdateSweptSpherePerfMon >= 0, "siUpdateSweptSpherePerfMon >= 0");
    }

    // ==============================================================================================
    // (6) DetachedPartManager::Construct
    //
    // The X360 INLINES this inside DeformationManager::Construct @0x82621510 -- the 50-iteration,
    // 496-byte-stride PhysicalBodyPart::Construct loop plus the pool's count/cursor init, which is
    // exactly PhysicalBodyPartPool::Construct (BrnPhysicalBodyPartPool.cpp:70). DetachedPartManager
    // has exactly ONE data member (mPartPool, BrnDetachedPartManager.h:136), so the whole body is
    // that one forward. DWARF BrnDetachedPartManager.h:51.
    // ==============================================================================================
    void DetachedPartManager::Construct()
    {
        mPartPool.Construct();
    }
}
}

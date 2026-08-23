// ===========================================================================
// CgsSceneManager::SceneManagerModule + CullingGroupManager::CreateCullingTable
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, named members):
//   CullingGroupManager::CreateCullingTable                     @ 0x828BAB48
//   SceneManagerModule::Construct                               @ 0x828D09A0
//   SceneManagerModule::Destruct                               @ 0x828D1640
//   SceneManagerModule::Prepare                                @ 0x828D13E0
//   SceneManagerModule::CreateCullingTable                     @ 0x828BAC90
//   SceneManagerModule::EndUpdateTriangleCache                 @ 0x828C7500
//   SceneManagerModule::ProcessSetVolumeInstanceCullingGroupEvent @ 0x828CF8E8
//   SceneManagerModule::UpdateContactGeneration                @ 0x828D5CA0
//   SceneManagerModule::ProcessFrustumTestJobResults           @ 0x828C7838
//
// Behaviour-faithful to the X360 asm: every store / side-effect / early-out is
// accounted for; member access is by NAME (the asm's huge byte offsets are the
// X360 layout -- on the x64 PC compile the layout differs, per the project's
// semantic-parity rule). Decompiler temporaries / gotos are removed; the repeated
// "register perfmon if not yet registered" and "start/stop perfmon" idioms are
// de-optimised into a helper + a scoped guard.
// ===========================================================================

#include "GameShared/GameClasses/SceneManager/CgsSceneManagerModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/GameClasses/SceneManager/ContactGen/CgsContactGenerationIO.h"
#include "GameShared/GameClasses/SceneManager/ContactGen/CgsOverlapGenerationModule.h"  // OverlapGenerationIO::InputBuffer / InAddBodyEvent (AddBody)
#include "GameShared/GameClasses/Module/CgsIOBufferStack.h"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // [DIAG culling wave] CgsDev::Log::gpDebugPrint
#include "rw/rwcore_structs.h"   // rw::IResourceAllocator / rw::Resource / rw::ResourceDescriptor
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO.h"                                    // SceneManagerIO::InputBuffer_Update / OutputBuffer / OutCoarseQueryResult
#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/CgsSpatialPartitionManagerIO.h"  // SpatialPartitionIO::InputBuffer_Update / OutputBuffer
#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/SpatialPartitions/CgsLooseOctree.h" // LooseOctree (the frustum-test entry points)

// ---- wave Q5 / E1a (2026-08-19): the volume-collision drain legs ------------------------
#include "GameShared/GameClasses/SceneManager/ContactGen/CgsOverlapGenerationModuleIO.h"  // OverlapGenerationIO::InputBuffer (AddBody/RemoveBody producers) + AABBoxRows
#include "GameShared/GameClasses/SceneManager/ContactGen/CgsSceneSweeper.h"               // SceneSweeper::siUpdate_*PerfMon (the perfmon fork repair)
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstance.h"                        // VolumeInstance (mWorldSpaceTransform / miVolumeIndex)
#include "SDKs/EATech/rwcollision/volume_debug_access.h"                                  // rw::collision::Volume::GetBBox (through the per-type descriptor)
#include "rw/math/fpu/scalar_operation.h"                                                 // rw::math::fpu::IsZero (the box tripwires' epsilon)
#include "rw/math/vpu/vector3_operation.h"                                                // rw::math::vpu::Clamp (the transform leg's padding clamp)

#include <stdlib.h>   // getenv (BRN_PROP_DIAG, host-only bring-up diagnostic)

// rw::collision::AABBox is only NAMED in this TU (GetBBox's out-parameter and AddBody's
// pointer parameter). Its real home, vendor/renderware/collision/AABBox.hpp, pulls the SDK
// rw::math::vpu::Vector3 CLASS into a TU that already has the vendor 4-lane POD via
// BrnCommonTypes.h, and the two cannot coexist (measured; the sibling CgsVolumeManager.cpp:54-77
// and PropManager_wQ4_03.cpp:289-301 route around it the same way). What this TU needs is only
// the byte image -- two 16-byte float4 rows, mMin @+0x00 / mMax @+0x10 -- which already has a
// named home here as OverlapGenerationIO::AABBoxRows, pinned by the MOUNTED layout oracle
// PropManager_wQ4_03_embed_check.cpp (it CAN include AABBox.hpp and static_asserts
// sizeof==32 / offsetof(mMin)==0 / offsetof(mMax)==16). A change to AABBox breaks that gate,
// not this reader.
namespace rw { namespace collision { class AABBox; } }

// ---------------------------------------------------------------------------
// rw::collision::Volume::InitializeVTable -- the X360 Construct lazily fills the shared
// per-type volume descriptor table once at scene-manager construction.
//
// ⭐ FORK RETIRED 2026-08-19 (wave Q5 / E1a). A four-line local `class Volume { static int
// InitializeVTable(); };` used to stand here, with a banner saying "there is no shared
// rw::collision::Volume header". There is one now -- SDKs/EATech/rwcollision/volume_debug_access.h,
// included above -- and this TU needs its GetBBox anyway, so the local re-declaration was
// both a gotcha-7 type fork and an outright C2011 against the real home. The real class
// spells the entry point `static RwBool InitializeVTable()` where RwBool is s32, so the
// mangled symbol the caller at Construct binds to is UNCHANGED by this deletion.
// ---------------------------------------------------------------------------

namespace CgsSceneManager
{

// ---------------------------------------------------------------------------
// File-static perfmon handles. The X360 stores each registered monitor's handle in
// a file-scope `si*PerfMon` int (DWARF CgsSceneManagerModule.cpp:50-86), initialised
// to -1 ("not yet registered"). Construct lazily registers each one once.
// ---------------------------------------------------------------------------
static s32 siContactGen_UpdatePerfMon                = -1;
static s32 siContactGen_GenerateOverlapsPerfMon      = -1;
// ⭐ FORK REPAIRED 2026-08-19 (wave Q5 / E1a; round-2 sweep3 finding D3).
// Three file-local `siSceneSweeper_{SortLists,SweepLists,BuildCollidingPairs}PerfMon`
// statics used to sit here, and Construct registered into THEM while carrying the
// console's own "SceneSweeper::siUpdate_*PerfMon >= 0" assert strings. Those strings are
// the tell: the console's ids are SceneSweeper's OWN class statics
// (CgsSceneSweeper.h:297-299 / .cpp:70-72, X360 dword_82F33F54/_58/_5C), which is where
// SceneSweeper::Update @0x828D5A88 reads them from. With the fork in place the canonical
// statics stayed at their -1 image forever, so the sweeper's three per-phase perfmon
// brackets were permanently dead in this tree while the console's were live -- and the
// registrar's own asserts could never catch it, because they tested the forked copies.
// Not a link duplicate (different mangled symbols), so there was no gate to retire and
// nothing crashed; the repair is that Construct now registers into the real statics.
static s32 siContactGen_CullOverlapsPerfMon          = -1;
static s32 siContactGen_IOBuffersPerfMon             = -1;
static s32 siContactGen_BridgesPerfMon               = -1;
static s32 siTriCache_UpdatePerfMon                  = -1;
static s32 siTriCache_CachedObj_UpdatePerfMon        = -1;
static s32 siTriCache_SetupFillTriangleCachePerfMon  = -1;
static s32 siTriCache_FillTriangleCachePerfMon       = -1;
static s32 siTriCache_ProcessCollisionResultsPerfMon = -1;
static s32 siUpdateScenePerfMon                      = -1;
static s32 siProcessSceneQueriesPerfMon              = -1;
static s32 siProcessCoarseQueriesPerfMon             = -1;
static s32 siProcessFineQueriesPerfMon               = -1;
static s32 siProcessFineQueriesLineFinePerfMon       = -1;
static s32 siProcessFineQueriesLineNearPerfMon       = -1;
static s32 siProcessFineQueriesLineFastDSPerfMon     = -1;
static s32 siProcessFineQueriesSphereFastPerfMon     = -1;
static s32 siProcessFineQueriesVolFinePerfMon        = -1;
static s32 siProcessFineQueriesVolNearPerfMon        = -1;
static s32 siProcessTriCollisionLineTestsPerfMon     = -1;
static s32 siProcessTriCollisionLineTestNearestsPerfMon = -1;

// ---------------------------------------------------------------------------
// The swept-motion padding clamp the volume-instance TRANSFORM leg applies to the
// translation delta before it becomes a broad-phase padding
// (BridgeInputSceneUpdateInterfaceToSubModules leg 18).
//
// MEASURED, not inferred: the console splats two rodata floats into the three xyz lanes
// of two stack vectors and leaves the w lane at the stack zero
// (0x828D3530 `lfs f31, flt_82006D70`, 0x828D3554 `lfs f0, flt_82001D9C`, then
//  0x828D3570/0x828D3578/0x828D3580 store f31 and 0x828D355C/0x828D3560/0x828D3568 store f0,
//  with 0x828D3584 / 0x828D357C zeroing the two w lanes), then does
//  `vmaxfp128 v0, delta, minVec` followed by `vminfp v1, v0, maxVec` -- i.e. exactly
//  rw::math::vpu::Clamp. Both words were read out of the shipped image with headless IDA on
//  a private .i64 copy: 0x82006D70 == 0xC0000000 == -2.0f, 0x82001D9C == 0x40000000 == +2.0f
//  (big-endian). The zero w lanes are why the clamped padding's w lane is always 0.
// ---------------------------------------------------------------------------
static const Vector3 KV_MIN_SWEPT_PADDING = { -2.0f, -2.0f, -2.0f, 0.0f };
static const Vector3 KV_MAX_SWEPT_PADDING = {  2.0f,  2.0f,  2.0f, 0.0f };

// The source path the X360 asserts cite (the build's absolute file path).
static const char* const KPC_SOURCE_FILE =
    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\scenemanager\\CgsSceneManagerModule.cpp";

namespace
{
    // Register one perfmon monitor the first time we see it (handle still -1) and
    // assert it registered. De-optimises the ~25 identical Construct blocks. The
    // X360 6-arg AddMonitor(name, colour, minimum, budgetMs, parentHandle, flags);
    // the parent handle was an uninitialised register at the X360 call sites (never
    // a meaningful nest), so it is passed as -1 here.
    inline void RegisterPerfMonOnce(s32& lriHandle, const char* lpcName, s32 liColour,
                                    f32 lfBudgetMs, s32 liFlags, const char* lpcAssertExpr)
    {
        if (lriHandle == -1)
        {
            lriHandle = CgsDev::PerfMonCpu::AddMonitor(lpcName, liColour, 0, lfBudgetMs, -1, liFlags);
            CGS_ASSERT(lriHandle >= 0, lpcAssertExpr);
        }
    }

    // Start a perfmon region if its handle is valid; stop it on scope exit. Replaces
    // the repeated `if (handle > -1) StartMonitor(handle) ... StopMonitor(handle)`.
    class ScopedPerfMon
    {
    public:
        explicit ScopedPerfMon(s32 liHandle) : miHandle(liHandle)
        {
            if (miHandle > -1)
                CgsDev::PerfMonCpu::StartMonitor(miHandle);
        }
        ~ScopedPerfMon()
        {
            if (miHandle > -1)
                CgsDev::PerfMonCpu::StopMonitor(miHandle);
        }
    private:
        s32 miHandle;
        ScopedPerfMon(const ScopedPerfMon&);
        ScopedPerfMon& operator=(const ScopedPerfMon&);
    };

    // [DIAG] NOT IN THE X360 BINARY. Opt-in wave-Q5 bring-up probe (set BRN_PROP_DIAG),
    // same latch pattern as the prop module's: getenv is read ONCE into a function-local
    // static, never per frame and never per event. Off by default; when off this is one
    // predictable branch per call site.
    inline bool Q5DiagEnabled()
    {
        static const bool sbEnabled = (getenv("BRN_PROP_DIAG") != 0);
        return sbEnabled && CgsDev::Log::gpDebugPrint != 0;
    }
}

// ===========================================================================
// CullingGroupManager::CreateCullingTable @ 0x828BAB48
//
// Allocate the culling-table backing store through the scene resource allocator and
// initialise it as a 24x24 BitTable (= 84 bytes: 12-byte header + 18 packed words),
// then clear every bit. The X360 builds the rw resource descriptor inline (size 0x54
// = 84, alignment 4), allocates through the allocator's descriptor-allocate vtable
// slot, then BitTable::Initialize's the result. Re-expressed via the named rw types.
// ===========================================================================
void CullingGroupManager::CreateCullingTable(rw::IResourceAllocator* lpSceneAllocator)
{
    CGS_ASSERT(lpSceneAllocator != NULL, "lpSceneAllocator != NULL");

    // Memory requirement for the 24x24 grid (the X360's inline 0x54/align-4 block).
    rw::BaseResourceDescriptor lDescriptor;
    rw::BitTable::GetResourceDescriptor(&lDescriptor, 24, 24);

    // Allocate the backing store through the scene resource allocator and initialise
    // the bit grid in it. m_baseResources[0] is the allocated buffer base.
    rw::ResourceDescriptor lResourceDesc;
    lResourceDesc.m_baseResourceDescriptors[0] = lDescriptor;
    rw::Resource lResource = lpSceneAllocator->DoAllocate(lResourceDesc, "CullingTable");

    rw::BitTable::Storage* lpStorage =
        static_cast<rw::BitTable::Storage*>(lResource.m_baseResources[0]);
    mpCullingTable = rw::BitTable::Initialize(&lpStorage, 24, 24);

    // Clear every packed word (the asm's zeroing loop over muWordCount).
    if (mpCullingTable != nullptr)
    {
        for (u32 luWord = 0; luWord < mpCullingTable->muWordCount; ++luWord)
            mpCullingTable->maBits[luWord] = 0;
    }
}

// ===========================================================================
// SceneManagerModule::CreateCullingTable @ 0x828BAC90
//
// Assert the allocator, then forward to the embedded culling-group manager.
// ===========================================================================
void SceneManagerModule::CreateCullingTable(rw::IResourceAllocator* lpSceneAllocator)
{
    CGS_ASSERT(lpSceneAllocator != NULL, "lpSceneAllocator != NULL");
    mCullingGroupManager.CreateCullingTable(lpSceneAllocator);
}

// ===========================================================================
// SceneManagerModule::Construct @ 0x828D09A0  (EXECUTED in boot trace)
//
// Construct the base single-buffered module, reset the prepare/release progression,
// zero the per-sub-manager scratch regions, construct each embedded sub-manager and
// the debug components, and lazily register every CPU perfmon monitor. Finally mark
// the module "new" (base flag) and clear the cached collision-generator pointer.
// ===========================================================================
void SceneManagerModule::Construct()
{
    ModuleSingleBuffered::Construct();

    // Construct leaves prepare at START and release already DONE.
    mePrepareStage = E_SCENEMANAGER_PREPARE_START;     // X360 [+0x268] <- 0
    meReleaseStage = E_SCENEMANAGER_RELEASE_DONE;      // X360 [+0x26C] <- 10

    rw::collision::Volume::InitializeVTable();

    // ⭐ LANDED 2026-08-19 (wave Q5 round-3 integration; the first drive with the volume
    // legs live died in EntityManager::AddVolumeInstance -> VolumeManager::GetVolumeIndexByID
    // with this == NULL). The console's four memsets at 0x828D09CC..0x828D0A28 ARE these
    // two Constructs, inlined: memset(mVolumeManager+0x94808, 0, 0x240) + memset(+0xC1038,
    // 0, 0x278) == mVolumeManager.Construct() (VolumeStore + volume pool), memset(
    // mEntityManager+0x27108, 0, 0x4E8) + memset(+0xCA0E8, 0, 0x278) == the two
    // EntityManager pools, and `stwx r30, r28, 0x125300` @0x828D0A3C stores &mVolumeManager
    // into EntityManager::mpVolumeManager -- the binding EntityManager::Construct's sole
    // parameter exists for (CgsEntityManager.cpp's own banner asked for exactly this line).
    mVolumeManager.Construct();
    mEntityManager.Construct(&mVolumeManager);

    // Construct the embedded sub-managers in declaration order. Construct() on each is
    // the X360's per-region memset + sub-object construction (the asm zero-fills the
    // entity / volume / triangle-collision / cache scratch regions then runs each
    // sub-manager's Construct / vtable construct slot).
    mSpatialPartitionManager.Construct();
    mOverlapGenerator.Construct();          // X360 (**[+0x290])([+0x290])  vtable slot 0
    mOverlapCuller.Construct();             // X360 (**[+0xDC9A0])([+0xDC9A0])
    mFineIntersectionTestModule.Construct();

    mpTriangleCacheCollisionGenerator = nullptr;   // X360 [+0x3A84D0] <- 0

    // The two "total CG setup" timers are registered unconditionally each Construct
    // (the X360 stores the handle straight into the member; not the lazy si* pattern).
    miTimeInCachedContactGen    = CgsDev::PerfMonCpu::AddMonitor("Total cached CG setup", 4, 0, 10.0, -1, 1);
    miTimeInNonCachedContactGen = CgsDev::PerfMonCpu::AddMonitor("Total non cached  CG setup", 4, 0, 10.0, -1, 1);

    // X360: mSceneManagerDebugComponent.Construct(this); DebugComponent::Register(&it).
    // The debug component is modelled as an opaque tail member here (its home header does
    // not yet compile against the vendor rw headers -- see the module header). Its
    // construct + debug-overlay registration are owned by that component's own TU; they
    // are non-load-bearing for the module's scene behaviour and are routed there once the
    // home compiles.

    // Lazily register every contact-generation / triangle-cache / query CPU monitor.
    // Colour 16 = the contact-generation page; colour 17 = the query page.
    RegisterPerfMonOnce(siContactGen_UpdatePerfMon, "UpdateContactGeneration", 16, 10.0f, 0,
                        "siContactGen_UpdatePerfMon >= 0");
    RegisterPerfMonOnce(siContactGen_GenerateOverlapsPerfMon, "   GenerateOverlaps", 16, 10.0f, 0,
                        "siContactGen_GenerateOverlapsPerfMon >= 0");
    // The three SWEEPER phase monitors register into SceneSweeper's own class statics --
    // the ids SceneSweeper::Update @0x828D5A88 actually reads (see the fork note above).
    RegisterPerfMonOnce(SceneSweeper::siUpdate_SortListsPerfMon, "      SortLists", 16, 10.0f, 0,
                        "SceneSweeper::siUpdate_SortListsPerfMon >= 0");
    RegisterPerfMonOnce(SceneSweeper::siUpdate_SweepListsPerfMon, "      SweepLists", 16, 10.0f, 0,
                        "SceneSweeper::siUpdate_SweepListsPerfMon >= 0");
    RegisterPerfMonOnce(SceneSweeper::siUpdate_BuildCollidingPairsPerfMon, "      BuildColPairs", 16, 10.0f, 0,
                        "SceneSweeper::siUpdate_BuildCollidingPairsPerfMon >= 0");
    RegisterPerfMonOnce(siContactGen_CullOverlapsPerfMon, "   CullOverlaps", 16, 10.0f, 0,
                        "siContactGen_CullOverlapsPerfMon >= 0");
    RegisterPerfMonOnce(siContactGen_IOBuffersPerfMon, "   IOBuffers", 16, 10.0f, 0,
                        "siContactGen_IOBuffersPerfMon >= 0");
    RegisterPerfMonOnce(siContactGen_BridgesPerfMon, "   Bridges", 16, 10.0f, 0,
                        "siContactGen_BridgesPerfMon >= 0");
    RegisterPerfMonOnce(siTriCache_UpdatePerfMon, "UpdateTriCacheManager", 16, 10.0f, 0,
                        "siTriCache_UpdatePerfMon >= 0");
    RegisterPerfMonOnce(siTriCache_CachedObj_UpdatePerfMon, "   UpdateCachedObjects", 16, 5.0f, 0,
                        "siTriCache_CachedObj_UpdatePerfMon >= 0");
    RegisterPerfMonOnce(siTriCache_SetupFillTriangleCachePerfMon, "      SetupFillTriCache", 16, 3.0f, 0,
                        "TriangleCacheManager::siSetupFillTriangleCachePerfMon >= 0");
    RegisterPerfMonOnce(siTriCache_FillTriangleCachePerfMon, "      FillTriCache", 16, 3.0f, 0,
                        "TriangleCacheManager::siFillTriangleCachePerfMon >= 0");
    RegisterPerfMonOnce(siTriCache_ProcessCollisionResultsPerfMon, "      ProcessColResults", 16, 3.0f, 0,
                        "TriangleCacheManager::siProcessCollisionResultsPerfMon >= 0");
    RegisterPerfMonOnce(siUpdateScenePerfMon, "[SM] UpdateScene", 17, 15.0f, 0,
                        "siUpdateScenePerfMon >= 0");
    RegisterPerfMonOnce(siProcessSceneQueriesPerfMon, "[SM] ProcessSceneQueries", 17, 15.0f, 0,
                        "siProcessSceneQueriesPerfMon >= 0");
    RegisterPerfMonOnce(siProcessCoarseQueriesPerfMon, "       Coarse", 17, 15.0f, 0,
                        "siProcessCoarseQueriesPerfMon >= 0");
    RegisterPerfMonOnce(siProcessFineQueriesPerfMon, "       Fine", 17, 15.0f, 0,
                        "siProcessFineQueriesPerfMon >= 0");
    RegisterPerfMonOnce(siProcessFineQueriesLineFinePerfMon, "           LineFine", 17, 15.0f, 0,
                        "siProcessFineQueriesLineFinePerfMon >= 0");
    RegisterPerfMonOnce(siProcessFineQueriesLineNearPerfMon, "           LineNear", 17, 15.0f, 0,
                        "siProcessFineQueriesLineNearPerfMon >= 0");
    RegisterPerfMonOnce(siProcessFineQueriesLineFastDSPerfMon, "           LineFastDS", 17, 15.0f, 0,
                        "siProcessFineQueriesLineFastDSPerfMon >= 0");
    RegisterPerfMonOnce(siProcessFineQueriesSphereFastPerfMon, "           SphereFast", 17, 15.0f, 0,
                        "siProcessFineQueriesSphereFastPerfMon >= 0");
    RegisterPerfMonOnce(siProcessFineQueriesVolFinePerfMon, "           VolFine", 17, 15.0f, 0,
                        "siProcessFineQueriesVolFinePerfMon >= 0");
    RegisterPerfMonOnce(siProcessFineQueriesVolNearPerfMon, "           VolNear", 17, 15.0f, 0,
                        "siProcessFineQueriesVolNearPerfMon >= 0");
    RegisterPerfMonOnce(siProcessTriCollisionLineTestsPerfMon, "       TriCollLTs", 17, 15.0f, 0,
                        "siProcessTriCollisionLineTestsPerfMon >= 0");
    RegisterPerfMonOnce(siProcessTriCollisionLineTestNearestsPerfMon, "       TriCollLTNs", 17, 15.0f, 0,
                        "siProcessTriCollisionLineTestNearestsPerfMon >= 0");

    // Mark the module "new" (base flag at [+4]) and clear the per-frame cached
    // collision-generator pointer (X360 [+0x3A84D0] <- 0).
    mbIsNewModule = true;
    mpTriangleCacheCollisionGenerator = nullptr;
}

// ===========================================================================
// SceneManagerModule::Destruct @ 0x828D1640
//
// Destruct the scene-graph (overlap-culling) and overlap-generation modules via
// their vtable destruct slot, zero the sub-manager scratch regions, then destruct
// the base single-buffered module.
// ===========================================================================
void SceneManagerModule::Destruct()
{
    mOverlapCuller.Destruct();      // X360 (*(*[+0xDC9A0]+12))([+0xDC9A0])  vtable destruct slot
    mOverlapGenerator.Destruct();   // X360 (*(*[+0x290]+12))([+0x290])      vtable destruct slot

    ModuleSingleBuffered::Destruct();
}

// ===========================================================================
// SceneManagerModule::Prepare @ 0x828D13E0
//
// Resumable staged prepare: each call advances the prepare progression by one stage,
// preparing the next sub-manager; a sub-manager that returns false leaves the stage
// where it is so the next call retries it. The stage variable (mePrepareStage) is the
// switch selector; sub_828AA4C0 is the stage post-increment + bound assert. When the
// final stage completes the module returns true and resets meReleaseStage to START.
// ===========================================================================
bool SceneManagerModule::Prepare(SpatialPartitionConstructParams* lpConstructParams,
                                 rw::IResourceAllocator*          lpSceneAllocator,
                                 rw::IResourceAllocator*          lpCacheAllocator,
                                 CgsMemory::LinearMalloc*         lpLinearAllocator)
{
    // The X360 falls through the switch from the current stage to DONE, advancing the
    // stage after each successful sub-step; an unrecognised stage asserts. De-gotoed
    // into a fall-through switch (each successful case advances mePrepareStage).
    switch (mePrepareStage)
    {
        case E_SCENEMANAGER_PREPARE_DONE:
            mePrepareStage = E_SCENEMANAGER_PREPARE_START;
            // fall through
        case E_SCENEMANAGER_PREPARE_START:
            mePrepareStage = E_SCENEMANAGER_PREPARE_MANAGER;
            // fall through
        case E_SCENEMANAGER_PREPARE_MANAGER:
            if (!ModuleSingleBuffered::Prepare())
                return false;
            mePrepareStage = E_SCENEMANAGER_PREPARE_ENTITY_MANAGER;
            // fall through
        case E_SCENEMANAGER_PREPARE_ENTITY_MANAGER:
            if (!mEntityManager.Prepare())
                return false;
            mePrepareStage = E_SCENEMANAGER_PREPARE_VOLUME_MANAGER;
            // fall through
        case E_SCENEMANAGER_PREPARE_VOLUME_MANAGER:
            if (!mVolumeManager.Prepare())
                return false;
            mePrepareStage = E_SCENEMANAGER_PREPARE_CACHE_MANAGER;
            // fall through
        case E_SCENEMANAGER_PREPARE_CACHE_MANAGER:
            if (!mTriangleCacheManager.Prepare(reinterpret_cast<rw::IResourceAllocator*>(lpCacheAllocator)))
                return false;
            mePrepareStage = E_SCENEMANAGER_PREPARE_TRI_COLLISION_MANAGER;
            // fall through
        case E_SCENEMANAGER_PREPARE_TRI_COLLISION_MANAGER:
            if (!mTriangleCollisionManager.Prepare(lpLinearAllocator, 512))
                return false;
            mePrepareStage = E_SCENEMANAGER_PREPARE_SCENE_GRAPH_MODULE;
            // fall through
        case E_SCENEMANAGER_PREPARE_SCENE_GRAPH_MODULE:
            if (!mSpatialPartitionManager.Prepare(lpConstructParams, lpSceneAllocator))
                return false;
            mePrepareStage = E_SCENEMANAGER_PREPARE_OVERLAP_GENERATION_MODULE;
            // fall through
        case E_SCENEMANAGER_PREPARE_OVERLAP_GENERATION_MODULE:
            CreateCullingTable(lpSceneAllocator);
            // ⭐ CALL SITE REPAIRED 2026-08-19 (wave Q5 / E1a), taking up the request cluster
            // E2 left in ContactGen/CgsOverlapGenerationModule.h:96-103: Prepare's first
            // parameter is `u8*` (the DWARF's, pinned by the SceneSweeper::Prepare it forwards
            // to unchanged @0x828CB734), and what the console passes is the culling-group
            // manager's PER-INSTANCE GROUP ARRAY -- which is that manager's first member, so
            // `&mCullingGroupManager` and the accessor below are address-identical and the old
            // spelling was right by accident, wrong by type.
            if (!mOverlapGenerator.Prepare(mCullingGroupManager.GetVolumeInstanceCullingGroup(),
                                           mCullingGroupManager.GetCullingTable()))
                return false;
            mePrepareStage = E_SCENEMANAGER_PREPARE_OVERLAP_CULLING_MODULE;
            // fall through
        case E_SCENEMANAGER_PREPARE_OVERLAP_CULLING_MODULE:
            if (!mOverlapCuller.Prepare(&mEntityManager, &mVolumeManager))
                return false;
            mePrepareStage = E_SCENEMANAGER_PREPARE_FINE_INTERSECTION_TEST_MODULE;
            // fall through
        case E_SCENEMANAGER_PREPARE_FINE_INTERSECTION_TEST_MODULE:
            if (!mFineIntersectionTestModule.Prepare(&mEntityManager, &mVolumeManager))
                return false;
            mePrepareStage = E_SCENEMANAGER_PREPARE_DONE;
            meReleaseStage = E_SCENEMANAGER_RELEASE_START;
            return true;

        default:
            CGS_ASSERT(false, "Unrecognised release state");
            return false;
    }
}

// ===========================================================================
// SceneManagerModule::StartUpdateTriangleCache @ 0x828C73D8  (73 insns)
//
// The frame's whole triangle-collision front end, in the console's order. Landed
// 2026-08-10 (spatial-partition wave) -- it was a WorldLinkStubs gate purely
// because ONE of its seven callees did not exist: PolygonSoupListSpatialMap::
// BuildSpacialPartition @0x82841740. Every other callee was already reconstructed.
//
// X360 body, statement for statement:
//   assert(lpInputBufferStack  != NULL)                    CgsSceneManagerModule.cpp:573
//   assert(lpOutputBufferStack != NULL)                    CgsSceneManagerModule.cpp:574
//   *(this + 0x3A84D0) = lpCollisionGenerator      <- mpTriangleCacheCollisionGenerator,
//                                                     the pointer EndUpdateTriangleCache
//                                                     reads back later this frame
//   IOBuffer::LockForRead(lpSceneInputBuffer)
//   lrScene = lpSceneInputBuffer->GetInSceneUpdateInterface()      (sub_828AF1C8)
//   mTriangleCacheManager.ProcessRemoveFromCacheEvents(lrScene)                 (+0)
//   mTriangleCacheManager.ProcessAddToCacheEvents(lrScene.mAddToCacheQueue)     (+0xC4930)
//   mTriangleCollisionManager.ProcessAddPolySoupListEvents(
//                                       lrScene.mAddPolySoupListQueue)          (+0xC7C94)
//   /* ProcessClearPolySoupListEvents, INLINED by the console: */
//   if (*(lrScene + 0xC7DE8) > 0) { FreeAll(mgr+0x7C); Clear(mgr+0x0); mgr->+0x78 = 0; }
//   mTriangleCacheManager.ProcessUpdateCachedPositionEvents(
//                                       lrScene.mUpdateCachedPositionQueue)     (+0xC5290)
//   mTriangleCacheManager.StartUpdateTriangleCaches(
//                                       *(this + 0x3A84D0), &the collision scene)
//   IOBuffer::UnlockForRead(lpSceneInputBuffer)
//
// ⭐ THE ORDER IS THE POINT and it is not incidental: the poly-soup registration
// (and therefore the partition rebuild) runs BEFORE the cache fill in the same
// call, so the very first frame that registers a soup list also gets a usable
// spatial map handed to StartUpdateTriangleCaches. Reproduced exactly.
//
// ⚠️ +0xC7DE8 is mClearPolySoupListsQueue (+0xC7DE0) PLUS 8 -- the EventQueue's
// length word, not the queue base. Expressed here as the queue's own GetLength()
// via the real ProcessClearPolySoupListEvents (PS3 @0xC49108 proves the three
// inlined statements are exactly that function).
// ===========================================================================
void SceneManagerModule::StartUpdateTriangleCache(CgsModule::IOBufferStack* lpInputBufferStack,
                                                  CgsModule::IOBufferStack* lpOutputBufferStack,
                                                  SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer,
                                                  BaseCollisionGenerator* lpCollisionGenerator)
{
    CGS_ASSERT(lpInputBufferStack != NULL,  "lpInputBufferStack != NULL");
    CGS_ASSERT(lpOutputBufferStack != NULL, "lpOutputBufferStack != NULL");

    // Latch the frame's generator for the End half. The X360 stores it BEFORE the
    // lock, and unconditionally -- including when it is null.
    mpTriangleCacheCollisionGenerator = lpCollisionGenerator;

    if (lpSceneInputBuffer == NULL)
    {
        // PC GUARD (not console): the console never reaches here with a null buffer
        // because WorldModule::Update always supplies one. Bail rather than fault if
        // an earlier stage was skipped.
        return;
    }

    lpSceneInputBuffer->LockForRead();

    // ⚠️ THE **CONST** OVERLOAD (X360 0x828AF1C8), and the const is load-bearing: its twin
    // 0x825BD8C0 tests the WRITE bit and fires "Not locked for writing". Reaching it through a
    // const reference is what selects the read-lock accessor the console actually calls here.
    // Getting this wrong cost a full verification run: 927 asserts, boot wedged in FLYBY.
    const SceneManagerIO::InputBuffer_Update* lpcSceneInputBuffer = lpSceneInputBuffer;
    const SceneManagerIO::InSceneUpdateInterface* lpScene =
        lpcSceneInputBuffer->GetInSceneUpdateInterface();

    if (lpScene != NULL)
    {
        // Slot bookkeeping first: free, then claim, so a slot released and re-taken in
        // one frame lands correctly (Remove's own dev cross-checks depend on this order).
        mTriangleCacheManager.ProcessRemoveFromCacheEvents(*lpScene);
        mTriangleCacheManager.ProcessAddToCacheEvents(lpScene->mAddToCacheQueue);

        // Then the static world: register any newly-streamed poly-soup lists and, if any
        // of them asked for it, REBUILD THE SPATIAL PARTITION. This is the call that makes
        // mpLeafNodes non-null and therefore lets the fill below actually do something.
        mTriangleCollisionManager.ProcessAddPolySoupListEvents(lpScene->mAddPolySoupListQueue);
        mTriangleCollisionManager.ProcessClearPolySoupListEvents(lpScene->mClearPolySoupListsQueue);

        // Reposition the cached objects, marking dirty the ones that left their sphere.
        mTriangleCacheManager.ProcessUpdateCachedPositionEvents(lpScene->mUpdateCachedPositionQueue);

        // And open this frame's fill against the (now built) partition.
        mTriangleCacheManager.StartUpdateTriangleCaches(
            mpTriangleCacheCollisionGenerator,
            mTriangleCollisionManager.GetPolySoupListSpacialMap());
    }

    lpSceneInputBuffer->UnlockForRead();
}

// ===========================================================================
// SceneManagerModule::EndUpdateTriangleCache @ 0x828C7500
//
// Forward to the triangle-cache manager with the cached collision generator the
// matching StartUpdateTriangleCache stashed, and the triangle-collision scene.
// ===========================================================================
void SceneManagerModule::EndUpdateTriangleCache(CgsModule::IOBufferStack* /*lpInputBufferStack*/,
                                                CgsModule::IOBufferStack* /*lpOutputBufferStack*/)
{
    // X360 tail-call: TriangleCacheManager::EndUpdateTriangleCaches(&mTriangleCacheManager,
    //   mpTriangleCacheCollisionGenerator, &mTriangleCollisionManager's collision scene).
    mTriangleCacheManager.EndUpdateTriangleCaches(mpTriangleCacheCollisionGenerator,
                                                  &mTriangleCollisionManager);
}

// ===========================================================================
// SceneManagerModule::ProcessSetVolumeInstanceCullingGroupEvent @ 0x828CF8E8
//
// Resolve the event's volume-instance id to its pool index and store the requested
// culling group on that instance. Asserts the input buffer, that the instance was
// found, and that the index is in range.
//
// ⭐ RETYPED 2026-08-19 (wave Q5 / E1a) -- see the declaration's note. The two fitted
// placeholder parameter types are gone, so the event is now read through its own named
// members (InEventSetVolumeInstanceCullingGroup::mVolumeInstanceId / miCullingGroupId)
// instead of a `const u32*` view over the buffer. Behaviour is unchanged: the X360 loads
// the 64-bit id (`ld r4,0(r28)`) and the third word (`lwz r11,8(r28)`), then stores that
// word's low byte with `stbx`.
// ===========================================================================
void SceneManagerModule::ProcessSetVolumeInstanceCullingGroupEvent(
    const SceneManagerIO::InEventSetVolumeInstanceCullingGroup& lrEvent,
    OverlapGenerationIO::InputBuffer* lpOverlapGenerationInput)
{
    CGS_ASSERT(lpOverlapGenerationInput != NULL, "lpOverlapGenerationInputBuffer != NULL");

    const VolumeInstanceId lInstanceId    = lrEvent.mVolumeInstanceId;
    const u8               lu8CullingGroup = static_cast<u8>(lrEvent.miCullingGroupId);

    const s32 liVolumeInstIndex = mEntityManager.GetVolumeInstanceIndexByID(lInstanceId);
    if (liVolumeInstIndex == -1)
    {
        CGS_ASSERT(false, "Volume instance not found");
        return;
    }

    CGS_ASSERT(liVolumeInstIndex < KI_MAX_NUM_VOLUME_INSTANCES,
               "liVolumeInstIndex < KI_MAX_NUM_VOLUME_INSTANCES");

    // Store the new culling group on that instance (X360 stores a byte into the
    // culling-group manager's per-instance array at the resolved index).
    mCullingGroupManager.GetVolumeInstanceCullingGroup()[liVolumeInstIndex] = lu8CullingGroup;
}

// ===========================================================================
// SceneManagerModule::UpdateContactGeneration @ 0x828D5CA0
//
// The per-frame contact-generation pipeline:
//   generate overlap pairs -> bridge to the culler -> cull -> bridge results to the
//   scene output buffer + the tri-cache. Each stage is bracketed by a CPU perfmon
//   region; the IO buffers are pushed/popped on the in/out buffer stacks.
//
// SCOPE NOTE: the X360 threads the overlap pairs through a chain of sub_823B* helper
// passes (the SceneSweeper sort/sweep/build-colliding-pairs bridge funcs) and reads
// the culler's contact count for a debug log line. Those bridge helpers + the
// truncated-name accessors live in the SceneManager bridge TU; this body reconstructs
// the pipeline at the IO-buffer + named-stage level the SceneManagerModule owns.
// ===========================================================================
void SceneManagerModule::UpdateContactGeneration(CgsModule::IOBufferStack* lpInputBufferStack,
                                                 CgsModule::IOBufferStack* lpOutputBufferStack,
                                                 SceneManagerIO::InputBuffer_Update*  lpSceneInputBuffer,
                                                 SceneManagerIO::OutputBuffer*  lpSceneOutputBuffer)
{
    ScopedPerfMon lUpdate(siContactGen_UpdatePerfMon);

    CGS_ASSERT(lpInputBufferStack != NULL,  "lpInputBufferStack != NULL");
    CGS_ASSERT(lpOutputBufferStack != NULL, "lpOutputBufferStack != NULL");
    CGS_ASSERT(lpSceneInputBuffer != NULL,  "lpSceneInputBuffer != NULL");
    CGS_ASSERT(lpSceneOutputBuffer != NULL, "lpSceneOutputBuffer != NULL");

    CgsModule::IOBufferStack* lpInStack  = lpInputBufferStack;
    CgsModule::IOBufferStack* lpOutStack = lpOutputBufferStack;

    // --- allocate the pipeline IO buffers on the in/out stacks ---
    OverlapCullingIO::InputBuffer*     lpCullInput   = nullptr;
    OverlapCullingIO::OutputBuffer*    lpCullOutput  = nullptr;
    OverlapGenerationIO::OutputBuffer* lpGenOutput   = nullptr;
    {
        ScopedPerfMon lIoBuffers(siContactGen_IOBuffersPerfMon);
        lpInStack->CreateIOBuffer(&lpCullInput,  "OverlapCulling");
        lpOutStack->CreateIOBuffer(&lpCullOutput, "OverlapCulling");
        lpOutStack->CreateIOBuffer(&lpGenOutput,  "OverlapGeneration");
    }

    // --- generate overlap pairs ---
    {
        ScopedPerfMon lGenerate(siContactGen_GenerateOverlapsPerfMon);
        mOverlapGenerator.GenerateOverlaps(lpGenOutput, &mEntityManager);
    }

    // --- bridge generation -> culling input ---
    {
        ScopedPerfMon lBridges(siContactGen_BridgesPerfMon);
        // ⭐ CASTS RETIRED 2026-08-19 (wave Q5 / E1a, following cluster F1's header retype of
        // the three BridgeOverlap* declarations): the parameters are now the console's real
        // buffer types, so the pointers go through unmodified. The three reinterpret_casts
        // that stood here were the price of the old `(OutputBuffer*, OutputBuffer*)` fitting.
        // The console's 2-buffer lock helper pair around the bridge (sub_823B6FE0 @0x828D5E3C:
        // LockForWrite(cullIn) + LockForRead(genOut); sub_823B7060 @0x828D5E58 unlocks in
        // reverse). Every accessor the bridge calls carries the console's own lock tripwire
        // ('Not locked for reading/writing'), so these brackets are load-bearing, not tidy.
        // (wave Q5 round-3 integration, 2026-08-19; registers resolved in f1.owner.md section 8)
        lpCullInput->LockForWrite();
        lpGenOutput->LockForRead();
        BridgeOverlapGenerationToOverlapCulling(lpCullInput, lpGenOutput);
        lpGenOutput->UnlockForRead();
        lpCullInput->UnlockForWrite();
    }

    // --- the query accumulator the culler writes alongside its output ---
    ContactGenerator::QueryAccumulator* lpQueryAccumulator = nullptr;
    {
        ScopedPerfMon lIoBuffers(siContactGen_IOBuffersPerfMon);
        lpInStack->CreateIOBuffer(&lpQueryAccumulator, "QueryAccumulator");
    }

    // --- cull the overlaps ---
    {
        ScopedPerfMon lCull(siContactGen_CullOverlapsPerfMon);
        mOverlapCuller.CullOverlaps(lpCullInput, lpCullOutput);
    }

    // --- bridge culler + generation results to the scene output buffer ---
    {
        ScopedPerfMon lBridges(siContactGen_BridgesPerfMon);
        // The console's 3-buffer lock helper pair (sub_823B70E0 @0x828D5F20: LockForWrite(sceneOut)
        // + LockForRead(cullOut) + LockForRead(genOut); sub_823B7190 @0x828D5FB0 unlocks in
        // reverse). Between them the console also prints the debug-only 'Contact count from
        // culler: N' line (0x828D5F48, gated on byte_83085B30 && gxMessageFilterFlags&1).
        lpSceneOutputBuffer->LockForWrite();
        lpCullOutput->LockForRead();
        lpGenOutput->LockForRead();
        BridgeOverlapCullerToOutputBuffer(lpSceneOutputBuffer, lpCullOutput);
        BridgeOverlapGenerationToOutputBuffer(lpSceneOutputBuffer, lpGenOutput);
        lpGenOutput->UnlockForRead();
        lpCullOutput->UnlockForRead();
        lpSceneOutputBuffer->UnlockForWrite();
    }

    // --- tear down the pipeline IO buffers (reverse order) ---
    {
        ScopedPerfMon lIoBuffers(siContactGen_IOBuffersPerfMon);
        lpInStack->DestroyIOBuffer(&lpQueryAccumulator);
        lpInStack->DestroyIOBuffer(&lpCullInput);
        lpOutStack->DestroyIOBuffer(&lpGenOutput);
        lpOutStack->DestroyIOBuffer(&lpCullOutput);
    }

    (void)lpSceneInputBuffer;
}

// ===========================================================================
// SceneManagerModule::BridgeInputSceneUpdateInterfaceToSubModules @ 0x828D1F88
//   (1614 insns. Console home CgsSceneManagerBridgeFunctions.cpp:58 -- its three
//    null tripwires bake lines :60 / :61 / :62.)
//
// The scene input's InSceneUpdateInterface is a batch of 25 producer queues; this fans
// them out into the sub-modules.
//
// ⭐ WAVE Q5 / CLUSTER E1a (2026-08-19) -- THE VOLUME LEGS LANDED. Until this wave the
// body had only the entity legs and the triangle-cache/poly-soup legs, so every queued
// AddDynamicVolume / AddVolumeInstance / AddForCollision / SetVolumeInstanceTransform
// event was dropped with the frame's buffer and NOTHING in the scene ever became a
// VolumeInstance -- the first of the three missing layers between a registered prop
// volume and a car-vs-prop contact.
//
// ⭐ WAVE Q5 / CLUSTER RMLEG (2026-08-19) -- THE THREE REMOVAL LEGS LANDED (3, 4's volume
// half, 5), retiring round 3's §P1/§P2. 22 of 23 legs are now live; only §P3 remains.
// This closed a LIVE boot assert: with no removal leg, no VolumeInstanceId was ever taken
// out of mVolumeInstanceIdToIndex, so the game re-creating the player car (colour change /
// HIDE_ONLINE remove+create) drove ActiveRaceCar::RemoveFromScene -> RemoveVolumeInstance /
// RemoveVolume / RemoveForCollision / RemoveEntity, all of which were dropped, and the next
// AddToScene fired "2 elements with the same key inserted" in
// IndexedHashTable<VolumeInstanceId,u32,509>::Insert from EntityManager::AddVolumeInstance.
//
// ---- THE CONSOLE'S LEG ORDER (each `bl` read off the raw asm), reproduced below -------
//    1  0x828D20A4  mRemoveVolumeQueue                 -> VolumeManager::RemoveVolume
//    2  0x828D20DC  mRemoveForCollisionQueue           -> ProcessRemoveForCollisionEvent
//    3  0x828D21C4  mRemoveVolumeInstanceQueue         -> EntityManager::RemoveVolumeInstance
//    4  0x828D279C  mRemoveEntityQueue                 -> RemoveAllEntityVolumeInstances
//                   0x828D27B8 partition vtbl+0x3C (RemoveEntityFromGraph)
//                   0x828D2838 EntityManager::RemoveEntity
//    5  0x828D28FC  mRemoveAllEntitiesQueue            -> RemoveAllOwnerVolumeInstances
//    6  0x828D292C  [lbPrepare] mRemoveFromCacheQueue  -> TriangleCacheManager
//    7  0x828D2A10  mAddEntityQueue                    -> EntityManager::AddEntity
//                   0x828D2B70 partition AllocEntity, 0x828D2BAC vtbl+0x38 AddEntityToGraph
//    8  0x828D2C2C  mAddDynamicVolumeQueue             -> VolumeManager::AddDynamicVolume
//    9  0x828D2C78  mAddVolumeInstanceQueue            -> EntityManager::AddVolumeInstance
//   10  0x828D2D28  mAddForCollisionQueue              -> ProcessAddForCollisionEvent
//   11  0x828D2D5C  [lbPrepare] mAddToCacheQueue       -> TriangleCacheManager
//       0x828D2D74  [lbPrepare] mAddPolySoupListQueue  -> TriangleCollisionManager
//       0x828D2D90  [lbPrepare] the INLINED ProcessClearPolySoupListEvents
//   12  0x828D2E64  mReplaceDynamicVolumeQueue         -> VolumeManager::ReplaceDynamicVolume
//   13  0x828D30A0  mSetEntityRadiusQueue              -> partition vtbl+0x30
//   14  0x828D31A8  mClearCullingTableQueue            -> the culling table's own words
//   15  0x828D3230  mSetCullingGroupPairQueue          -> CullingGroupManager::SetCullingGroupPair
//   16  0x828D32D0  mSetVolumeInstanceCullingGroupQueue-> ProcessSetVolumeInstanceCullingGroupEvent
//   17  0x828D34E8  mUpdatePositionQueue               -> partition vtbl+0x2C
//   18  0x828D358C  mSetVolumeInstanceTransformQueue   -> EntityManager::SetVolumeInstanceTransform
//                   0x828D35C8 SetVolumePadding, 0x828D35DC UpdateCollisionBody
//   19  0x828D368C  mSetPaddingQueue                   -> ProcessSetPaddingEvent
//   20  0x828D3730  mClearEntityPaddingQueue           -> ProcessClearEntityPaddingEvent
//   21  0x828D37D4  mForceNoPaddingQueue               -> ProcessForceNoPaddingEvent
//   22  0x828D3874  mAddVolumeInstanceForCachingQueue  -> ProcessAddVolumeInstanceForCachingEvent
//                                                                     ⚠️ PARKED (§P3)
//   23  0x828D38A8  [lbPrepare] mUpdateCachedPositionQueue -> TriangleCacheManager
//
// ---- WHAT IS BEHIND `lbPrepare` -- and what is NOT ------------------------------------
// ONLY the four triangle-cache / poly-soup legs (6, 11, 23) are gated. Measured:
//     0x828D1FA4  stb r7, arg_37(r1)          <- lbPrepare, the 5th register argument
//     0x828D290C  lbz r11, arg_37 ; beq       -> skips leg 6
//     0x828D2D44  lwz r11, var_188 ; beq      -> skips leg 11 (the same latched byte)
//     0x828D388C  lwz r11, var_188 ; beq      -> skips leg 23
// EVERY volume leg runs on the per-frame pass. The bridge owns the cache queues on the
// PREPARE passes and StartUpdateTriangleCache @0x828C73D8 owns them per frame; nothing
// double-drains. (Why the cache legs matter at all: VehicleManager::PrepareTriangleCache
// @0x82615BA0 and WorldModule::PrepareWorldCollision @0x827C9478 each stage their events
// into a scene input buffer they create AND destroy inside one call, with
// UpdateScene(..., lbPrepare = TRUE) the only consumer in between -- landed 2026-08-10,
// runtime-witnessed at 396 poly-soup zones + 28 cache registrations.)
//
// ---- THE SPATIAL PARTITION IS PER-LEG, NOT WHOLE-BODY (behaviour preserved) -----------
// The previous revision fetched the partition and `return`ed early when it was null,
// which is a PC guard the console does not have (the console loads module+0x280 and
// calls straight through it). That early return sat ABOVE every volume leg, so wiring
// the volume legs underneath it would have made a missing entity octree silently
// suppress the whole collision-volume registry -- an absent partition has nothing to do
// with volumes. The guard is therefore now per-leg, on exactly the four legs that use
// the partition (4, 7, 13, 17). Behaviour for those four is bit-identical to the early
// return; the volume legs simply no longer depend on it.
//
// ---- P A R K S  (nothing here is fabricated; each names its exact blocker) ------------
// §P1 / §P2 -- RETIRED 2026-08-19 (wave Q5 / cluster RMLEG). LANDED: leg 3 now calls
//      EntityManager::RemoveVolumeInstance (DWARF CgsEntityManager.h:120, bodied this wave
//      in CgsEntityManager.cpp together with RemoveVolumeInstanceByIndex h:126), and legs 4
//      and 5 call SceneManagerModule::RemoveAllEntityVolumeInstances @0x828CD9A0 /
//      RemoveAllOwnerVolumeInstances @0x828CDA70, both bodied at the end of this file. The
//      blocker was ownership only -- the trio's last two calls touch private EntityManager
//      members and cluster E1a did not own that TU. Neither the pool-slot leak (§P1) nor the
//      permanent phantom broad-phase body (§P2) exists any more.
// §P3  LEG 22, ProcessAddVolumeInstanceForCachingEvent @0x828CF038 (DWARF
//      CgsSceneManagerModule.h:328, .cpp:995). Not seeded in the header by the conductor
//      and not assigned to any round-3 cluster; declaring it here without a body would be
//      a link hole for no gain. It feeds the TRIANGLE cache, not the volume broad phase,
//      so it is off the car-vs-prop path.
// ===========================================================================
void SceneManagerModule::BridgeInputSceneUpdateInterfaceToSubModules(
    OverlapGenerationIO::InputBuffer* lpOverlapGenerationInput,
    SpatialPartitionIO::InputBuffer_Update* /*lpSpatialPartitionInput*/,
    SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer,
    bool lbPrepare)
{
    // The console's three tripwires, in its order (BridgeFunctions.cpp:60/:61/:62). The
    // spatial-partition one is deliberately NOT reproduced: this tree applies the entity
    // legs straight to the partition rather than restaging them through a
    // SpatialPartitionIO::InputBuffer_Update, so UpdateScene never creates that buffer and
    // asserting on it would fire every frame. See UpdateScene's own FLAG.
    CGS_ASSERT(lpOverlapGenerationInput != NULL, "lpOverlapGenerationInputBuffer != NULL");
    CGS_ASSERT(lpSceneInputBuffer != NULL, "lpSceneInputBuffer != NULL");
    if (lpSceneInputBuffer == NULL)
    {
        return;
    }

    SceneManagerIO::InSceneUpdateInterface* lpScene =
        lpSceneInputBuffer->GetInSceneUpdateInterface();
    if (lpScene == NULL)
    {
        return;
    }

    // Fetched once, used by legs 4 / 7 / 13 / 17 only (see the per-leg note above).
    SpatialPartition* lpPartition = mSpatialPartitionManager.GetSpatialPartition();

    // FLAG PC (not console): the seven legs that PRODUCE overlap-generation work take the
    // input buffer by pointer and the console can never see it null. On the host
    // IOBufferStack::CreateIOBuffer<InputBuffer> CAN fail (the buffer is ~2.4 MB), and the
    // console's answer to a null -- assert once at the top and then dereference anyway --
    // would be 1536 asserts and a fault. The single tripwire above is kept; the seven legs
    // that need the buffer are additionally skipped when it is absent, which is the same
    // shape as the partition guard and is announced once under BRN_PROP_DIAG.
    const bool lbHaveGenerationInput = (lpOverlapGenerationInput != NULL);
    if (!lbHaveGenerationInput && Q5DiagEnabled())
    {
        static bool sbLoggedNoGenInput = false;
        if (!sbLoggedNoGenInput)
        {
            sbLoggedNoGenInput = true;
            *CgsDev::Log::gpDebugPrint
                << "[Q5-drain] NO overlap-generation input buffer -- the AddForCollision /"
                   " RemoveForCollision / transform / padding legs are skipped this frame\n";
        }
    }

    // ---- leg 1: remove volumes (X360 0x828D2084..0x828D20B0) --------------------------
    {
        const s32 liCount = lpScene->mRemoveVolumeQueue.GetLength();
        for (s32 liEvent = 0; liEvent < liCount; ++liEvent)
        {
            // `ld r4, 0(event)` -- the whole 64-bit VolumeId.
            mVolumeManager.RemoveVolume(lpScene->mRemoveVolumeQueue.GetEvent(liEvent).mVolumeId);
        }
    }

    // ---- leg 2: remove-for-collision (X360 0x828D20C4..0x828D20E8) --------------------
    if (lbHaveGenerationInput)
    {
        const s32 liCount = lpScene->mRemoveForCollisionQueue.GetLength();
        for (s32 liEvent = 0; liEvent < liCount; ++liEvent)
        {
            ProcessRemoveForCollisionEvent(lpScene->mRemoveForCollisionQueue.GetEvent(liEvent),
                                           lpOverlapGenerationInput);
        }
    }

    // ---- leg 3: remove volume instances (X360 0x828D214C..0x828D21FC) -----------------
    // ⭐ LANDED 2026-08-19 (wave Q5 / cluster RMLEG); was §P1. The console inlines
    // EntityManager::RemoveVolumeInstance(id) here as the trio
    //   0x828D21C4 GetVolumeInstanceIndexByID -> 0x828D21D4 RemoveVolumeInstanceFromEntity
    //   -> 0x828D21E0 mVolumeInstanceIdToIndex.Remove -> 0x828D21EC mVolumeInstancePool.FreeObject
    // and that method (DWARF CgsEntityManager.h:120) now exists, so the leg is three lines.
    // ⚠️ THIS LEG IS THE FIX FOR THE LIVE BOOT ASSERT "2 elements with the same key
    // inserted" in IndexedHashTable<VolumeInstanceId,u32,509>::Insert: without it nothing
    // ever removed a key, so ActiveRaceCar::RemoveFromScene followed by AddToScene (colour
    // change / HIDE_ONLINE remove+create) re-inserted the car's own slot-0 id.
    {
        const s32 liCount = lpScene->mRemoveVolumeInstanceQueue.GetLength();
        for (s32 liEvent = 0; liEvent < liCount; ++liEvent)
        {
            // `ldx r31, r28, r11` -- the whole 64-bit VolumeInstanceId.
            mEntityManager.RemoveVolumeInstance(
                lpScene->mRemoveVolumeInstanceQueue.GetEvent(liEvent).mVolumeInstanceId);
        }
    }

    // ---- leg 4: remove entities (X360 0x828D22B0..0x828D2854) -------------------------
    // X360 order: a slot has to be free before the adds run.
    // ⭐ THE VOLUME HALF LANDED 2026-08-19 (wave Q5 / cluster RMLEG); was §P2. The console's
    // order is RemoveAllEntityVolumeInstances (0x828D279C) BEFORE the partition unlink
    // (0x828D27B8) and the entity free (0x828D2838), and the volume call is gated on the
    // event's own byte at +4 (`lbz r11, 4(r25)` @0x828D2784 -- the producer writes it with
    // `stb r5, 4` @0x822B12E8, so producer and consumer agree; on the host both sides speak
    // the whole u32 muOptions word, which is consistent for every observed flag value).
    // The whole-leg `if (lpPartition != NULL)` is gone: only the graph unlink needs the
    // partition, and a missing octree must not suppress the volume registry (same reasoning
    // as the per-leg partition note in the banner).
    {
        const CgsModule::EventQueue<SceneManagerIO::InEventRemoveEntity, 10000>& lrQueue =
            lpScene->GetRemoveEntityQueue();
        const s32 liCount = lrQueue.GetLength();
        for (s32 liEvent = 0; liEvent < liCount; ++liEvent)
        {
            const SceneManagerIO::InEventRemoveEntity& lrEvent = lrQueue.GetEvent(liEvent);
            const s32 liIndex = mEntityManager.GetEntityIndexByID(lrEvent.mEntityId);
            if (liIndex < 0)
            {
                // FLAG PC (not console): the X360 fires its "Entity added and removed on the
                // same frame" / "0x%X" diagnostic here and then FALLS THROUGH into the
                // removal with index 0xFFFF (0x828D240C branches to the gate either way).
                // Reproducing that would hand 0xFFFF to RemoveEntityFromGraph and
                // RemoveEntity. Kept as the pre-existing skip.
                continue;   // never registered (or already retired)
            }
            if (lrEvent.muOptions != 0)                                     // 0x828D2784
            {
                RemoveAllEntityVolumeInstances(static_cast<u16>(liIndex),
                                               lpOverlapGenerationInput);   // 0x828D279C
            }
            if (lpPartition != NULL)
            {
                lpPartition->RemoveEntityFromGraph(static_cast<u16>(liIndex));  // 0x828D27B8
            }
            mEntityManager.RemoveEntity(static_cast<u16>(liIndex));         // 0x828D2838
        }
    }

    // ---- leg 5: remove all entities of an owner (X360 0x828D2884..0x828D2908) ---------
    // ⭐ LANDED 2026-08-19 (wave Q5 / cluster RMLEG); was §P2. One event = one owner byte
    // (`lbzx r4, r11, r31` @0x828D28F8); the whole sweep lives in the callee.
    {
        const s32 liCount = lpScene->mRemoveAllEntitiesQueue.GetLength();
        for (s32 liEvent = 0; liEvent < liCount; ++liEvent)
        {
            RemoveAllOwnerVolumeInstances(
                lpScene->mRemoveAllEntitiesQueue.GetEvent(liEvent).mu8Owner,
                lpOverlapGenerationInput);                                  // 0x828D28FC
        }
    }

    // ---- leg 6: [lbPrepare] the triangle cache's slot RELEASES ------------------------
    // ⭐ ADDED 2026-08-10 (producer wave); the `lbPrepare` gate is the console's own, and
    // the ORDER (release before claim) is what makes a slot released and re-taken in one
    // frame land correctly -- Remove's dev cross-checks depend on it.
    if (lbPrepare)
    {
        mTriangleCacheManager.ProcessRemoveFromCacheEvents(*lpScene);
    }

    // ---- leg 7: add entities ---------------------------------------------------------
    // Note the X360 calls the partition DIRECTLY (module+0x280) for this leg rather than
    // routing it through the spatial-partition update queue -- reproduced.
    if (lpPartition != NULL)
    {
        const CgsModule::EventQueue<SceneManagerIO::InEventAddEntity, 5120>& lrQueue =
            lpScene->GetAddEntityQueue();
        const s32 liCount = lrQueue.GetLength();
        for (s32 liEvent = 0; liEvent < liCount; ++liEvent)
        {
            const SceneManagerIO::InEventAddEntity& lrEvent = lrQueue.GetEvent(liEvent);

            const u16 lu16Index = mEntityManager.AddEntity(lrEvent.mEntityId);
            CGS_ASSERT(lu16Index < KI_MAX_NUM_ENTITIES, "Out of bounds entity index from entity ");
            if (lu16Index >= KI_MAX_NUM_ENTITIES)
            {
                continue;
            }

            SpatialPartitionEntityLink* lpNewEntity =
                lpPartition->AllocEntity(lu16Index,
                                         static_cast<u32>(lrEvent.miField14),
                                         lrEvent.mTransformLane,
                                         lrEvent.mfField18);
            if (lpNewEntity == NULL)
            {
                CGS_ASSERT(false, "lpNewEntity != NULL");
                continue;
            }
            lpPartition->AddEntityToGraph(lu16Index);
        }

        // [DIAG culling wave] running tally of everything the broad phase has taken on.
        if (liCount > 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            static s32 siTotalAdded = 0;
            siTotalAdded += liCount;
            *CgsDev::Log::gpDebugPrint << "[culling-diag] scene AddEntity batch " << liCount
                                       << " (total " << siTotalAdded << ")\n";
        }
    }

    // ---- leg 8: add dynamic volumes (X360 0x828D2BE8..0x828D2C38) ---------------------
    // `ld r4,0(event)` id, `lbz r5,8(event)` type flags, `r6 = event + 0x10` -- the 128-byte
    // serialised rw collision-volume image the producer memcpy'd in. The console asserts
    // `lpVolume != NULL` (BridgeFunctions.cpp:206) on that INTERIOR pointer, which can only
    // be null if the event itself is; kept as the console has it. AddDynamicVolume's return
    // (the volume-pool index) is discarded here -- the owner learns it through the
    // AddVolumeInstance event it posts next.
    {
        const s32 liCount = lpScene->mAddDynamicVolumeQueue.GetLength();
        for (s32 liEvent = 0; liEvent < liCount; ++liEvent)
        {
            const SceneManagerIO::InEventAddDynamicVolume& lrEvent =
                lpScene->mAddDynamicVolumeQueue.GetEvent(liEvent);

            const VolRef::Volume* lpVolume =
                reinterpret_cast<const VolRef::Volume*>(lrEvent.maVolumeData);
            CGS_ASSERT(lpVolume != NULL, "lpVolume != NULL");

            mVolumeManager.AddDynamicVolume(VolumeId(lrEvent.muId), lrEvent.mu8Flags, lpVolume);
        }
    }

    // ---- leg 9: add volume instances (X360 0x828D2C58..0x828D2C84) --------------------
    // `ld r4,0` id, `ld r5,8` volume id, `r6 = event + 0x10` the world transform.
    {
        const s32 liCount = lpScene->mAddVolumeInstanceQueue.GetLength();
        for (s32 liEvent = 0; liEvent < liCount; ++liEvent)
        {
            const SceneManagerIO::InEventAddVolumeInstance& lrEvent =
                lpScene->mAddVolumeInstanceQueue.GetEvent(liEvent);

            mEntityManager.AddVolumeInstance(lrEvent.mVolumeInstanceId,
                                             lrEvent.mVolumeId,
                                             lrEvent.mTransform);
        }
    }

    // ---- leg 10: add-for-collision (X360 0x828D2CB0..0x828D2D38) ----------------------
    // ⭐ THE LEG THE WHOLE WAVE IS ABOUT: this is what turns a registered prop volume into
    // a body the sweep-and-prune broad phase can see.
    if (lbHaveGenerationInput)
    {
        const s32 liCount = lpScene->mAddForCollisionQueue.GetLength();
        for (s32 liEvent = 0; liEvent < liCount; ++liEvent)
        {
            ProcessAddForCollisionEvent(lpScene->mAddForCollisionQueue.GetEvent(liEvent),
                                        lpOverlapGenerationInput);
        }

        // [DIAG] NOT IN THE X360 BINARY -- the wave's own "did the middle come alive?"
        // line. One-shot on the first event ever drained, then a per-100 running count.
        if (liCount > 0 && Q5DiagEnabled())
        {
            static s32  siTotalForCollision = 0;
            static bool sbLoggedFirst       = false;
            const SceneManagerIO::InEventAddForCollision& lrFirst =
                lpScene->mAddForCollisionQueue.GetEvent(0);
            const s32 liPrevTotal = siTotalForCollision;
            siTotalForCollision += liCount;
            if (!sbLoggedFirst)
            {
                sbLoggedFirst = true;
                // The id is printed as its two dwords: the HIGH one carries the embedded
                // EntityId (owner byte + index), the LOW one the per-entity volume index --
                // truncating to 32 bits would throw the half that identifies the prop away.
                const u64 lu64Id = lrFirst.mVolumeInstanceId.muId;
                *CgsDev::Log::gpDebugPrint
                    << "[Q5-drain] first AddForCollision id=" << static_cast<s32>(lu64Id >> 32)
                    << ":" << static_cast<s32>(lu64Id & 0xFFFFFFFFull)
                    << " owner=" << static_cast<s32>(lrFirst.mVolumeInstanceId.GetEntityIDOwner())
                    << " vol=" << mEntityManager.GetVolumeInstanceIndexByID(lrFirst.mVolumeInstanceId)
                    << " group=" << static_cast<s32>(lrFirst.mCullGroup)
                    << " batch=" << liCount << "\n";
            }
            if ((liPrevTotal / 100) != (siTotalForCollision / 100))
            {
                *CgsDev::Log::gpDebugPrint << "[Q5-drain] AddForCollision total "
                                           << siTotalForCollision << "\n";
            }
        }
    }

    // ---- leg 11: [lbPrepare] the triangle cache's slot CLAIMS + the static world -------
    // ⭐ The poly-soup registration (and therefore the partition rebuild) runs BEFORE the
    // cache fill, so the first frame that registers a soup list also gets a usable spatial
    // map. Without this leg the 396 InEventAddPolySoupList events WorldModule::
    // PrepareWorldCollision stages were dropped every load and numLeafNodes stayed 0.
    if (lbPrepare)
    {
        mTriangleCacheManager.ProcessAddToCacheEvents(lpScene->mAddToCacheQueue);

        mTriangleCollisionManager.ProcessAddPolySoupListEvents(lpScene->mAddPolySoupListQueue);
        // ⭐ ADDED 2026-08-19 (E1a): the console INLINES ProcessClearPolySoupListEvents right
        // here (0x828D2D78 `lwz r11, scene+0xC7DE8` == mClearPolySoupListsQueue's length word,
        // then FreeAll / PolygonSoupListSpatialMap::Clear / `stw 0, 0x78`). The sibling
        // StartUpdateTriangleCache @0x828C73D8 already calls the real method; this leg was the
        // one omission left in the group and is restored by name, not by the inlined form.
        mTriangleCollisionManager.ProcessClearPolySoupListEvents(lpScene->mClearPolySoupListsQueue);
    }

    // ---- leg 12: replace dynamic volumes (X360 0x828D2E24..0x828D2E70) ----------------
    {
        const s32 liCount = lpScene->mReplaceDynamicVolumeQueue.GetLength();
        for (s32 liEvent = 0; liEvent < liCount; ++liEvent)
        {
            const SceneManagerIO::InEventReplaceDynamicVolume& lrEvent =
                lpScene->mReplaceDynamicVolumeQueue.GetEvent(liEvent);

            const VolRef::Volume* lpVolume =
                reinterpret_cast<const VolRef::Volume*>(lrEvent.maVolumeData);
            CGS_ASSERT(lpVolume != NULL, "lpVolume != NULL");   // BridgeFunctions.cpp:280

            mVolumeManager.ReplaceDynamicVolume(VolumeId(lrEvent.muId), lpVolume);
        }
    }

    // ---- leg 13: radius updates ------------------------------------------------------
    if (lpPartition != NULL)
    {
        const CgsModule::EventQueue<SceneManagerIO::InEventSetEntityRadius, 512>& lrQueue =
            lpScene->GetSetEntityRadiusQueue();
        const s32 liCount = lrQueue.GetLength();
        for (s32 liEvent = 0; liEvent < liCount; ++liEvent)
        {
            const SceneManagerIO::InEventSetEntityRadius& lrEvent = lrQueue.GetEvent(liEvent);
            const s32 liIndex = mEntityManager.GetEntityIndexByID(lrEvent.mEntityId);
            if (liIndex >= 0)
            {
                lpPartition->SetEntityRadius(static_cast<u16>(liIndex), lrEvent.mfRadius);
            }
        }
    }

    // ---- leg 14: clear the culling table (X360 0x828D30C4..0x828D31F8) ----------------
    // The console asserts a multiple-clear tripwire on the QUEUE LENGTH (`cmpwi r28,1 ; ble`
    // -> BridgeFunctions.cpp:298) and then walks the queue anyway, filling every packed word
    // of the adjacency grid with -1 when the event's mbCullAll byte is set and with 0 when it
    // is not (`li r9,0xC ; stwx <-1|0>, table, r9 ; addi r9,r9,4` bounded by `lwz r8,8(table)`
    // == muWordCount, i.e. the words start at table+0xC == maBits).
    {
        const s32 liCount = lpScene->mClearCullingTableQueue.GetLength();
        // Console message read out of .rdata (0x820F6B90) rather than copied from IDA's
        // 39-char-truncated listing comment -- including its own typo and trailing newline.
        CGS_ASSERT(liCount <= 1,
                   "Scene manager recieved multiple clear culling table events in one frame\n");
        for (s32 liEvent = 0; liEvent < liCount; ++liEvent)
        {
            const SceneManagerIO::InEventClearCullingTable& lrEvent =
                lpScene->mClearCullingTableQueue.GetEvent(liEvent);

            rw::BitTable::Storage* lpTable = mCullingGroupManager.GetCullingTable();
            if (lpTable == NULL)
            {
                continue;   // PC guard: the console reads module+0x3A6EA0+0x13BC unchecked
            }
            const u32 luFill = lrEvent.mbCullAll ? 0xFFFFFFFFu : 0u;
            for (u32 luWord = 0; luWord < lpTable->muWordCount; ++luWord)
            {
                lpTable->maBits[luWord] = luFill;
            }
        }
    }

    // ---- leg 15: culling-group pairs (X360 0x828D3210..0x828D323C) --------------------
    // PropEntityModule::InitializePropPhysicsData @0x822DA840 posts seven of these, including
    // CARS x PROPS. The adjacency grid this fills is what SceneSweeper::BuildCollidingPairs
    // @0x828C2190 reads to STAMP each pair's mbCull -- it does NOT accept or reject pairs itself;
    // the stamped verdict gates the NARROW phase (CgsOverlapCullingModule.cpp:569,
    // `if (!lPair.mbCull)`). Without this leg the grid keeps whatever leg 14's fill left in it, so
    // every pair carries that one default verdict instead of its own group pair's.
    //
    // ⚠️ REPORTED, NOT FIXED (foreign file): the console reads the third field with
    // `lbz r6, 8(event)` -- a BYTE -- and DWARF CgsSceneManagerIO_SceneUpdate.h:230 spells the
    // member `bool mbCull`. This tree's CgsSceneManagerIO_EventSetCullingGroupPair.h models it
    // as `u32 muEnabled` and its producer (CgsSceneManagerIO_SceneUpdate.cpp:404) writes the
    // whole word, so producer and consumer agree on the host and the flag arrives intact; the
    // NAME and TYPE are the divergence, not the value. Do not "fix" the consumer alone: a byte
    // read of a big-endian word is 0, which is exactly the console-value-on-a-host-tree trap.
    {
        const s32 liCount = lpScene->mSetCullingGroupPairQueue.GetLength();
        for (s32 liEvent = 0; liEvent < liCount; ++liEvent)
        {
            const SceneManagerIO::InEventSetCullingGroupPair& lrEvent =
                lpScene->mSetCullingGroupPairQueue.GetEvent(liEvent);

            mCullingGroupManager.SetCullingGroupPair(lrEvent.muGroupA, lrEvent.muGroupB,
                                                     lrEvent.muEnabled != 0);
        }
    }

    // ---- leg 16: per-instance culling group (X360 0x828D3258..0x828D32E0) -------------
    if (lbHaveGenerationInput)
    {
        const s32 liCount = lpScene->mSetVolumeInstanceCullingGroupQueue.GetLength();
        for (s32 liEvent = 0; liEvent < liCount; ++liEvent)
        {
            ProcessSetVolumeInstanceCullingGroupEvent(
                lpScene->mSetVolumeInstanceCullingGroupQueue.GetEvent(liEvent),
                lpOverlapGenerationInput);
        }
    }

    // ---- leg 17: position updates ----------------------------------------------------
    if (lpPartition != NULL)
    {
        const CgsModule::EventQueue<SceneManagerIO::InEventSetEntityPosition, 1024>& lrQueue =
            lpScene->GetUpdatePositionQueue();
        const s32 liCount = lrQueue.GetLength();
        for (s32 liEvent = 0; liEvent < liCount; ++liEvent)
        {
            const SceneManagerIO::InEventSetEntityPosition& lrEvent = lrQueue.GetEvent(liEvent);
            const s32 liIndex = mEntityManager.GetEntityIndexByID(EntityId(lrEvent.mEntityId));
            if (liIndex >= 0)
            {
                lpPartition->SetEntityPosition(static_cast<u16>(liIndex), lrEvent.mPosition);
            }
        }
    }

    // ---- leg 18: volume-instance transforms (X360 0x828D3534..0x828D35E8) -------------
    // ⭐ THE PER-FRAME MOTION LEG: this is how a moving car's (and a moving prop's) box
    // follows it. Four steps, in the console's order:
    //   1. SetVolumeInstanceTransform overwrites the instance's world matrix and RETURNS the
    //      translation delta (sret in r3 @0x828D3574, this in r4);
    //   2. the delta is clamped lane-wise into the swept-padding range and becomes the
    //      instance's broad-phase padding (`vmaxfp128` against a splat of flt_82006D70 then
    //      `vminfp` against a splat of flt_82001D9C -- both read out of the shipped image:
    //      0xc0000000 == -2.0f and 0x40000000 == +2.0f; the w lane of BOTH bounds is the
    //      stack zero at 0x828D357C/0x828D3584, so the clamped w lane is 0);
    //   3. SetVolumePadding stores it;
    //   4. UpdateCollisionBody re-posts box+padding to the generator's UpdateBody queue.
    //
    // ⚠️ MEASURED REDUNDANCY, recorded rather than smoothed: the console calls
    // GetVolumeInstanceIndexByID TWICE with identical arguments (0x828D354C and 0x828D35A0)
    // and the FIRST result is provably dead -- r3 is overwritten at 0x828D3574 by the sret
    // pointer before any use. It is a pure const hash lookup with no side effects, so the
    // single call below is behaviour-identical.
    if (lbHaveGenerationInput)
    {
        const s32 liCount = lpScene->mSetVolumeInstanceTransformQueue.GetLength();
        for (s32 liEvent = 0; liEvent < liCount; ++liEvent)
        {
            const SceneManagerIO::InEventSetVolumeInstanceTransform& lrEvent =
                lpScene->mSetVolumeInstanceTransformQueue.GetEvent(liEvent);

            const Vector3 lPositionDelta =
                mEntityManager.SetVolumeInstanceTransform(lrEvent.mVolumeInstanceId,
                                                          lrEvent.mTransform);

            const s32 liVolumeInstanceIndex =
                mEntityManager.GetVolumeInstanceIndexByID(lrEvent.mVolumeInstanceId);

            mEntityManager.SetVolumePadding(
                liVolumeInstanceIndex,
                rw::math::vpu::Clamp(lPositionDelta, KV_MIN_SWEPT_PADDING, KV_MAX_SWEPT_PADDING));

            UpdateCollisionBody(liVolumeInstanceIndex, lrEvent.mVolumeInstanceId,
                                lpOverlapGenerationInput);
        }
    }

    // ---- leg 19: explicit padding (X360 0x828D3614..0x828D369C, stride 0x20) ----------
    if (lbHaveGenerationInput)
    {
        const s32 liCount = lpScene->mSetPaddingQueue.GetLength();
        for (s32 liEvent = 0; liEvent < liCount; ++liEvent)
        {
            ProcessSetPaddingEvent(lpScene->mSetPaddingQueue.GetEvent(liEvent),
                                   lpOverlapGenerationInput);
        }
    }

    // ---- leg 20: clear an entity's padding (X360 0x828D36B8..0x828D3740, stride 4) ----
    if (lbHaveGenerationInput)
    {
        const s32 liCount = lpScene->mClearEntityPaddingQueue.GetLength();
        for (s32 liEvent = 0; liEvent < liCount; ++liEvent)
        {
            ProcessClearEntityPaddingEvent(lpScene->mClearEntityPaddingQueue.GetEvent(liEvent),
                                           lpOverlapGenerationInput);
        }
    }

    // ---- leg 21: force no padding (X360 0x828D375C..0x828D37E4, stride 8) -------------
    if (lbHaveGenerationInput)
    {
        const s32 liCount = lpScene->mForceNoPaddingQueue.GetLength();
        for (s32 liEvent = 0; liEvent < liCount; ++liEvent)
        {
            ProcessForceNoPaddingEvent(lpScene->mForceNoPaddingQueue.GetEvent(liEvent),
                                       lpOverlapGenerationInput);
        }
    }

    // ---- leg 22: add volume instance for caching -- PARKED, see §P3 -------------------

    // ---- leg 23: [lbPrepare] reposition the cached objects ----------------------------
    if (lbPrepare)
    {
        mTriangleCacheManager.ProcessUpdateCachedPositionEvents(lpScene->mUpdateCachedPositionQueue);
    }
}

// ===========================================================================
// SceneManagerModule::UpdateScene @ 0x828D4C28  (X360 vtbl+64)
//
//   1. StartMonitor(the UpdateScene CPU monitor);
//   2. four null tripwires;
//   3. CreateIOBuffer<SpatialPartitionIO::InputBuffer_Update>("SpatialPartition") and
//      <OverlapGenerationIO::InputBuffer>("OverlapGeneration") on the INPUT stack,
//      <SpatialPartitionIO::OutputBuffer> + <OverlapGenerationIO::OutputBuffer> on the
//      OUTPUT stack;
//   4. read-lock the scene input, write-lock both sub-module inputs, and fan the scene
//      input's update interface out through BridgeInputSceneUpdateInterfaceToSubModules,
//      then unlock in reverse;
//   5. SpatialPartitionManager::UpdateScene(spIn) -- drain the queue into the octree and
//      run its per-frame bounds update;
//   6. the overlap generator's own update;
//   7. write-lock the scene output and publish &mTriangleCacheManager on it;
//   8. destroy the four buffers; StopMonitor.
//
// ⭐ STEP 3's OVERLAP-GENERATION HALF IS LIVE AS OF 2026-08-19 (wave Q5 / E1a). The note
// that stood here said the generator's input buffer "would only be filled by the volume
// legs the bridge does not reconstruct" -- those legs now exist, so the buffer is created
// on the input stack, write-locked, threaded into the bridge and destroyed, exactly as the
// console does (0x828D4D3C CreateIOBuffer<OverlapGenerationIO::InputBuffer>("OverlapGeneration"),
// 0x828D4D7C LockForWrite, 0x828D4D94 the bridge call with it in r4, 0x828D4D98
// UnlockForWrite, 0x828D4E2C DestroyIOBuffer). Without it every AddForCollision event this
// frame would have had nowhere to go.
//
// FLAG, STILL TRIMMED -- step 3's SPATIAL-PARTITION half. The bridge applies the entity legs
// DIRECTLY to the partition (as the X360's own add leg does) rather than restaging them
// through a SpatialPartitionIO::InputBuffer_Update, so that buffer -- a 135 KB VEQ pushed on
// the input stack every frame -- is still not created, and neither are the two OUTPUT-side
// buffers this function's console form pushes (SpatialPartitionIO::OutputBuffer +
// OverlapGenerationIO::OutputBuffer): nothing in this tree reads either from here.
// UpdateContactGeneration creates its own generator OUTPUT buffer.
//
// ⭐ STEP 6 IS LIVE AS OF 2026-08-19 (wave Q5 / E1a) -- `mOverlapGenerator.Update(ogIn)`
// (X360 0x828D4DBC..0x828D4DD0: the module vtable slot +0x44, called with the generator
// input buffer in r4). It is the pass that REPLAYS the queues step 4 now fills --
// ProcessAddBodyQueue / ProcessUpdateBodyQueue / ProcessRemoveBodyQueue /
// ProcessForceNoPaddingQueue -> SceneSweeper::Add/Update/RemoveObject -- so without it the
// events the drain queues would be destroyed unread at the end of this function and the
// broad phase would still see nothing. It became callable only when cluster E2 retyped the
// declaration this same round (it was `void Update()`, a no-argument placeholder that does
// not exist on the console class and could not see the buffer).
//
// ⚠️ CONDUCTOR, HARD LINK DEPENDENCY: the real body is
// ContactGen/CgsOverlapGenerationModule.cpp:201 and that TU is NOT on build_game_exe.bat.
// Mount it in the same commit as this file, and delete the now-orphaned no-argument gate
// `void CgsSceneManager::OverlapGenerationModule::Update()` at WorldLinkStubs.cpp:1903-1918
// (it no longer matches any declaration, so it is a compile error there, not just dead code).
// Leave the Destruct gate above it alone -- that one still has no body anywhere.
//
// ⭐ STEP 7 IS LIVE AS OF 2026-08-11 (triangle-cache wiring wave). The note that used to
// stand here -- "the triangle-cache publish (step 7) has no committed consumer" -- is
// RETIRED: WorldModule::BridgeSceneQueryResultsToPhysics @0x827A8E88 and
// BridgeSceneModuleToOutput @0x827A5700 are both bodied now and both read this seat.
// ===========================================================================
bool SceneManagerModule::UpdateScene(CgsModule::IOBufferStack* lpInputBufferStack,
                                     CgsModule::IOBufferStack* lpOutputBufferStack,
                                     SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer,
                                     SceneManagerIO::OutputBuffer* lpSceneOutputBuffer,
                                     bool lbPrepare)
{
    ScopedPerfMon lUpdateScene(siUpdateScenePerfMon);

    CGS_ASSERT(lpInputBufferStack != NULL,  "lpInputBufferStack != NULL");
    CGS_ASSERT(lpOutputBufferStack != NULL, "lpOutputBufferStack != NULL");
    CGS_ASSERT(lpSceneInputBuffer != NULL,  "lpSceneInputBuffer != NULL");
    CGS_ASSERT(lpSceneOutputBuffer != NULL, "lpSceneOutputBuffer != NULL");

    if (lpSceneInputBuffer == NULL)
    {
        return true;
    }

    // Step 3 (generator half): push this frame's overlap-generation INPUT buffer. Failure is
    // possible on the host (the buffer is ~2.4 MB of event queues and the stack is finite);
    // CreateIOBuffer nulls the pointer and returns false, and the bridge's own guard then
    // skips the legs that need it rather than firing one assert per queued event.
    OverlapGenerationIO::InputBuffer* lpOverlapGenerationInput = nullptr;
    lpInputBufferStack->CreateIOBuffer(&lpOverlapGenerationInput, "OverlapGeneration");

    // NOTE the WRITE lock on the SCENE input: InputBuffer_Update::GetInSceneUpdateInterface
    // @0x825BD8C0 guards on the write bit (bit 3, "Not locked for writing\n"), because on the
    // console this buffer reaches UpdateScene still owned by the producer side. Read-locking
    // it here trips that tripwire on the first frame.
    // ⚠️ MEASURED DIVERGENCE, deliberately left alone: the CONSOLE read-locks it here
    // (0x828D4D64 `bl IOBuffer::LockForRead`) and its bridge reaches the interface through the
    // CONST accessor 0x828AF1C8, whose own tripwire is "Not locked for reading". The tree's
    // non-const accessor + write lock is a committed, runtime-witnessed pairing (the sibling
    // StartUpdateTriangleCache learned the same lesson from the other side: 927 asserts and a
    // wedged boot when it used the wrong overload). Flipping BOTH halves at once is the only
    // correct move and it is not this cluster's mandate -- recorded, not changed.
    lpSceneInputBuffer->LockForWrite();
    if (lpOverlapGenerationInput != nullptr)
    {
        lpOverlapGenerationInput->LockForWrite();
    }

    BridgeInputSceneUpdateInterfaceToSubModules(lpOverlapGenerationInput, NULL,
                                                lpSceneInputBuffer, lbPrepare);

    if (lpOverlapGenerationInput != nullptr)
    {
        lpOverlapGenerationInput->UnlockForWrite();
    }
    lpSceneInputBuffer->UnlockForWrite();

    // The partition's own per-frame pass (re-derive the loose bounds of every branch the
    // adds/removes flagged). The X360 reaches it through SpatialPartitionManager::
    // UpdateScene, which read-locks the (unused here) spatial-partition input first.
    SpatialPartition* lpPartition = mSpatialPartitionManager.GetSpatialPartition();
    if (lpPartition != NULL)
    {
        lpPartition->Update();
    }

    // Step 6: replay this frame's add/update/remove/force-no-padding queues into the
    // sweep-and-prune broad phase (see the STEP 6 note in the banner).
    if (lpOverlapGenerationInput != nullptr)
    {
        mOverlapGenerator.Update(lpOverlapGenerationInput);
    }

    // Step 7 (X360 0x828D4D8C..0x828D4DC4, byte-identical to ProcessSceneQueries' tail):
    // write-lock the scene output and publish &mTriangleCacheManager on it, so every scene
    // output buffer this module fills carries the cache handle -- not just the query one.
    lpSceneOutputBuffer->LockForWrite();
    lpSceneOutputBuffer->GetTriangleCacheInterface()->SetTriangleCacheManager(&mTriangleCacheManager);
    lpSceneOutputBuffer->UnlockForWrite();

    // Step 8 (the half this function pushes): pop the generator input buffer. The console
    // destroys it first of the four (0x828D4E2C).
    if (lpOverlapGenerationInput != nullptr)
    {
        lpInputBufferStack->DestroyIOBuffer(&lpOverlapGenerationInput);
    }

    return true;
}

// ===========================================================================
// SceneManagerModule::ProcessSceneQueries @ 0x828D57D0  (X360 vtbl+68)
//
// ⭐ RECONSTRUCTED 2026-08-11 (triangle-cache wiring wave); RETIRES the inert boot gate
// that stood at WorldLinkStubs.cpp:2350.
//
// THIS FUNCTION IS THE SOURCE OF THE ENTIRE TRIANGLE-CACHE CHAIN. Its last step is the
// ONLY write of a TriangleCacheManager* into a TriangleCacheInterface that reaches the
// physics module: every hop after it is an Append that ADOPTS an already-set pointer, so
// while this body was inert the physics side read an interface whose mpTriangleCacheManager
// had never been written -- the "mpTriangleCacheManager != NULL" assert plus the AV inside
// GetTrianglesForCachedObject that the traction-line leg was dying on.
//
// The X360 shell (CgsSceneManagerModule.cpp:806..), step for step:
//   1. StartMonitor(dword_82F33ECC);
//   2. four null tripwires (:806/:807/:808/:809);
//   3. StartMonitor(dword_82F33ED0); ProcessCoarseQueries @0x828CE770; StopMonitor;
//   4. StartMonitor(dword_82F33ED4); ProcessFineQueries   @0x828D5608; StopMonitor;
//   5. LockForWrite(sceneOut);
//      GetTriangleCacheInterface()          -- 0x828D592C -> SceneManagerIO::Output
//                                              @0x828AFAF8 (write-locked, +217164)
//      SetTriangleCacheManager(&mTriangleCacheManager)
//                                           -- 0x828D5920 addis r31,r28,0x3B ;
//                                              0x828D5928 addi r31,r31,-0x7DA0  == this+0x3A8260
//                                              0x828D5934 the inlined ":1268" tripwire
//                                              0x828D5960 stw r31, 0(r30)
//      UnlockForWrite(sceneOut); StopMonitor.
//
// FLAG (honest park, unchanged by this wave): steps 3 and 4 are NOT landed --
// ProcessCoarseQueries @0x828CE770 and ProcessFineQueries @0x828D5608 are their own console
// functions with their own TUs and neither is reconstructed. They are a no-op on this build
// anyway (both walk the coarse/fine query queues, and BridgePhysicsSceneQueriesToScene --
// the only producer that would fill them -- is still an inert gate), which is precisely why
// the publish can be landed on its own: it does not depend on either pass. DELETE-WHEN both
// query passes land; this body then calls them where the banner marks them.
// ===========================================================================
void SceneManagerModule::ProcessSceneQueries(CgsModule::IOBufferStack* lpInputBufferStack,
                                             CgsModule::IOBufferStack* lpOutputBufferStack,
                                             SceneManagerIO::InputBuffer_Query* lpQueryInput,
                                             SceneManagerIO::OutputBuffer* lpQueryOutput)
{
    ScopedPerfMon lProcessSceneQueries(siProcessSceneQueriesPerfMon);

    CGS_ASSERT(lpInputBufferStack != NULL,  "lpInputBufferStack != NULL");    // :806
    CGS_ASSERT(lpOutputBufferStack != NULL, "lpOutputBufferStack != NULL");   // :807
    CGS_ASSERT(lpQueryInput != NULL,        "lpSceneInputBuffer != NULL");    // :808
    CGS_ASSERT(lpQueryOutput != NULL,       "lpSceneOutputBuffer != NULL");   // :809

    if (lpQueryOutput == NULL)
    {
        return;
    }

    // (steps 3/4 -- ProcessCoarseQueries @0x828CE770 / ProcessFineQueries @0x828D5608 --
    //  go here when they land; see the FLAG above.)

    // Step 5: publish the triangle-cache manager on the scene output. This is the handoff
    // the world bridges then carry into the physics vehicle input and the world output.
    lpQueryOutput->LockForWrite();
    lpQueryOutput->GetTriangleCacheInterface()->SetTriangleCacheManager(&mTriangleCacheManager);
    lpQueryOutput->UnlockForWrite();
}

// ===========================================================================
// SceneManagerModule::ProcessFrustumTestJobRequests @ 0x828C7628
//
// Walk the frame's staged coarse-query events and hand each one to the octree's job
// system, then kick the jobs:
//   for each event in the query input's InCoarseQueryQueue:
//       assert(id == E_IN_EVENT_FRUSTUM_TEST_VP);
//       maFrustumTestJobQueryIds[queryIndex] = event->mQueryId;      // event +0xC0
//       assert(KA_FRUSTUM_QUERY_JOB_INDEX is non-decreasing);
//       LooseOctree::AddJobFrustumTest(event->mx32EntityTypeFlags,   // event +0xC4
//                                      &event->maFrustumPlanes,      // event +0x40
//                                      &event->mViewProjection,      // event +0x00
//                                      KA_FRUSTUM_QUERY_JOB_INDEX[queryIndex]);
//   LooseOctree::StartFrustumTestJobs();
//
// The per-query id array is what ProcessFrustumTestJobResults stamps each result batch
// with, so the batches come back in submission order.
//
// FLAG: KA_FRUSTUM_QUERY_JOB_INDEX (X360 unk_82F33E48) is a static table with no writer,
// and the exports carry no data section, so its contents cannot be read back. Its ONE
// attested property is the assert right beside it -- it is non-decreasing -- and that is
// also the only property the results depend on: WaitForFrustumTestJobResults drains jobs
// in index order and each job's queries in submission order, so a non-decreasing map is
// exactly the condition for the result batches to come back in query order. The map below
// spreads the queries evenly over the four jobs (which also keeps each job inside its
// KU_JOB_RESULT_BUFFER_SIZE run pool); any other non-decreasing map produces the same
// result stream.
// ===========================================================================
void SceneManagerModule::ProcessFrustumTestJobRequests(CgsModule::IOBufferStack* lpInputBufferStack,
                                                       CgsModule::IOBufferStack* lpOutputBufferStack,
                                                       SceneManagerIO::InputBuffer_Query* lpQueryInput,
                                                       SceneManagerIO::OutputBuffer* lpQueryOutput)
{
    ScopedPerfMon lProcessSceneQueries(siProcessSceneQueriesPerfMon);

    // (The two buffer-stack tripwires the X360 fires here are omitted while the
    //  bring-up dispatch producer is the caller: it has no stacks -- see the
    //  FLAG in ProcessFrustumTestJobResults -- and neither entry point uses them
    //  on this path. Restore them with BrnGameModule::DoDispatch.)
    CGS_ASSERT(lpQueryInput != NULL,        "lpSceneInputBuffer != NULL");
    CGS_ASSERT(lpQueryOutput != NULL,       "lpSceneOutputBuffer != NULL");
    (void)lpInputBufferStack;
    (void)lpOutputBufferStack;

    ScopedPerfMon lCoarse(siProcessCoarseQueriesPerfMon);

    LooseOctree* lpOctree =
        static_cast<LooseOctree*>(mSpatialPartitionManager.GetSpatialPartition());
    if (lpQueryInput == NULL || lpOctree == NULL)
    {
        return;
    }

    lpQueryInput->LockForRead();

    const SceneManagerIO::InCoarseQueryQueue<16384>* lpQueue = lpQueryInput->GetInCoarseQueryQueue();

    const CgsModule::Event* lpEvent = NULL;
    s32 liSize = 0;
    s32 liId = lpQueue->GetFirstEvent(&lpEvent, &liSize);

    u32 luQueryIndex = 0;
    while (liId >= 0)
    {
        CGS_ASSERT(liId == SceneManagerIO::E_IN_EVENT_FRUSTUM_TEST_VP,
                   "liId == SceneManagerIO::E_IN_EVENT_FRUSTUM_TEST_VP");

        if (luQueryIndex >= KU_MAX_FRUSTUM_TEST_JOB_QUERIES)
        {
            break;
        }

        const SceneManagerIO::InEventFrustumTestVp* lpQuery =
            static_cast<const SceneManagerIO::InEventFrustumTestVp*>(lpEvent);

        maFrustumTestJobQueryIds[luQueryIndex] = lpQuery->mQueryId;

        lpOctree->AddJobFrustumTest(
            lpQuery->mx32EntityTypeFlags,
            reinterpret_cast<const CgsGeometric::Frustum*>(lpQuery->maFrustumPlanes),
            &lpQuery->mViewProjection,
            (luQueryIndex * KU_NUM_FRUSTUM_TEST_JOBS) / KU_MAX_FRUSTUM_TEST_JOB_QUERIES);

        liId = lpQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
        ++luQueryIndex;
    }

    lpQueryInput->UnlockForRead();

    lpOctree->StartFrustumTestJobs();
}

// ===========================================================================
// SceneManagerModule::ProcessFrustumTestJobResults @ 0x828C7838
//
// Drain the loose-octree frustum-test job results into the scene output event queue.
// Bracketed by the ProcessSceneQueries + Coarse perfmon regions. For each batch of
// coarse results the octree produced, allocate a variable event in the scene output
// queue and copy the resolved entity ids out, asserting each index/id is valid.
//
// SCOPE NOTE: the per-result entity-id resolution + the variable-event queue copy go
// through the SpatialPartition coarse-result-buffer + EntityManager accessors whose
// X360 names are truncated in the IDB (CgsSceneManager::Scen / SceneManager /
// CoarseQueryResultBuffer<16384>::GetBatchByOffset). The buffer plumbing the
// SceneManagerModule owns is reconstructed here; the truncated-name accessors are the
// documented boundary (their full homes are the SpatialPartition / Entity TUs).
// ===========================================================================
void SceneManagerModule::ProcessFrustumTestJobResults(CgsModule::IOBufferStack* lpInputBufferStack,
                                                      CgsModule::IOBufferStack* lpOutputBufferStack,
                                                      SceneManagerIO::InputBuffer_Query*  lpSceneInputBuffer,
                                                      SceneManagerIO::OutputBuffer*  lpSceneOutputBuffer)
{
    ScopedPerfMon lProcessSceneQueries(siProcessSceneQueriesPerfMon);

    // (see ProcessFrustumTestJobRequests on the two omitted stack tripwires)
    CGS_ASSERT(lpSceneInputBuffer != NULL,  "lpSceneInputBuffer != NULL");
    CGS_ASSERT(lpSceneOutputBuffer != NULL, "lpSceneOutputBuffer != NULL");
    (void)lpInputBufferStack;

    ScopedPerfMon lCoarse(siProcessCoarseQueriesPerfMon);

    if (lpSceneInputBuffer == NULL || lpSceneOutputBuffer == NULL)
    {
        return;
    }

    // The X360 opens with the W+R helper sub_823B6FE0(sceneOut, sceneIn): the scene
    // OUTPUT is write-locked (the results ring is about to be filled) and the scene INPUT
    // read-locked; the tail unlocks both.
    reinterpret_cast<CgsModule::IOBuffer*>(lpSceneOutputBuffer)->LockForWrite();
    reinterpret_cast<CgsModule::IOBuffer*>(lpSceneInputBuffer)->LockForRead();

    CgsModule::IOBufferStack* lpOutStack = lpOutputBufferStack;

    // The octree writes its frustum-test results into a SpatialPartition output buffer
    // pushed on the output stack; the SceneManagerModule reads the resolved entity ids
    // out and re-emits them into the scene output event queue.
    SpatialPartitionIO::OutputBuffer* lpResultBuffer = nullptr;
    if (lpOutStack != nullptr)
    {
        lpOutStack->CreateIOBuffer(&lpResultBuffer, "SpacialPartition");
    }
    else
    {
        // [FLAG PC bring-up] the bring-up dispatch producer has no IO buffer stacks (the
        // console's BrnGameModule::DoDispatch @0x823DC458 creates them; that spine is not
        // live yet), so the coarse-result buffer -- a 53 KB frame-local on the console --
        // is a file static here. Retire with the bring-up producer.
        static SpatialPartitionIO::OutputBuffer sStandInResultBuffer;
        static bool sbStandInConstructed = false;
        if (!sbStandInConstructed)
        {
            sbStandInConstructed = true;
            sStandInResultBuffer.Construct();
        }
        lpResultBuffer = &sStandInResultBuffer;
    }
    if (lpResultBuffer == nullptr)
    {
        return;
    }

    reinterpret_cast<CgsModule::IOBuffer*>(lpResultBuffer)->LockForWrite();

    CoarseQueryResultBuffer<16384>* lpResults = lpResultBuffer->GetCoarseResultBuffer();
    lpResults->Construct();

    LooseOctree* lpOctree =
        static_cast<LooseOctree*>(mSpatialPartitionManager.GetSpatialPartition());
    if (lpOctree != NULL)
    {
        lpOctree->WaitForFrustumTestJobResults(lpResults);
    }

    // One scene-output event per coarse-result BATCH, stamped with that query's id (the
    // batches come back in submission order, so batch i belongs to query i):
    //   Event* e = queue.AllocateEvent(0, 4 * (numResults + 3));
    //   e[0] = query id; e[1] = e[2] = numResults; e[3..] = one EntityId per result
    // (the octree hands back pool INDICES, so each is resolved through the entity
    // manager on the way out).
    {
        u32 luOffset      = 0;
        u32 luNumResults  = 0;
        const u32 luNumBatches = lpResults->GetNumBatches();

        for (u32 luBatch = 0; luBatch < luNumBatches; ++luBatch)
        {
            const SceneQueryId lQueryId = maFrustumTestJobQueryIds[
                (luBatch < KU_MAX_FRUSTUM_TEST_JOB_QUERIES) ? luBatch : 0];

            const u16* lpu16Results =
                lpResults->GetBatchByOffset(luOffset, &luOffset, &luNumResults);
            CGS_ASSERT(lpu16Results != NULL, "lpu16Results != NULL");
            if (lpu16Results == NULL)
            {
                break;
            }

            SceneManagerIO::OutCoarseQueryResult* lpEvent =
                static_cast<SceneManagerIO::OutCoarseQueryResult*>(
                    lpSceneOutputBuffer->GetSceneQueryResultsQueueForWrite()->AllocateEventSafe(
                        SceneManagerIO::OutCoarseQueryResult::KI_EVENT_TYPE,
                        static_cast<s32>(4 * (luNumResults + 3))));
            if (lpEvent == NULL)
            {
                break;   // the results ring is full for this frame
            }

            lpEvent->mQueryId              = lQueryId;
            lpEvent->miNumResults          = static_cast<s32>(luNumResults);
            lpEvent->miNumResultsAttempted = static_cast<s32>(luNumResults);

            EntityId* lpIds = lpEvent->GetEntityIds();
            for (u32 luResult = 0; luResult < luNumResults; ++luResult)
            {
                const u16 lu16Index = lpu16Results[luResult];
                CGS_ASSERT(lu16Index < KI_MAX_NUM_ENTITIES, "lu16Index < KI_MAX_NUM_ENTITIES");

                const EntityId lId = mEntityManager.GetEntityIdByIndex(lu16Index);
                CGS_ASSERT(lId != K_INVALID_ENTITY_ID, "lID != K_INVALID_ENTITY_ID");

                lpIds[luResult] = lId;
            }
        }
    }

    reinterpret_cast<CgsModule::IOBuffer*>(lpResultBuffer)->UnlockForWrite();

    CGS_ASSERT(lpSceneOutputBuffer != NULL, "lpInputBuffer");
    CGS_ASSERT(lpSceneInputBuffer != NULL, "lpOutputBuffer0");
    reinterpret_cast<CgsModule::IOBuffer*>(lpSceneInputBuffer)->UnlockForRead();
    reinterpret_cast<CgsModule::IOBuffer*>(lpSceneOutputBuffer)->UnlockForWrite();

    if (lpOutStack != nullptr)
    {
        lpOutStack->DestroyIOBuffer(&lpResultBuffer);
    }
}

// ===========================================================================
// SceneManagerModule::AddBody @ 0x828BA498
//
// Producer side of the overlap-generation add-body path: queue a 64-byte InAddBody
// request on the overlap-generation input buffer, then record the object's culling
// group in the culling-group manager's per-object array.
//
// The X360 emits this function with OverlapGenerationIO::InputBuffer::AddBody inlined
// into it, which is why the whole event assembly is visible here store-for-store:
//     ld/std x4 off r6 -> mAabb  +0x00..+0x1F   (the caller's rw::collision::AABBox)
//     stw r7           -> mCullGroup            +0x20
//     stw r5           -> muIndex               +0x24
//     stw r8           -> meBodyState           +0x28
//     std r9           -> mVolumeInstanceID     +0x30
// then the truncated `CgsSceneMana(a2)` callee -- the non-const
// OverlapGenerationIO::InputBuffer::GetAddBodyQueue accessor @0x828B0188 (== a2+16,
// the queue is the first member after the IOBuffer status byte) -- and
// BaseEventQueue<InAddBody>::AddEvent @0x828B85D8, which copies the 64 bytes wholesale.
// ⚠️ CORRECTED 2026-08-18 (D1): the previous banner claimed the +0x20 culling-group lane
// "sits inside the event's leading AABB span" and therefore had no named field. It does
// not -- the box ends at +0x1F and +0x20 is InAddBody::mCullGroup (DWARF
// CgsOverlapGenerationModuleIO.h:57). That misreading is what produced the 36-byte
// maAABBox stand-in the wave retired.
// The trailing index assert is a non-gating tripwire; the culling-group byte store runs
// unconditionally after it (X360 stbx past the blt).
// ===========================================================================
void SceneManagerModule::AddBody(OverlapGenerationIO::InputBuffer* lpOverlapGenerationInputBuffer,
                                 u32                          luObjectIndex,
                                 const rw::collision::AABBox* lpAabb,
                                 OverlapGenerationIO::InAddBody::CullingGroup lCullingGroup,
                                 rw::physics::BodyState       leBodyState,
                                 VolumeInstanceId             lVolumeInstanceId)
{
    // ⭐ REWRITTEN 2026-08-18 (wave Q5 cluster D1, OverlapGenerationModuleIO owner).
    // What used to be here was a hand-assembled 64-byte image poked through a u8* view
    // (four qword copies, then `*(u32*)(bytes + 0x20) = luCullingGroup`), because the
    // event type was a stand-in whose `u8 maAABBox[0x24]` swallowed the culling-group
    // word. That stand-in is retired: OverlapGenerationIO::InAddBody now has the DWARF
    // field set (mAabb / mCullGroup / muIndex / meBodyState / mVolumeInstanceID) and the
    // assembly itself lives where the console put it -- InputBuffer::AddBody
    // (CgsOverlapGenerationModuleIO.h:200), which the X360 inlines into THIS function.
    // So this body is now the console's own two halves, in order: the producer call, then
    // the culling-group bookkeeping.
    //
    // ⭐ SIGNATURE RETYPED 2026-08-19 (wave Q5 cluster E1a), taking up the request D1 left
    // here: the last three parameters were pre-recovery placeholders that this body had to
    // undo by cast (`const void*` / a u32 misnamed luVolumeHandle / a bare u64). They are
    // now the DWARF's and the X360 register usage's own types, so the casts are gone and
    // the compiler -- not a comment -- enforces that a BodyState never arrives where a
    // culling group belongs. See the declaration in CgsSceneManagerModule.h.
    lpOverlapGenerationInputBuffer->AddBody(luObjectIndex, lpAabb, lCullingGroup,
                                            leBodyState, lVolumeInstanceId);

    // Record the object's culling group (non-gating index tripwire, then the store
    // runs unconditionally -- X360 stbx sits past the assert's branch).
    CGS_ASSERT(luObjectIndex < 0x13BBu, "luObjectIndex < (uint32_t)SceneSweeper::KU_MAX_NUM_OBJECTS");
    mCullingGroupManager.GetVolumeInstanceCullingGroup()[luObjectIndex] =
        static_cast<u8>(lCullingGroup);
}

// ===========================================================================
// SceneManagerModule::ProcessAddForCollisionEvent @ 0x828CE898  (487 insns)
//   console home CgsSceneManagerModule.cpp:937; its asserts bake lines 939 / 955-961 / 977
//
// ⭐ THE EVENT THAT PUTS A VOLUME INTO THE BROAD PHASE. PropCellManager::
// AddPropToContactGeneration @0x822DF6C8 and ActiveRaceCar::AddToCollision @0x822D41F0 both
// end in InSceneUpdateInterface::AddForCollision; this is the consumer that turns that queued
// request into an OverlapGeneration InAddBody.
//
// X360 body, step for step:
//   assert(lpOverlapGenerationInputBuffer != NULL)                          (:939)
//   liIndex = mEntityManager.GetVolumeInstanceIndexByID(event.mVolumeInstanceId)
//                                                         (`ld r4,0x10(r26)` @0x828CE8E8)
//   if (liIndex == -1) { assert("Volume instance not found for volume instance id ") (:977);
//                        return; }                          (the 0x828CEF98 arm)
//   mEntityManager.SetVolumeForCollision(event.mVolumeInstanceId, true)   -- sets the
//                                        VolumeInstance COLLIDABLE bit (mx8Flags bit 0)
//   lpInstance = mEntityManager.GetVolumeInstance(liIndex)   (sub_828B9FD8 @0x828CE91C)
//   lpVolume   = mVolumeManager.GetRwVolume(lpInstance->miVolumeIndex)  (`lwz r4,0x5C(r31)`)
//   lpVolume->GetBBox(&lpInstance->mWorldSpaceTransform, /*tight*/1, lBox)
//        -- dispatched through the rwcollision per-TYPE descriptor at volume+0x40, slot
//           +0x04 (`lwz r11,0x40(r3) ; lwz r11,4(r11) ; mtctr ; bctrl` @0x828CE934). NOTE
//           the transform argument is the VOLUME INSTANCE POINTER itself: mWorldSpaceTransform
//           is the record's first 64 bytes, so &instance == &instance.mWorldSpaceTransform.
//           The RwBool result is not tested (same as every other GetBBox call site).
//   six dev tripwires on that box + the instance's matrix, then
//   AddBody(lpInput, liIndex, &lBox, event.mCullGroup, event.GetBodyState(),
//           event.mVolumeInstanceId)                                    (0x828CEF74)
//   mEntityManager.SetVolumePadding(liIndex, event.mPadding)  (`lvx128 v1, r0, r26` -- the
//                                                              event's own first 16 bytes)
//
// ---- the six tripwires, in the console's order and on the console's lanes --------------
//   :955 "Invalid transform for volume instance id "  -- FOUR `lvx128` + per-lane
//        `vcmpeqfp.` self-compares over the instance's matrix rows at +0x00/+0x10/+0x20/+0x30,
//        lanes x/y/z of each; the four row results are ANDed (`v66 & v52 & v38 & v24`). A
//        self-compare is the NaN test, so this is "no lane of the world matrix is NaN".
//   :956/:957/:958 "Bounding box with zero width|height|depth for volume instance id "
//        -- `vsubfp max-min ; vandc <sign bit> ; vcmpgtfp` against a splat of *0x820F25D0
//        (== FLT_EPSILON), i.e. !IsZero(extent), on lanes 0/1/2 respectively.
//   :959/:960/:961 "Inside out bounding box for volume instance id " -- `vcmpgefp. max,min`
//        with CR6 "all true", on lanes 0 (X), 2 (Z), 1 (Y). The X/Z/Y order is the console's
//        own and is NOT a transcription slip; the identical six-tripwire block in
//        VolumeManager::AddDynamicVolume @0x828D1708 has the same order and the DWARF names
//        its comparators operator<=<VectorAxisX>, <VectorAxisZ>, <VectorAxisY>.
// Every one is non-gating: the console fires the assert and falls through to AddBody.
//
// The console streams the offending VolumeInstanceId onto each message through StrStream;
// CGS_ASSERT takes a plain string here, so the message stops at the string -- the same
// reduction every other reconstructed TU makes (and the reason the strings keep their
// trailing space).
//
// PRE-CONDITION, MEASURED 2026-08-19 AND NOW SATISFIED: GetBBox dispatches through
// rw::collision::gVolumeVTable[type]. Both halves of the rwcollision mount have landed --
// rw::collision::Volume::InitializeVTable is static with a REAL body (its WorldLinkStubs
// `return 0` gate was retired 2026-08-18, so the table is filled, not all-zero), and the
// six descriptor records with their bound method slots live in
// vendor/renderware/collision/VolumeVTables.cpp, which is MOUNTED at
// tools/build/build_game_exe.bat:2092. Their getBBox slots are among the bound ones, so a
// volume reaching this body no longer null-calls. (34 of the 40 image-bound method slots
// across the six records are live; 2 are genuine image zeros and 6 are parked with
// per-slot reasons -- see the banner in VolumeVTables.cpp.) A null guard the console does
// not have would only hide breakage, so there is still none.
// ===========================================================================
void SceneManagerModule::ProcessAddForCollisionEvent(
    const SceneManagerIO::InEventAddForCollision& lrEvent,
    OverlapGenerationIO::InputBuffer* lpOverlapGenerationInput)
{
    CGS_ASSERT(lpOverlapGenerationInput != NULL, "lpOverlapGenerationInputBuffer != NULL");  // :939

    const s32 liVolumeInstanceIndex =
        mEntityManager.GetVolumeInstanceIndexByID(lrEvent.mVolumeInstanceId);
    if (liVolumeInstanceIndex == KI_INVALID_VOLUME_INSTANCE_INDEX)
    {
        CGS_ASSERT(false, "Volume instance not found for volume instance id ");             // :977
        return;
    }

    mEntityManager.SetVolumeForCollision(lrEvent.mVolumeInstanceId, true);

    VolumeInstance* lpVolumeInstance = mEntityManager.GetVolumeInstance(liVolumeInstanceIndex);
    const VolRef::Volume* lpVolume = mVolumeManager.GetRwVolume(lpVolumeInstance->miVolumeIndex);

    // The scene manager's opaque VolRef::Volume IS rw::collision::Volume (DecFIGS volume.h:39
    // typedefs exactly that) -- two spellings of one type, not a layout reinterpretation. The
    // box is the AABBox byte image (see the include note at the top of this TU).
    OverlapGenerationIO::AABBoxRows lBox;
    reinterpret_cast<const rw::collision::Volume*>(lpVolume)->GetBBox(
        &lpVolumeInstance->mWorldSpaceTransform, 1,
        *reinterpret_cast<rw::collision::AABBox*>(&lBox));

    // rw::math::vpu::IsValid(Vector3) IS the console's per-row test: lanes X/Y/Z, each a
    // `vcmpeqfp.` self-equality (a NaN never equals itself). Four rows, ANDed.
    const Matrix44Affine& lrTransform = lpVolumeInstance->mWorldSpaceTransform;
    const bool lbTransformValid =
        rw::math::vpu::IsValid(lrTransform.xAxis) && rw::math::vpu::IsValid(lrTransform.yAxis) &&
        rw::math::vpu::IsValid(lrTransform.zAxis) && rw::math::vpu::IsValid(lrTransform.wAxis);
    CGS_ASSERT(lbTransformValid, "Invalid transform for volume instance id ");               // :955

    CGS_ASSERT(!rw::math::fpu::IsZero(lBox.mMax.x - lBox.mMin.x),
               "Bounding box with zero width for volume instance id ");                      // :956
    CGS_ASSERT(!rw::math::fpu::IsZero(lBox.mMax.y - lBox.mMin.y),
               "Bounding box with zero height for volume instance id ");                     // :957
    CGS_ASSERT(!rw::math::fpu::IsZero(lBox.mMax.z - lBox.mMin.z),
               "Bounding box with zero depth for volume instance id ");                      // :958
    CGS_ASSERT(lBox.mMin.x <= lBox.mMax.x, "Inside out bounding box for volume instance id ");// :959
    CGS_ASSERT(lBox.mMin.z <= lBox.mMax.z, "Inside out bounding box for volume instance id ");// :960
    CGS_ASSERT(lBox.mMin.y <= lBox.mMax.y, "Inside out bounding box for volume instance id ");// :961

    AddBody(lpOverlapGenerationInput,
            static_cast<u32>(liVolumeInstanceIndex),
            reinterpret_cast<const rw::collision::AABBox*>(&lBox),
            lrEvent.mCullGroup,
            lrEvent.GetBodyState(),
            lrEvent.mVolumeInstanceId);

    mEntityManager.SetVolumePadding(liVolumeInstanceIndex, lrEvent.mPadding);
}

// ===========================================================================
// SceneManagerModule::ProcessRemoveForCollisionEvent @ 0x828CF828  (48 insns)
//   console home CgsSceneManagerModule.cpp:1045; asserts bake lines 1047 and 1060
//
// The mirror of the above, and the ONLY thing that takes a body back out of the sweeper:
//   assert(lpOverlapGenerationInputBuffer != NULL)                        (:1047)
//   liIndex = mEntityManager.GetVolumeInstanceIndexByID(event.muCollisionId)
//                                                       (`ld r4,0(r31)` @0x828CF870)
//   if (liIndex == -1) { assert("Volume instance not found") (:1060); return; }  -- GATING:
//        0x828CF888 `beq cr6, loc_828CF8C4` jumps PAST everything to the epilogue.
//   mEntityManager.SetVolumeForCollision(id, false)   -- clears the COLLIDABLE bit
//   lpInput->RemoveBody(liIndex, id)   -- the console inlines the producer here:
//        `std r11, var_40` (the id at InRemoveBody +0x00), `stw r29, var_38` (the index at
//        +0x08), sub_828B02D8 == the non-const GetRemoveBodyQueue, then
//        BaseEventQueue<InRemoveBody>::AddEvent @0x828B8888.
//
// ⚠️ The event's payload is spelled `u64 muCollisionId` in this tree's
// CgsSceneManagerIO_EventRemoveForCollision.h; it IS a VolumeInstanceId (the producer
// InSceneUpdateInterface::RemoveForCollision(VolumeInstanceId) stores the whole 64-bit id
// into it, CgsSceneManagerIO_SceneUpdate.h:208-216). Rebuilt by name here rather than
// retyped, because that element header is not this cluster's file.
// ===========================================================================
void SceneManagerModule::ProcessRemoveForCollisionEvent(
    const SceneManagerIO::InEventRemoveForCollision& lrEvent,
    OverlapGenerationIO::InputBuffer* lpOverlapGenerationInput)
{
    CGS_ASSERT(lpOverlapGenerationInput != NULL, "lpOverlapGenerationInputBuffer != NULL");  // :1047

    VolumeInstanceId lVolumeInstanceId;
    lVolumeInstanceId.muId = lrEvent.muCollisionId;

    const s32 liVolumeInstanceIndex =
        mEntityManager.GetVolumeInstanceIndexByID(lVolumeInstanceId);
    if (liVolumeInstanceIndex == KI_INVALID_VOLUME_INSTANCE_INDEX)
    {
        CGS_ASSERT(false, "Volume instance not found");                                     // :1060
        return;
    }

    mEntityManager.SetVolumeForCollision(lVolumeInstanceId, false);

    lpOverlapGenerationInput->RemoveBody(static_cast<u32>(liVolumeInstanceIndex),
                                         lVolumeInstanceId);
}

// ===========================================================================
// SceneManagerModule::RemoveAllEntityVolumeInstances @ 0x828CD9A0
//   (DWARF CgsSceneManagerModule.h:439, console body CgsSceneManagerBridgeFunctions.cpp:430)
//
// ⭐ WAVE Q5 / CLUSTER RMLEG (2026-08-19) -- LANDED. This is the volume half of drain
// leg 4 and the per-entity worker of leg 5, both parked by round 3 (cluster E1a, §P2).
//
// Walk the entity's singly-linked volume-instance chain and retire EVERY instance on it:
// take each collidable one off the broad phase, then unlink + free the record.
//
// ---- THE X360 SPINE, store for store -------------------------------------------------
//   0x828CD9AC  r30 = this + 0x1A2480  == &mEntityManager (reached by NAME here)
//   0x828CD9C0  bl GetFirstEntityVolumeInstance(lu16EntityIndex /*r4, passed through*/,
//                                               &liVolumeInstanceIndex /*r5 = stack out*/)
//   0x828CD9CC  null instance -> return
//   loc_828CD9E4 (the loop head):
//   0x828CD9E4  lbz r11,0x6A(r31) ; clrlwi r11,r11,31   == VolumeInstance::IsCollidable()
//   0x828CD9F0  not collidable -> skip straight to the unlink at loc_828CDA24
//   0x828CDA00  bl SetVolumeForCollisionByIndex(liVolumeInstanceIndex, false)
//   0x828CDA04  ld r11,0x50(r31)  == the record's mUserID (the VolumeInstanceId)
//   0x828CDA14/0x828CDA0C  std id / stw index -> a stack InRemoveBody { id +0x00, index +0x08 }
//   0x828CDA18  bl 0x828B02D8 == OverlapGenerationIO::InputBuffer::GetRemoveBodyQueue
//   0x828CDA20  bl EventQueue<InRemoveBody,16384>::AddEvent
//               -- the two together ARE InputBuffer::RemoveBody(index, id), which this tree
//               already has out of line (CgsOverlapGenerationModuleIO_InputBuffer.cpp:133).
//   0x828CDA28  ld r28,0x50(r31)   -- the id re-read BEFORE the record is disturbed
//   0x828CDA30  bl RemoveVolumeInstanceFromEntity(index)   ] EntityManager::
//   0x828CDA3C  bl IndexedHashTable<...,509>::Remove(id)   ]   RemoveVolumeInstanceByIndex
//   0x828CDA48  bl ObjectPool<VolumeInstance,5048,int>::FreeObject(index) ] (id, index)
//   0x828CDA4C  lwz r4,0x60(r31)   -- miNextEntityVolumeInstance, read off the JUST-FREED
//               record (FreeObject only clears the occupancy bit, it does not scrub the
//               slot) -- reproduced, this is the console's own iteration
//   0x828CDA58  bl GetVolumeInstance(next) ; loop while non-null
//
// ⚠️ The three-call trio at the tail is NOT open-coded here: it is exactly
// EntityManager::RemoveVolumeInstanceByIndex (DWARF h:126), landed this same wave in
// CgsEntityManager.cpp because two of its three calls touch private EntityManager members.
// That ownership -- nothing else -- is why this function sat parked.
// ===========================================================================
void SceneManagerModule::RemoveAllEntityVolumeInstances(
    u16 lu16EntityIndex, OverlapGenerationIO::InputBuffer* lpOverlapGenerationInput)
{
    s32 liVolumeInstanceIndex = KI_INVALID_VOLUME_INSTANCE_INDEX;
    const VolumeInstance* lpVolumeInstance =
        mEntityManager.GetFirstEntityVolumeInstance(lu16EntityIndex, &liVolumeInstanceIndex);

    while (lpVolumeInstance != NULL)
    {
        // The id has to be taken off the record before RemoveVolumeInstanceByIndex runs
        // (the console re-loads it at 0x828CDA28 for the same reason).
        const VolumeInstanceId lVolumeInstanceId = lpVolumeInstance->mUserID;

        if (lpVolumeInstance->IsCollidable())                                 // 0x828CD9E4
        {
            mEntityManager.SetVolumeForCollisionByIndex(liVolumeInstanceIndex, false);  // 0x828CDA00

            // FLAG PC (not console): the console loads the buffer pointer and calls through
            // it unconditionally. On the host CreateIOBuffer<InputBuffer> can fail (the
            // buffer is ~2.4 MB) and the drain already announces that once -- see the
            // lbHaveGenerationInput note in BridgeInputSceneUpdateInterfaceToSubModules.
            // The guard is on the SWEEPER post ONLY: the unlink/free below must still run,
            // or a missing IO buffer would turn every entity removal into a permanent
            // volume-instance leak plus a duplicate-key Insert on the next AddVolumeInstance.
            if (lpOverlapGenerationInput != NULL)
            {
                lpOverlapGenerationInput->RemoveBody(static_cast<u32>(liVolumeInstanceIndex),
                                                     lVolumeInstanceId);      // 0x828CDA18/0x828CDA20
            }
        }

        mEntityManager.RemoveVolumeInstanceByIndex(lVolumeInstanceId,
                                                   liVolumeInstanceIndex);    // 0x828CDA30..0x828CDA48

        liVolumeInstanceIndex = lpVolumeInstance->miNextEntityVolumeInstance;  // 0x828CDA4C
        lpVolumeInstance = mEntityManager.GetVolumeInstance(liVolumeInstanceIndex);  // 0x828CDA58
    }
}

// ===========================================================================
// SceneManagerModule::RemoveAllOwnerVolumeInstances @ 0x828CDA70
//   (DWARF CgsSceneManagerModule.h:442, console body CgsSceneManagerBridgeFunctions.cpp:460)
//
// ⭐ WAVE Q5 / CLUSTER RMLEG (2026-08-19) -- LANDED. Drain leg 5's whole body: retire
// EVERY entity whose EntityId owner byte matches, volumes first. This is what an
// InSceneUpdateInterface::RemoveAllEntities(owner) request resolves to.
//
// ---- THE X360 SPINE ------------------------------------------------------------------
//   0x828CDA84  r29 = this + 0x1A2480 == &mEntityManager (== &mEntityPool, its first member)
//   0x828CDA94  bl ObjectPool<SceneManagerEntity,10000,int>::GetFirstObjectIndex
//               -- reached here by NAME through EntityManager::GetFirstEntityIndex (DWARF
//                  h:216), landed this wave; the pool is private.
//   0x828CDA9C  -1 -> return
//   loc_828CDAC0 (the loop head):
//   0x828CDAC0  r31 = index & 0xFFFF
//   0x828CDAC8  assert "lu16Index < KI_MAX_NUM_ENTITIES" (line 301)
//   0x828CDAF0  bl `Scen`(&em, index) -> &mEntityPool[index] ; lwz 0(r3) ; srwi r11,r11,24
//               == EntityId::GetOwner() -- read through GetEntityIdByIndex here, which
//               carries that same bound assert.
//   0x828CDB00  owner mismatch -> next entity
//   0x828CDB10  bl SceneManagerModule::RemoveAllEntityVolumeInstances(index, inputBuffer)
//   0x828CDB14  lwz r3,0x280(this) ; vtable +0x3C ; bctrl
//               == SpatialPartition::RemoveEntityFromGraph(index) -- the SAME slot drain
//                  leg 4 calls at 0x828D27B8.
//   0x828CDB30  assert "lu16Index < KI_MAX_NUM_ENTITIES" (line 147, same string)
//   0x828CDB58  bl EntityManager::RemoveEntity(index)
//   0x828CDB64  bl GetNextObjectIndex(r28 /* the ORIGINAL, pre-mask index */) ; loop
//
// ⚠️ MUTATING THE POOL MID-WALK IS THE CONSOLE'S OWN PATTERN and is safe: the scan
// resumes strictly ABOVE the index just handed to it, so clearing that slot's occupancy
// bit in RemoveEntity cannot make the walk skip or repeat a slot.
// ⚠️ ORDER IS LOAD-BEARING and is the same as leg 4's: volumes off the broad phase and out
// of the id table FIRST, then the graph, then the entity slot. RemoveAllEntityVolumeInstances
// needs the entity slot still allocated (GetFirstEntityVolumeInstance asserts on it).
// ===========================================================================
void SceneManagerModule::RemoveAllOwnerVolumeInstances(
    u8 lu8Owner, OverlapGenerationIO::InputBuffer* lpOverlapGenerationInput)
{
    // FLAG PC (not console): the console loads module+0x280 and calls straight through it.
    // Fetched once and tested per use, exactly like the drain's other partition legs -- an
    // absent entity octree has nothing to do with the volume registry, so it must not
    // suppress the volume half.
    SpatialPartition* lpPartition = mSpatialPartitionManager.GetSpatialPartition();

    for (s32 liEntityIndex = mEntityManager.GetFirstEntityIndex();               // 0x828CDA94
         liEntityIndex != -1;                                                     // 0x828CDA9C
         liEntityIndex = mEntityManager.GetNextEntityIndex(liEntityIndex))        // 0x828CDB64
    {
        const u16 lu16EntityIndex = static_cast<u16>(liEntityIndex);              // 0x828CDAC0

        // 0x828CDAF0: the owner byte is EntityId bits [31..24].
        if (mEntityManager.GetEntityIdByIndex(lu16EntityIndex).GetOwner() != lu8Owner)
        {
            continue;                                                             // 0x828CDB00
        }

        RemoveAllEntityVolumeInstances(lu16EntityIndex, lpOverlapGenerationInput); // 0x828CDB10

        if (lpPartition != NULL)
        {
            lpPartition->RemoveEntityFromGraph(lu16EntityIndex);                  // 0x828CDB28
        }

        CGS_ASSERT(lu16EntityIndex < KI_MAX_NUM_ENTITIES,
                   "lu16Index < KI_MAX_NUM_ENTITIES");                            // 0x828CDB30
        mEntityManager.RemoveEntity(lu16EntityIndex);                             // 0x828CDB58
    }
}

}  // namespace CgsSceneManager

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

// ---------------------------------------------------------------------------
// rw::collision::Volume::InitializeVTable -- the X360 Construct lazily fills the
// shared Volume processing vtable once at scene-manager construction. Its body is
// owned by the rwcollision SDK TU (SDKs/EATech/rwcollision/volume.cpp) and there is
// no shared rw::collision::Volume header, so this is the documented platform/SDK
// forward-declaration exception: a minimal declaration of the one entry point the
// SceneManager calls (the X360 invokes it with no `this`, i.e. as a static).
// ---------------------------------------------------------------------------
namespace rw { namespace collision {
    class Volume
    {
    public:
        static int InitializeVTable();
    };
} }

namespace CgsSceneManager
{

// ---------------------------------------------------------------------------
// File-static perfmon handles. The X360 stores each registered monitor's handle in
// a file-scope `si*PerfMon` int (DWARF CgsSceneManagerModule.cpp:50-86), initialised
// to -1 ("not yet registered"). Construct lazily registers each one once.
// ---------------------------------------------------------------------------
static s32 siContactGen_UpdatePerfMon                = -1;
static s32 siContactGen_GenerateOverlapsPerfMon      = -1;
static s32 siSceneSweeper_SortListsPerfMon           = -1;
static s32 siSceneSweeper_SweepListsPerfMon          = -1;
static s32 siSceneSweeper_BuildCollidingPairsPerfMon = -1;
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
    RegisterPerfMonOnce(siSceneSweeper_SortListsPerfMon, "      SortLists", 16, 10.0f, 0,
                        "SceneSweeper::siUpdate_SortListsPerfMon >= 0");
    RegisterPerfMonOnce(siSceneSweeper_SweepListsPerfMon, "      SweepLists", 16, 10.0f, 0,
                        "SceneSweeper::siUpdate_SweepListsPerfMon >= 0");
    RegisterPerfMonOnce(siSceneSweeper_BuildCollidingPairsPerfMon, "      BuildColPairs", 16, 10.0f, 0,
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
            if (!mOverlapGenerator.Prepare(&mCullingGroupManager, mCullingGroupManager.GetCullingTable()))
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
// found, and that the index is in range. The event payload (a2) is laid out as
// { VolumeInstanceId mInstanceId; <pad>; u8 muCullingGroup; }: a2[0/1] is the 64-bit
// id, a2[2] is the new culling group (stored as a byte per the X360 `stbx`).
// ===========================================================================
void SceneManagerModule::ProcessSetVolumeInstanceCullingGroupEvent(
    const SceneManagerIO::InputBuffer& lrEvent,
    SceneManagerIO::OutputBuffer*      lpInputBuffer)
{
    CGS_ASSERT(lpInputBuffer != NULL, "lpOverlapGenerationInputBuffer != NULL");

    // The event payload is { VolumeInstanceId mVolumeInstanceId (u64 @ +0);
    //   u8 muCullingGroup (@ +8) }. The X360 loads the 64-bit id (`ld r4,0(r28)`) and
    //   later stores the third word's low byte (`lwz r11,8(r28); stbx`).
    const u32* lpEventWords = reinterpret_cast<const u32*>(&lrEvent);
    VolumeInstanceId lInstanceId;
    lInstanceId.muId = *reinterpret_cast<const u64*>(&lrEvent);
    const u8 lu8CullingGroup = static_cast<u8>(lpEventWords[2]);

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
        BridgeOverlapGenerationToOverlapCulling(reinterpret_cast<SceneManagerIO::OutputBuffer*>(lpCullInput),
                                                reinterpret_cast<SceneManagerIO::OutputBuffer*>(lpGenOutput));
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
        BridgeOverlapCullerToOutputBuffer(lpSceneOutputBuffer,
                                          reinterpret_cast<SceneManagerIO::OutputBuffer*>(lpCullOutput));
        BridgeOverlapGenerationToOutputBuffer(lpSceneOutputBuffer,
                                              reinterpret_cast<SceneManagerIO::OutputBuffer*>(lpGenOutput));
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
//
// The scene input's InSceneUpdateInterface is a batch of 25 producer queues; this fans
// them out into the sub-modules. The X360 body walks every queue; the ENTITY legs -- the
// ones the broad-phase runs on -- are reconstructed here:
//
//   mRemoveEntityQueue  -> RemoveEntityFromGraph(index) + EntityManager::RemoveEntity
//   mAddEntityQueue     -> index = EntityManager::AddEntity(id);
//                          SpatialPartition::AllocEntity(index, typeFlags, centre, radius)
//                          then the virtual AddEntityToGraph(index)   [vtable +0x38]
//   mUpdatePositionQueue-> SetEntityPosition(index, position)          [vtable +0x2C]
//   mSetEntityRadiusQueue->SetEntityRadius(index, radius)              [vtable +0x30]
//
// Note the X360 calls the partition DIRECTLY (module+0x280) for the add leg rather than
// routing it through the spatial-partition update queue -- reproduced.
//
// SCOPE NOTE (updated 2026-08-10): the volume / culling-group legs are the VolumeManager's
// territory and are still not reconstructed here; they feed no part of the frustum path, so
// their events stay queued and are dropped with the frame's buffer. The TRIANGLE-CACHE and
// POLY-SOUP legs ARE reconstructed, behind the console's own `lbPrepare` guard -- see the long
// note at the call itself for why that guard is the whole mechanism.
// ===========================================================================
void SceneManagerModule::BridgeInputSceneUpdateInterfaceToSubModules(
    OverlapGenerationIO::InputBuffer* /*lpOverlapGenerationInput*/,
    SpatialPartitionIO::InputBuffer_Update* /*lpSpatialPartitionInput*/,
    SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer,
    bool lbPrepare)
{
    CGS_ASSERT(lpSceneInputBuffer != NULL, "lpSceneInputBuffer != NULL");
    if (lpSceneInputBuffer == NULL)
    {
        return;
    }

    SceneManagerIO::InSceneUpdateInterface* lpScene =
        lpSceneInputBuffer->GetInSceneUpdateInterface();

    // ---- the STATIC-WORLD collision leg ---------------------------------------------------
    // ⭐ ADDED 2026-08-10 (world-collision wave), and it is the console's, not an invention:
    // `xrefs_to` of TriangleCollisionManager::ProcessAddPolySoupListEvents @0x828B3160 lists
    // exactly TWO callers -- SceneManagerModule::StartUpdateTriangleCache @0x828C73D8 (the
    // per-frame one, already committed) and THIS bridge @0x828D1F88.
    //
    // WHY IT MATTERS HERE: WorldModule::PrepareWorldCollision @0x827C9478 stages the world's
    // 396 InEventAddPolySoupList events into a scene input buffer it creates AND DESTROYS
    // inside the same call, with UpdateScene (-> this bridge) as the only consumer in
    // between. Without this leg those events were dropped on the floor every frame of the
    // load, the last one carrying mbRebuildSpacialPartitioning -- so BuildSpacialPartition
    // never ran and the triangle cache stayed empty even with the whole streaming round trip
    // working. RUNTIME-WITNESSED: 396 zones acquired, numLeafNodes still 0.
    //
    // Placed BEFORE the entity legs' partition guard on purpose: the poly-soup partition is
    // the TriangleCollisionManager's own quadtree and has nothing to do with the entity
    // octree, so an absent SpatialPartition must not suppress it.
    //
    // ⭐⭐ CORRECTED + EXTENDED 2026-08-10 (producer wave). The previous wave added the soup leg
    // UNCONDITIONALLY and explicitly declined to add the three TRIANGLE-CACHE legs, reasoning
    // that they "are already driven per frame by StartUpdateTriangleCache". That reasoning is
    // right for the per-frame path and WRONG for the Prepare path, and the console spells the
    // distinction out itself: all four legs sit behind ONE condition, the bridge's own
    // `lbPrepare` argument --
    //     0x828D1FA4  stb r7, arg_37(r1)                      <- lbPrepare, the 5th register arg
    //     0x828D290C  lbz r11, arg_37 ; beq -> skip           -> ProcessRemoveFromCacheEvents
    //     0x828D2D44  lwz r11, var_188 ; beq -> skip          -> ProcessAddToCacheEvents
    //                                                         -> ProcessAddPolySoupListEvents
    //     0x828D388C  lwz r11, var_188 ; beq -> skip          -> ProcessUpdateCachedPositionEvents
    // i.e. the bridge owns the cache queues on the PREPARE passes and StartUpdateTriangleCache
    // owns them per frame. Nothing double-drains.
    //
    // WHY IT MATTERS: VehicleManager::PrepareTriangleCache @0x82615BA0 posts the 8 race-car +
    // 20 traffic InEventAddToCache into a scene input buffer that WorldModule::Prepare's
    // eWorldPreparePhysicsModule stage creates AND destroys inside the same call, with
    // UpdateScene(..., lbPrepare = TRUE) the only consumer in between -- the same shape as the
    // 396 world-collision events. Without these legs the 28 registrations were dropped and
    // TriangleCacheManager::mUsedCacheSlots could never become non-zero.
    //
    // ORDER: the console's relative order is Remove(0x828D292C) ... Add(0x828D2D5C) ...
    // AddPolySoup(0x828D2D74) ... UpdateCachedPosition(0x828D38A8), and that order is preserved
    // below. What is NOT preserved is their absolute position: the console interleaves ~700
    // instructions of volume/entity work between them, none of which touches the cache, so the
    // four are grouped here and kept ahead of the entity legs' partition guard for the same
    // reason the soup leg is (an absent entity octree must not suppress the triangle cache).
    // ⚠️ Still NOT reconstructed from this bridge: ProcessAdd/RemoveForCollisionEvent and
    // UpdateCollisionBody -- volume-collision legs, unrelated to the triangle cache.
    //
    // ⚠️ The soup leg's `lbPrepare` guard is NEW here and is a behaviour change to a working
    // path, so it was proved safe before being added rather than after: its sole producer,
    // WorldEntityModule::AddCollisionZoneToSceneManager, is reached only from
    // WorldModule::PrepareWorldCollision, whose own UpdateScene (BrnWorldModule.cpp:1226) passes
    // TRUE. Every UpdateScene call site in the tree passes TRUE except the per-frame one at
    // :2565, which posts no soup events. Runtime-checked: the 23,645-leaf partition still builds.
    if (lpScene != NULL && lbPrepare)
    {
        // Slot bookkeeping first: free, then claim (Remove's dev cross-checks depend on it).
        mTriangleCacheManager.ProcessRemoveFromCacheEvents(*lpScene);
        mTriangleCacheManager.ProcessAddToCacheEvents(lpScene->mAddToCacheQueue);

        mTriangleCollisionManager.ProcessAddPolySoupListEvents(lpScene->mAddPolySoupListQueue);

        mTriangleCacheManager.ProcessUpdateCachedPositionEvents(lpScene->mUpdateCachedPositionQueue);
    }

    SpatialPartition* lpPartition = mSpatialPartitionManager.GetSpatialPartition();
    if (lpPartition == NULL)
    {
        return;
    }

    // ---- removes first (the X360 order: a slot has to be free before the adds run) ----
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
                continue;   // never registered (or already retired)
            }
            lpPartition->RemoveEntityFromGraph(static_cast<u16>(liIndex));
            mEntityManager.RemoveEntity(static_cast<u16>(liIndex));
        }
    }

    // ---- adds ----
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

    // ---- position updates ----
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

    // ---- radius updates ----
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
// FLAG: steps 3/6 are trimmed here. The bridge above applies the entity legs DIRECTLY to
// the partition (as the X360's own add leg does) rather than restaging them through a
// SpatialPartitionIO::InputBuffer_Update, so that buffer -- a 135 KB VEQ pushed on the
// input stack every frame -- is not created; and the overlap generator is a documented
// inert gate whose input buffer would only be filled by the volume legs the bridge does
// not reconstruct.
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

    // NOTE the WRITE lock: InputBuffer_Update::GetInSceneUpdateInterface @0x825BD8C0
    // guards on the write bit (bit 3, "Not locked for writing\n"), because on the console
    // this buffer reaches UpdateScene still owned by the producer side. Read-locking it
    // here trips that tripwire on the first frame.
    lpSceneInputBuffer->LockForWrite();
    BridgeInputSceneUpdateInterfaceToSubModules(NULL, NULL, lpSceneInputBuffer, lbPrepare);
    lpSceneInputBuffer->UnlockForWrite();

    // The partition's own per-frame pass (re-derive the loose bounds of every branch the
    // adds/removes flagged). The X360 reaches it through SpatialPartitionManager::
    // UpdateScene, which read-locks the (unused here) spatial-partition input first.
    SpatialPartition* lpPartition = mSpatialPartitionManager.GetSpatialPartition();
    if (lpPartition != NULL)
    {
        lpPartition->Update();
    }

    // Step 7 (X360 0x828D4D8C..0x828D4DC4, byte-identical to ProcessSceneQueries' tail):
    // write-lock the scene output and publish &mTriangleCacheManager on it, so every scene
    // output buffer this module fills carries the cache handle -- not just the query one.
    lpSceneOutputBuffer->LockForWrite();
    lpSceneOutputBuffer->GetTriangleCacheInterface()->SetTriangleCacheManager(&mTriangleCacheManager);
    lpSceneOutputBuffer->UnlockForWrite();

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
                                 u32         luObjectIndex,
                                 const void* lpAabb,
                                 u32         luCullingGroup,
                                 u32         luVolumeHandle,
                                 u64         lu64Body)
{
    VolumeInstanceId lVolumeInstanceId;
    lVolumeInstanceId.muId = lu64Body;

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
    // ⚠️ E1/SceneManagerModule OWNER, PLEASE READ: three of this function's parameter
    // TYPES are still the pre-recovery placeholders and are corrected here only by cast.
    // The X360 register usage (0x828BA498: r6 = the box, r8 -> +0x28, r9 -> +0x30) plus
    // DWARF CgsOverlapGenerationModuleIO.h:200 give the real ones --
    //     lpAabb          const void*  ->  const rw::collision::AABBox*
    //     luVolumeHandle  u32          ->  rw::physics::BodyState   (NOT a volume handle:
    //                                      it is the STATIC/FROZEN/ACTIVE tag that
    //                                      SceneSweeper::AddObject three-way dispatches on)
    //     lu64Body        u64          ->  VolumeInstanceId
    // -- and the declaration lives in CgsSceneManagerModule.h, which D1 does not own, so
    // the signature is left untouched. There is no caller yet
    // (ProcessAddForCollisionEvent @0x828CE898 is not reconstructed), so retyping it is a
    // free, isolated change whenever E1 gets here.
    lpOverlapGenerationInputBuffer->AddBody(
        luObjectIndex,
        static_cast<const rw::collision::AABBox*>(lpAabb),
        static_cast<OverlapGenerationIO::InAddBody::CullingGroup>(luCullingGroup),
        static_cast<rw::physics::BodyState>(luVolumeHandle),
        lVolumeInstanceId);

    // Record the object's culling group (non-gating index tripwire, then the store
    // runs unconditionally -- X360 stbx sits past the assert's branch).
    CGS_ASSERT(luObjectIndex < 0x13BBu, "luObjectIndex < (uint32_t)SceneSweeper::KU_MAX_NUM_OBJECTS");
    mCullingGroupManager.GetVolumeInstanceCullingGroup()[luObjectIndex] = static_cast<u8>(luCullingGroup);
}

}  // namespace CgsSceneManager

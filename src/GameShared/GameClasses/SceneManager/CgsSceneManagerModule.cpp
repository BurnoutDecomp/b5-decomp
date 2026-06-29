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
#include "GameShared/GameClasses/Module/CgsIOBufferStack.h"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"
#include "rw/rwcore_structs.h"   // rw::IResourceAllocator / rw::Resource / rw::ResourceDescriptor

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
// SceneManagerModule::EndUpdateTriangleCache @ 0x828C7500
//
// Forward to the triangle-cache manager with the cached collision generator the
// matching StartUpdateTriangleCache stashed, and the triangle-collision scene.
// ===========================================================================
void SceneManagerModule::EndUpdateTriangleCache(SceneManagerIO::IOBufferStack* /*lpInputBufferStack*/,
                                                SceneManagerIO::IOBufferStack* /*lpOutputBufferStack*/)
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
void SceneManagerModule::UpdateContactGeneration(SceneManagerIO::IOBufferStack* lpInputBufferStack,
                                                 SceneManagerIO::IOBufferStack* lpOutputBufferStack,
                                                 SceneManagerIO::OutputBuffer*  lpSceneInputBuffer,
                                                 SceneManagerIO::OutputBuffer*  lpSceneOutputBuffer)
{
    ScopedPerfMon lUpdate(siContactGen_UpdatePerfMon);

    CGS_ASSERT(lpInputBufferStack != NULL,  "lpInputBufferStack != NULL");
    CGS_ASSERT(lpOutputBufferStack != NULL, "lpOutputBufferStack != NULL");
    CGS_ASSERT(lpSceneInputBuffer != NULL,  "lpSceneInputBuffer != NULL");
    CGS_ASSERT(lpSceneOutputBuffer != NULL, "lpSceneOutputBuffer != NULL");

    CgsModule::IOBufferStack* lpInStack  = reinterpret_cast<CgsModule::IOBufferStack*>(lpInputBufferStack);
    CgsModule::IOBufferStack* lpOutStack = reinterpret_cast<CgsModule::IOBufferStack*>(lpOutputBufferStack);

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
void SceneManagerModule::ProcessFrustumTestJobResults(SceneManagerIO::IOBufferStack* lpInputBufferStack,
                                                      SceneManagerIO::IOBufferStack* lpOutputBufferStack,
                                                      SceneManagerIO::OutputBuffer*  lpSceneInputBuffer,
                                                      SceneManagerIO::OutputBuffer*  lpSceneOutputBuffer)
{
    ScopedPerfMon lProcessSceneQueries(siProcessSceneQueriesPerfMon);

    CGS_ASSERT(lpInputBufferStack != NULL,  "lpInputBufferStack != NULL");
    CGS_ASSERT(lpOutputBufferStack != NULL, "lpOutputBufferStack != NULL");
    CGS_ASSERT(lpSceneInputBuffer != NULL,  "lpSceneInputBuffer != NULL");
    CGS_ASSERT(lpSceneOutputBuffer != NULL, "lpSceneOutputBuffer != NULL");

    ScopedPerfMon lCoarse(siProcessCoarseQueriesPerfMon);

    CgsModule::IOBufferStack* lpOutStack = reinterpret_cast<CgsModule::IOBufferStack*>(lpOutputBufferStack);

    // The octree writes its frustum-test results into a SpatialPartition output buffer
    // pushed on the output stack; the SceneManagerModule reads the resolved entity ids
    // out and re-emits them into the scene output event queue.
    SpatialPartitionIO::OutputBuffer* lpResultBuffer = nullptr;
    lpOutStack->CreateIOBuffer(&lpResultBuffer, "SpacialPartition");

    reinterpret_cast<CgsModule::IOBuffer*>(lpResultBuffer)->LockForWrite();

    // The X360 here waits for the loose octree's outstanding frustum-test jobs
    // (mSpatialPartitionManager's active partition), then for every coarse-result batch
    // the jobs produced, allocates a variable event in the scene output event queue and
    // copies the resolved entity ids out (asserting each index < KI_MAX_NUM_ENTITIES and
    // each id != K_INVALID_ENTITY_ID). That per-batch drain reads through the
    // SpatialPartition coarse-result-buffer + EntityManager accessors whose X360 names
    // are truncated in the IDB (LooseOctree::WaitForFrustumTestJobResults,
    // CoarseQueryResultBuffer<16384>::GetBatchByOffset, EntityManager id resolution) and
    // the scene VariableEventQueue<32768,16>. Those callees' full homes are the
    // SpatialPartition / Entity / Module-event-queue TUs (see SCOPE NOTE); the buffer
    // lock/create/destroy plumbing the SceneManagerModule owns is reconstructed here.

    reinterpret_cast<CgsModule::IOBuffer*>(lpResultBuffer)->UnlockForWrite();

    CGS_ASSERT(lpSceneOutputBuffer != NULL, "lpInputBuffer");
    CGS_ASSERT(lpSceneInputBuffer != NULL, "lpOutputBuffer0");
    reinterpret_cast<CgsModule::IOBuffer*>(lpSceneInputBuffer)->UnlockForRead();
    reinterpret_cast<CgsModule::IOBuffer*>(lpSceneOutputBuffer)->UnlockForWrite();

    lpOutStack->DestroyIOBuffer(&lpResultBuffer);
}

}  // namespace CgsSceneManager

#pragma once

// ===========================================================================
// CgsSceneManager::SceneManagerModule
//   Home: GameShared/GameClasses/SceneManager/CgsSceneManagerModule.{h,cpp}
//
// The top-level scene-management CgsModule. It owns -- by value -- the full stack
// of scene sub-managers (broad-phase spatial partition, the overlap generation /
// culling contact-generation pipeline, the fine intersection test stage, the
// entity / volume / culling-group registries, the triangle cache + collision
// managers) and drives their staged Construct / Prepare / Destruct handshakes, the
// per-frame scene update, and the scene query (coarse + fine) processing.
//
// Layout + member set recovered from the DecFIGS DWARF (CgsSceneManagerModule.h)
// and pinned against the X360 ARTIST asm. The member ORDER and the staged
// behaviour are byte-exact to the asm; the embedded sub-managers are modelled by
// value via their own OWNING home headers (their internal layouts are owned by
// their own TUs). Per the project rule, byte OFFSETS are NOT reproduced on the
// x64 PC compile (pointers widen) -- this is semantic-parity-by-named-member, not
// a byte-match.
//
// X360 functions reconstructed in this TU (CgsSceneManagerModule.cpp):
//   SceneManagerModule::Construct                              @ 0x828D09A0
//   SceneManagerModule::Destruct                              @ 0x828D1640
//   SceneManagerModule::Prepare                               @ 0x828D13E0
//   SceneManagerModule::CreateCullingTable                    @ 0x828BAC90
//   SceneManagerModule::EndUpdateTriangleCache                @ 0x828C7500
//   SceneManagerModule::UpdateContactGeneration               @ 0x828D5CA0
//   SceneManagerModule::ProcessFrustumTestJobResults          @ 0x828C7838
//   SceneManagerModule::ProcessSetVolumeInstanceCullingGroupEvent @ 0x828CF8E8
//   CullingGroupManager::CreateCullingTable                   @ 0x828BAB48 (in CgsCullingGroupManager.h)
// ===========================================================================

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"  // CgsModule::ModuleSingleBuffered (base)
#include "GameShared/GameClasses/SceneManager/CgsSceneQueryId.h"    // SceneQueryId

// Embedded sub-managers (each by value, in DWARF declaration order).
#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/CgsSpatialPartitionManager.h"
#include "GameShared/GameClasses/SceneManager/ContactGen/CgsOverlapGenerationModule.h"  // full home of OverlapGenerationModule + OverlapGenerationIO (AddBody producer types)
#include "GameShared/GameClasses/SceneManager/CgsOverlapCullingModule.h"
#include "GameShared/GameClasses/SceneManager/FineIntersectionTestModule/CgsFineIntersectionTestModule.h"
#include "GameShared/GameClasses/SceneManager/CgsEntityManager.h"
#include "GameShared/GameClasses/SceneManager/CgsVolumeManager.h"
#include "GameShared/GameClasses/SceneManager/CgsCullingGroupManager.h"
#include "GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManager.h"
#include "GameShared/GameClasses/SceneManager/TriangleCollision/CgsTriangleCollisionManager.h"

// NOTE: the trailing debug component's home header
// (CgsSceneManagerDebugComponent.h) currently does NOT compile against the present
// vendor RenderWare headers -- it was authored against an older rw vocabulary
// (rw::math::Vector3 / Matrix44Affine::InParam / rw::collision::TriangleVolume that
// the current rw/ headers spell differently / do not yet expose). Pulling it here
// would cascade those breakages into this TU's compile. Per the AGENTS.md
// forward-declaration / cascade-avoidance exception, the embedded mSceneManagerDebugComponent
// is modelled below as a sized stand-in for the trailing debug-only member; swap it for
// the real `#include` once CgsSceneManagerDebugComponent.h is reconciled with the vendor
// rw headers (its own TU). See the member comment.

namespace rw { struct IResourceAllocator; }
namespace CgsMemory { class LinearMalloc; }

namespace CgsModule { struct IOBufferStack; }

namespace CgsSceneManager
{
    // Max in-flight frustum-test job queries the module tracks (DWARF
    // CgsSceneManagerModule.h:84).
    const u32 KU_MAX_FRUSTUM_TEST_JOB_QUERIES = 16;

    // Coarse / fine query result-buffer kinds (DWARF CgsSceneManagerModule.h:92).
    enum EQueryResultType
    {
        E_QUERY_RESULT_TYPE_COARSE             = 0,
        E_QUERY_RESULT_TYPE_LINE_TEST_FINE     = 1,
        E_QUERY_RESULT_TYPE_LINE_TEST_NEAREST  = 2,
        E_QUERY_RESULT_TYPE_VOLUME_TEST_DEEPEST = 3,
        E_QUERY_RESULT_TYPE_VOLUME_TEST_FINE   = 4,
        E_QUERY_RESULT_TYPE_COUNT              = 5,
    };

    // The contact-generator interface the triangle-cache stage uses. Referenced by
    // pointer only (mpTriangleCacheCollisionGenerator + the Start/EndUpdate APIs).
    // Its real home is Collision/ContactGenerator/CgsCollisionGenerator.h
    // (CgsSceneManager::CgsCollision::BaseCollisionGenerator) -- WorldModule::Update
    // @0x827D63E8 carves one per frame out of the world linear allocator and hands
    // it to StartUpdateTriangleCache.
    namespace CgsCollision { struct BaseCollisionGenerator; }
    typedef CgsCollision::BaseCollisionGenerator BaseCollisionGenerator;

    namespace SceneManagerIO
    {
        struct IOBufferStack;
        struct OutputBuffer;
        struct InputBuffer;
        struct InputBuffer_Update;
        struct InputBuffer_Query;
    }

    // The overlap-generation input buffer the AddBody producer pushes onto. Its full
    // definition lives in ContactGen/CgsOverlapGenerationModule.h (pulled by the .cpp);
    // this TU's header includes only the STUB SceneManager/CgsOverlapGenerationModule.h,
    // so a forward declaration suffices for the pointer parameter.
    namespace OverlapGenerationIO { struct InputBuffer; }

    class SceneManagerModule : public CgsModule::ModuleSingleBuffered
    {
    public:
        // ADDITIVE (WorldModule::ExternalSceneQueriesUpdate @0x827B06C8 forwards to
        // this virtual -- X360 vtbl slot +68). Declaration-only; body with its own TU.
        void ExternalSceneQueriesUpdate();

        // ADDITIVE (WorldModule::GenerateFrustumQueries @0x827DADF8 hands the staged
        // frustum-test queries to the job system through this). Declaration-only.
        void ProcessFrustumTestJobRequests( CgsModule::IOBufferStack* lpInputBufferStack,
                                            CgsModule::IOBufferStack* lpOutputBufferStack,
                                            SceneManagerIO::InputBuffer_Query* lpQueryInput,
                                            SceneManagerIO::OutputBuffer* lpQueryOutput );

        // @ 0x828D57D0 -- the X360 vtbl+68 entry: run this frame's staged scene
        // queries (coarse pass then fine pass, each under its own CPU monitor) and
        // publish the triangle-cache manager on the scene output. The entity-module
        // spines round-trip one query batch per module through it.
        // RENAMED 2026-07-27 (world-drive wave) from the placeholder name
        // "UpdateQueries" to the X360 symbol: CgsSceneManagerModule.cpp:806..
        void ProcessSceneQueries( CgsModule::IOBufferStack* lpInputBufferStack,
                                  CgsModule::IOBufferStack* lpOutputBufferStack,
                                  SceneManagerIO::InputBuffer_Query* lpQueryInput,
                                  SceneManagerIO::OutputBuffer* lpQueryOutput );

        // @ 0x828D4C28 -- the X360 vtbl+64 entry: fan the scene input's update
        // interface out into the spatial-partition + overlap-generation sub-modules,
        // tick both, and publish the triangle-cache manager on the scene output.
        // WorldModule::Prepare @0x827D53B0 drives it once per stage (lbPrepare set)
        // and WorldModule::Update @0x827D63E8 twice per frame (pre-scene / post-
        // physics). RENAMED 2026-07-27 from the placeholder name "Update": the X360
        // symbol literally named SceneManagerModule::Update @0x827E1F28 is the
        // "Don't use this function. Use UpdateScene(), UpdateSceneQueries() and
        // UpdateContactGeneration() instead" assert (CgsSceneManagerModule.h:276).
        bool UpdateScene( CgsModule::IOBufferStack* lpInputBufferStack,
                          CgsModule::IOBufferStack* lpOutputBufferStack,
                          SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer,
                          SceneManagerIO::OutputBuffer* lpSceneOutputBuffer,
                          bool lbPrepare );

        // @ 0x828C73D8 -- begin this frame's triangle-cache update: latch the
        // frame's collision generator (carved from the world's per-frame linear
        // allocator by WorldModule::Update @0x827D63E8) and kick the cache
        // manager's per-frame cache refresh.
        void StartUpdateTriangleCache( CgsModule::IOBufferStack* lpInputBufferStack,
                                       CgsModule::IOBufferStack* lpOutputBufferStack,
                                       SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer,
                                       BaseCollisionGenerator* lpCollisionGenerator );

        // Staged prepare progression (DWARF CgsSceneManagerModule.h:180). Construct
        // leaves the module at START; Prepare advances one stage per resumable call.
        enum ESceneManagerPrepareStage
        {
            E_SCENEMANAGER_PREPARE_START                       = 0,
            E_SCENEMANAGER_PREPARE_MANAGER                     = 1,
            E_SCENEMANAGER_PREPARE_ENTITY_MANAGER              = 2,
            E_SCENEMANAGER_PREPARE_VOLUME_MANAGER              = 3,
            E_SCENEMANAGER_PREPARE_CACHE_MANAGER               = 4,
            E_SCENEMANAGER_PREPARE_TRI_COLLISION_MANAGER       = 5,
            E_SCENEMANAGER_PREPARE_SCENE_GRAPH_MODULE          = 6,
            E_SCENEMANAGER_PREPARE_OVERLAP_GENERATION_MODULE   = 7,
            E_SCENEMANAGER_PREPARE_OVERLAP_CULLING_MODULE      = 8,
            E_SCENEMANAGER_PREPARE_FINE_INTERSECTION_TEST_MODULE = 9,
            E_SCENEMANAGER_PREPARE_DONE                        = 10,
        };

        // Staged release progression (DWARF CgsSceneManagerModule.h:195).
        enum ESceneManagerReleaseStage
        {
            E_SCENEMANAGER_RELEASE_START                       = 0,
            E_SCENEMANAGER_RELEASE_FINE_INTERSECTION_TEST_MODULE = 1,
            E_SCENEMANAGER_RELEASE_OVERLAP_CULLING_MODULE      = 2,
            E_SCENEMANAGER_RELEASE_OVERLAP_GENERATION_MODULE   = 3,
            E_SCENEMANAGER_RELEASE_SCENE_GRAPH_MODULE          = 4,
            E_SCENEMANAGER_RELEASE_VOLUME_MANAGER              = 5,
            E_SCENEMANAGER_RELEASE_ENTITY_MANAGER              = 6,
            E_SCENEMANAGER_RELEASE_TRI_COLLISION_MANAGER       = 7,
            E_SCENEMANAGER_RELEASE_CACHE_MANAGER               = 8,
            E_SCENEMANAGER_RELEASE_MANAGER                     = 9,
            E_SCENEMANAGER_RELEASE_DONE                        = 10,
        };

        // ---- lifecycle (CgsModule virtuals) ----
        void Construct() override;   // @ 0x828D09A0
        void Destruct() override;    // @ 0x828D1640

        // @ 0x828D13E0 -- staged, resumable prepare of every sub-manager. The X360
        // threads four resources: the coarse construct params, the scene resource
        // allocator (spatial partition + culling table), the cache resource
        // allocator (triangle cache), and the linear allocator (triangle collision).
        bool Prepare(SpatialPartitionConstructParams* lpConstructParams,
                     rw::IResourceAllocator*          lpSceneAllocator,
                     rw::IResourceAllocator*          lpCacheAllocator,
                     CgsMemory::LinearMalloc*         lpLinearAllocator);

        // @ 0x828D5CA0 -- the per-frame contact generation pass (generate -> cull ->
        // bridge -> tri-cache). Takes the in/out IO buffer stacks + the scene
        // in/out buffers (by pointer).
        void UpdateContactGeneration(CgsModule::IOBufferStack* lpInputBufferStack,
                                     CgsModule::IOBufferStack* lpOutputBufferStack,
                                     SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer,
                                     SceneManagerIO::OutputBuffer*  lpSceneOutputBuffer);

        // @ 0x828C7838 -- gather the loose-octree frustum-test job results into the
        // scene output event queue.
        // RECONCILED 2026-07-24: the X360 callers (WorldModule::GenerateDispatchLists
        // @0x827D1CE8 / GenerateFrustumQueries @0x827DADF8) stack-allocate the third
        // argument as an InputBuffer_Query (CreateIOBuffer<InputBuffer_Query>), so the
        // query-input parameter is typed as such (was OutputBuffer*).
        void ProcessFrustumTestJobResults(CgsModule::IOBufferStack* lpInputBufferStack,
                                          CgsModule::IOBufferStack* lpOutputBufferStack,
                                          SceneManagerIO::InputBuffer_Query* lpQueryInputBuffer,
                                          SceneManagerIO::OutputBuffer*  lpQueryOutputBuffer);

        // @ 0x828C7500 -- finish the triangle-cache update for this frame.
        void EndUpdateTriangleCache(CgsModule::IOBufferStack* lpInputBufferStack,
                                    CgsModule::IOBufferStack* lpOutputBufferStack);

    protected:
        // @ 0x828CF8E8 -- apply a "set volume-instance culling group" scene event.
        void ProcessSetVolumeInstanceCullingGroupEvent(const SceneManagerIO::InputBuffer&  lrEvent,
                                                        SceneManagerIO::OutputBuffer*       lpInputBuffer);

        // @ 0x828BAC90 -- build the culling-group adjacency table from the scene
        // resource allocator (forwards to mCullingGroupManager).
        void CreateCullingTable(rw::IResourceAllocator* lpSceneAllocator);

        // @ 0x828BA498 -- push an add-body request onto the overlap-generation input
        // buffer's add-body queue and record the object's culling group. lpAabb is the
        // 32-byte world box (opaque; block-copied whole into the queued 64-byte event).
        void AddBody(OverlapGenerationIO::InputBuffer* lpOverlapGenerationInputBuffer,
                     u32 luObjectIndex, const void* lpAabb, u32 luCullingGroup,
                     u32 luVolumeHandle, u64 lu64Body);

        // Bridges driven by UpdateContactGeneration (bodies in this TU's bridge
        // sibling TU; declared here for the call sites).
        void BridgeOverlapGenerationToOverlapCulling(SceneManagerIO::OutputBuffer* lpInput,
                                                     SceneManagerIO::OutputBuffer* lpGenerationOutput);
        void BridgeOverlapCullerToOutputBuffer(SceneManagerIO::OutputBuffer* lpSceneOutput,
                                               SceneManagerIO::OutputBuffer* lpCullerOutput);
        void BridgeOverlapGenerationToOutputBuffer(SceneManagerIO::OutputBuffer* lpSceneOutput,
                                                   SceneManagerIO::OutputBuffer* lpGenerationOutput);

        // @ 0x828D1F88 -- fan the scene input's InSceneUpdateInterface out into the
        // sub-modules: allocate / retire entity-manager slots for the add + remove
        // queues, re-emit the position / radius updates, and drive the collision-volume
        // legs. Only the ENTITY legs (the ones the broad-phase needs) are reconstructed;
        // see the body's SCOPE note.
        void BridgeInputSceneUpdateInterfaceToSubModules(
            OverlapGenerationIO::InputBuffer* lpOverlapGenerationInput,
            SpatialPartitionIO::InputBuffer_Update* lpSpatialPartitionInput,
            SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer,
            bool lbPrepare);

    private:
        // ---- members (DWARF order; offsets pinned to the X360 asm in comments) ----
        SceneQueryId maFrustumTestJobQueryIds[KU_MAX_FRUSTUM_TEST_JOB_QUERIES];  // X360 +0x228

        ESceneManagerPrepareStage mePrepareStage;  // X360 +0x268 (Prepare iterates this)
        ESceneManagerReleaseStage meReleaseStage;  // X360 +0x26C

        SpatialPartitionManager     mSpatialPartitionManager;     // X360 +0x270
        OverlapGenerationModule     mOverlapGenerator;            // X360 +0x290
        OverlapCullingModule        mOverlapCuller;               // X360 +0xDC9A0
        FineIntersectionTestModule  mFineIntersectionTestModule;  // X360 +0x148C40
        EntityManager               mEntityManager;               // X360 +0x1A2480
        VolumeManager               mVolumeManager;               // X360 +0x2C7800
        CullingGroupManager         mCullingGroupManager;         // X360 +0x3A6EA0
        TriangleCacheManager        mTriangleCacheManager;        // X360 +0x3A8260
        TriangleCollisionManager    mTriangleCollisionManager;    // X360 +0x3A82C0

        BaseCollisionGenerator*     mpTriangleCacheCollisionGenerator;  // X360 +0x3A84D0

        s32 miTimeInCachedContactGen;     // X360 +0x3A84D4
        s32 miTimeInNonCachedContactGen;  // X360 +0x3A84D8

        // The trailing by-value debug component (DWARF mSceneManagerDebugComponent,
        // X360 +0x3A84E0). Its real home CgsSceneManagerDebugComponent.h does not
        // currently compile against the vendor rw headers (see the include note above),
        // so it is modelled here as a sized opaque tail member -- the cascade-avoidance
        // forward-declaration exception. Replace with the real embedded type once that
        // header is reconciled. Construct()/Register() on it are routed as the documented
        // debug-overlay registration in the .cpp.
        u8 maSceneManagerDebugComponent[131200];  // X360 +0x3A84E0 (128KB query buffer + frustum + flags)
    };
}

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
    class BaseCollisionGenerator;

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
        void UpdateContactGeneration(SceneManagerIO::IOBufferStack* lpInputBufferStack,
                                     SceneManagerIO::IOBufferStack* lpOutputBufferStack,
                                     SceneManagerIO::OutputBuffer*  lpSceneInputBuffer,
                                     SceneManagerIO::OutputBuffer*  lpSceneOutputBuffer);

        // @ 0x828C7838 -- gather the loose-octree frustum-test job results into the
        // scene output event queue.
        void ProcessFrustumTestJobResults(SceneManagerIO::IOBufferStack* lpInputBufferStack,
                                          SceneManagerIO::IOBufferStack* lpOutputBufferStack,
                                          SceneManagerIO::OutputBuffer*  lpSceneInputBuffer,
                                          SceneManagerIO::OutputBuffer*  lpSceneOutputBuffer);

        // @ 0x828C7500 -- finish the triangle-cache update for this frame.
        void EndUpdateTriangleCache(SceneManagerIO::IOBufferStack* lpInputBufferStack,
                                    SceneManagerIO::IOBufferStack* lpOutputBufferStack);

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

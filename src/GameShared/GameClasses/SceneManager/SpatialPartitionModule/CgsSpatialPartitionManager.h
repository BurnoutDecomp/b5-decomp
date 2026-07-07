#pragma once

// ===========================================================================
// CgsSceneManager::SpatialPartitionManager
//   Home: GameShared/GameClasses/SceneManager/SpatialPartitionModule/
//         CgsSpatialPartitionManager.{h,cpp}
//
// The SceneManager's broad-phase (coarse) spatial partition -- a LooseOctree of
// the scene's collidable volumes, used for the frustum/sphere/line coarse
// queries. Embedded BY VALUE in CgsSceneManager::SceneManagerModule
// (mSpatialPartitionManager @ X360 +0x270); the embedded part is small (a handful
// of words -- prepare/release stage, two debug-text coords, the active octree
// pointer and its type), the octree storage itself is allocated through the rw
// resource allocator in Prepare.
//
// Member layout + method shapes are DWARF-attested (CgsSpatialPartitionManager.h),
// gated on the X360 ARTIST ledger:
//   Prepare            @ 0x828CFFA8
//   Release            @ 0x828AA860
//   ProcessUpdateQueue @ 0x828BAE50
// ===========================================================================

#include "types.hpp"

namespace rw { struct IResourceAllocator; }

namespace CgsSceneManager
{
    // Forward decls -- the manager only holds a SpatialPartition* and consumes the
    // update-input buffer by pointer; their real homes are the sibling headers, pulled
    // into the .cpp only (keeps this header -- and everything that embeds the manager --
    // light).
    struct SpatialPartition;
    namespace SpatialPartitionIO { struct InputBuffer_Update; }

    // DWARF canonical home is CgsSpatialPartition.h:61 (namespace CgsSceneManager). It is
    // not yet defined by that (shared) header, so it is modelled here -- the manager is the
    // TU that actually stores/branches on it (Prepare selects the partition kind from the
    // construct params). Values are DWARF-attested.
    enum ESpatialPartitionType
    {
        E_SPATIAL_PARTITION_TYPE_SPHERE_TREE  = 0,
        E_SPATIAL_PARTITION_TYPE_LOOSE_OCTREE = 1,
        E_SPATIAL_PARTITION_TYPE_COUNT        = 2,
    };

    // Coarse-query construct parameters. The SceneManagerModule receives one by pointer
    // from its caller and threads it straight into Prepare; only the leading partition-type
    // selector is read here (DWARF CgsSpatialPartition.h:84 -> meType @ +0x00). The rest of
    // its layout (volume budgets / world bounds) is owned by the SpatialPartition TUs.
    struct SpatialPartitionConstructParams
    {
        ESpatialPartitionType meType;  // +0x00
    };

    class SpatialPartitionManager
    {
    public:
        // Multi-step prepare handshake over the scene graph (DWARF
        // CgsSpatialPartitionManager.h). Prepare advances through these each call.
        enum ESpatialPartitionPrepareStage
        {
            E_SCENE_GRAPH_PREPARE_START                 = 0,
            E_SCENE_GRAPH_PREPARE_MANAGER               = 1,
            E_SCENE_GRAPH_PREPARE_CONSTRUCT_SCENE_GRAPH = 2,
            E_SCENE_GRAPH_PREPARE_SCENE_GRAPH           = 3,
            E_SCENE_GRAPH_PREPARE_DONE                  = 4,
        };

        // Multi-step release handshake (DWARF CgsSpatialPartitionManager.h). Release
        // advances through these each call.
        enum ESpatialPartitionReleaseStage
        {
            E_SCENE_GRAPH_RELEASE_START       = 0,
            E_SCENE_GRAPH_RELEASE_SCENE_GRAPH = 1,
            E_SCENE_GRAPH_RELEASE_MANAGER     = 2,
            E_SCENE_GRAPH_RELEASE_DONE        = 3,
        };

        void Construct();

        // @ 0x828CFFA8 -- prepare the octree. The X360 build passes the construct params and
        // the scene resource allocator; the staged handshake carves + Constructs the octree
        // then drives its own Prepare. Returns success.
        bool Prepare(SpatialPartitionConstructParams* lpConstructParams,
                     rw::IResourceAllocator*          lpSceneAllocator);

        // @ 0x828AA860 -- staged release of the active partition (Release then Destruct on
        // failure), resetting the prepare stage when done. Returns success.
        bool Release();

    private:
        // @ 0x828BAE50 -- drain the inbound update queue (Add/Remove/SetPosition/SetRadius
        // events) into the active partition. Called by UpdateScene.
        void ProcessUpdateQueue(const SpatialPartitionIO::InputBuffer_Update* lpInputBuffer);

        ESpatialPartitionPrepareStage mePrepareStage;         // +0x00 (SMM byte +0x270)
        ESpatialPartitionReleaseStage meReleaseStage;         // +0x04
        s32                           miTextX;                // +0x08 (debug-render text pos)
        s32                           miTextY;                // +0x0C
        SpatialPartition*             mpSpatialPartition;     // +0x10 (allocator-backed octree)
        ESpatialPartitionType         meSpatialPartitionType; // +0x14
    };

    // Post-increment stage advance -- the X360 emits these as distinct namespace-scope
    // operators (DWARF CgsSpatialPartitionManager.h:228/229); Prepare/Release call them to
    // step their handshake stages.
    inline SpatialPartitionManager::ESpatialPartitionPrepareStage
    operator++(SpatialPartitionManager::ESpatialPartitionPrepareStage& lreStage, int)
    {
        SpatialPartitionManager::ESpatialPartitionPrepareStage lePrev = lreStage;
        lreStage = static_cast<SpatialPartitionManager::ESpatialPartitionPrepareStage>(
            static_cast<int>(lreStage) + 1);
        return lePrev;
    }

    inline SpatialPartitionManager::ESpatialPartitionReleaseStage
    operator++(SpatialPartitionManager::ESpatialPartitionReleaseStage& lreStage, int)
    {
        SpatialPartitionManager::ESpatialPartitionReleaseStage lePrev = lreStage;
        lreStage = static_cast<SpatialPartitionManager::ESpatialPartitionReleaseStage>(
            static_cast<int>(lreStage) + 1);
        return lePrev;
    }
}

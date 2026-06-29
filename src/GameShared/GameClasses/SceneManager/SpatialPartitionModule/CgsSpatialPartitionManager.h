#pragma once

// ===========================================================================
// CgsSceneManager::SpatialPartitionManager
//   Home: GameShared/GameClasses/SceneManager/SpatialPartitionModule/
//         CgsSpatialPartitionManager.{h,cpp}
//
// The SceneManager's broad-phase (coarse) spatial partition -- a LooseOctree of
// the scene's collidable volumes, used for the frustum/sphere/line coarse
// queries. Embedded BY VALUE in CgsSceneManager::SceneManagerModule
// (mSpatialPartitionManager); the embedded part is small (a handful of words --
// stage + the active octree pointer), the octree storage itself is allocated
// through the rw resource allocator in Prepare.
//
// Enum + Prepare() signature from the DecFIGS DWARF
// (CgsSpatialPartitionManager.h), gated on the X360 ARTIST ledger.
//
// X360 function this TU (CgsSceneManagerModule.cpp) calls:
//   SpatialPartitionManager::Prepare  @ 0x828CFFA8 (3-arg X360 form)
// ===========================================================================

#include "types.hpp"

namespace rw { struct IResourceAllocator; }

namespace CgsSceneManager
{
    // Coarse-query construct parameters (volume budgets / world bounds). The
    // SceneManagerModule receives one by pointer from its caller and threads it
    // straight into Prepare; its full layout is owned by the SpatialPartition TUs.
    struct SpatialPartitionConstructParams;

    class SpatialPartitionManager
    {
    public:
        // Multi-step prepare handshake over the scene graph (DWARF
        // CgsSpatialPartitionManager.h).
        enum ESpatialPartitionPrepareStage
        {
            E_SCENE_GRAPH_PREPARE_START                = 0,
            E_SCENE_GRAPH_PREPARE_MANAGER              = 1,
            E_SCENE_GRAPH_PREPARE_CONSTRUCT_SCENE_GRAPH = 2,
            E_SCENE_GRAPH_PREPARE_SCENE_GRAPH          = 3,
            E_SCENE_GRAPH_PREPARE_DONE                 = 4,
        };

        void Construct();

        // @ 0x828CFFA8 -- prepare the octree. The X360 build passes the construct
        // params and the scene resource allocator. Returns success.
        bool Prepare(SpatialPartitionConstructParams* lpConstructParams,
                     rw::IResourceAllocator*          lpSceneAllocator);

    private:
        ESpatialPartitionPrepareStage mePrepareStage;  // +0x00 (SMM byte +0x270)

        // The remaining embedded state (active partition pointer + small scratch).
        // The octree itself is allocator-backed; modelled opaque here.
        u8 maEmbeddedState[28];
    };
}

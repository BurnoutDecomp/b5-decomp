#pragma once

#include "types.hpp"

#include <cstdint>   // uintptr_t

// ===========================================================================
// rw::collision::ClusteredMesh -- the clustered collision mesh procedural
// (clusteredmeshbase.h), viewed up to its cluster parameters.
//
// Its own header (rather than ClusteredMeshQuery.hpp) so a translation unit that
// already carries the game's 4-lane Vector3 can name the mesh without the AABBox
// header that query file pulls in. Everything before +0x30 is the Procedural /
// Aggregate base, which neither rw::collision::AA nor the scene manager's
// clustered-mesh line walk touches. The mesh is a serialised resource image, so its
// two pointer members are console-width words that resolve through the project's
// low-4 GB convention (GetKDTree / GetClusterOffsets).
// ===========================================================================

namespace rw
{
namespace collision
{

struct KdTree;

struct ClusteredMesh
{
    u32 mauReserved0[12];               // +0x00..+0x2F  the Procedural / Aggregate base
    u32 muKDTree;                       // +0x30  mKDTree (KDTree*)
    u32 muCluster;                      // +0x34  mCluster (uint32_t*): per-cluster byte offset from `this`
    // mClusterParams (ClusterParams, clusteredmeshcluster.h) @ +0x38
    f32 mfVertexCompressionGranularity; // +0x38  metres per compressed integer step
    u16 muClusterFlags;                 // +0x3C  mFlags
    u8  muGroupIdSize;                  // +0x3E  mGroupIdSize (bytes of a unit's group id)
    u8  muSurfaceIdSize;                // +0x3F  mSurfaceIdSize (bytes of a unit's surface id)

    KdTree*    GetKDTree() const         { return reinterpret_cast<KdTree*>(static_cast<uintptr_t>(muKDTree)); }
    const u32* GetClusterOffsets() const { return reinterpret_cast<const u32*>(static_cast<uintptr_t>(muCluster)); }
};

static_assert(sizeof(ClusteredMesh) == 0x40, "the view ends after the cluster parameters (+0x3F)");

} // namespace collision
} // namespace rw

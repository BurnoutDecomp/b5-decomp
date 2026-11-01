#pragma once

#include "types.hpp"

#include "vendor/renderware/collision/Aggregate.hpp"
#include "vendor/renderware/collision/KdTreeBBoxQuery.hpp"

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

// DecFIGS procedural.h:49 / clusteredmeshbase.h:49; ARTIST Fixup
// 82BB31E8..3200 reads and writes the two genuine pointers at +30/+34.
// Aggregate +28 and the mesh tail are still unrecovered.
struct Procedural : public Aggregate {};
struct ClusteredMesh : public Procedural
{
    KdTree* mpKDTree;
    u32* mpCluster;
    f32 mfVertexCompressionGranularity;
    u16 muClusterFlags;
    u8 muGroupIdSize;
    u8 muSurfaceIdSize;

    KdTree* GetKDTree() const { return mpKDTree; }
    const u32* GetClusterOffsets() const { return mpCluster; }
};
CGS_STATIC_ASSERT((sizeof(Aggregate)==0x30), "Aggregate native base extent");
CGS_STATIC_ASSERT((offsetof(ClusteredMesh,mpKDTree)==0x30), "ClusteredMesh KDTree");
CGS_STATIC_ASSERT((offsetof(ClusteredMesh,mpCluster)==0x34), "ClusteredMesh cluster offsets");

} // namespace collision
} // namespace rw

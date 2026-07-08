#pragma once

#include "types.hpp"
#include "vendor/renderware/collision/AABBox.hpp"
#include "vendor/renderware/collision/FeatureEdge.hpp"        // Vec4
#include "vendor/renderware/collision/LineSegIntersect.hpp"   // VolumeLineSegIntersectResult
#include "vendor/renderware/collision/TriangleVolume.hpp"     // rw::collision::TriangleVolume (real home)

// ===========================================================================
// rw::collision clustered-mesh query helpers -- the free functions of the
// clustered-mesh line/bbox query pass the X360 binary defines in the
// rw::collision TU:
//
//     rw::collision::AA              @ 0x82BB17E0  (IDA-truncated demangle;
//         the clustered-mesh unit bounding-box builder -- sole caller
//         rw::collision::ClusteredMesh::BBoxOverlapQueryThis)
//     rw::collision::ComputeEdgeCos  @ 0x82BB13A0  (sole caller
//         rw::collision::ClusteredMesh::GetUnitVolumes)
//     rw::collision::AddQueryResult  @ 0x82BB1588  (sole caller
//         rw::collision::ClusteredMesh::LineIntersectionQueryThis)
// ===========================================================================

namespace rw
{
namespace collision
{

// ---------------------------------------------------------------------------
// Minimal view of rw::collision::ClusteredMesh -- ONLY the two members
// AA @ 0x82BB17E0 reads are named (everything before +0x34 is the vtable +
// Aggregate/Procedural base this function never touches). Console offsets in
// the comments are the X360 ones the asm attests; the pointer member widens
// on x64 per the usual convention.
// ---------------------------------------------------------------------------
struct ClusteredMesh
{
    u32        mauReserved0[13];               // +0x00..+0x33  not read here
    const u32* mpuClusterOffsets;              // +0x34  per-cluster byte offset from `this`
    f32        mfVertexCompressionGranularity; // +0x38  metres per compressed integer step
};

// @ 0x82BB17E0 -- decompress the 3 (triangle) or 4 (quad, unit-flags nibble
// == 2) vertices of the unit at auUnitOffset inside cluster auClusterIndex
// and return their per-lane min/max box (returned via the hidden r3 slot on
// X360; lane 3 carries the decode artifact the asm min/maxes alongside xyz).
AABBox AA(const ClusteredMesh* lpMesh, u32 auClusterIndex, u32 auUnitOffset);

// The convex-edge bit ComputeEdgeCos distils from the vcmpgtfp. CR6 "all
// lanes true" flag (mfocrf r10,2 ; rlwinm -> 0x20 or 0).
const u8 KU_EDGE_CONVEX_FLAG = 0x20;

// @ 0x82BB13A0 -- edge cosine of the shared edge (arEdgeStart, arEdgeEnd)
// between triangles (apexA, start, end) and (apexB, end, start); stores the
// one-byte convexity flag through lpConvexFlag (unconditionally, BEFORE the
// degeneracy early-outs) and returns cos(angle between the triangle normals)
// -- 1.0f when either normal is degenerate.
// (X360 __fastcall: r3=lpConvexFlag, r4..r7 the four vertex-row pointers;
// returns f1.)
f32 ComputeEdgeCos(u8* lpConvexFlag,
                   const Vec4* lpApexA,
                   const Vec4* lpEdgeStart,
                   const Vec4* lpEdgeEnd,
                   const Vec4* lpApexB);

// rw::collision::TriangleVolume is homed by
// vendor/renderware/collision/TriangleVolume.hpp (included above); AddQueryResult
// calls its GetNormal @ 0x82BB0580 (bl rw__collision__TriangleVolume__GetNormal
// with r3 = the volume, r4 = the normal out-slot, r5 = the transform rows).

// The query object; defined (TU-locally) by the committed
// src/SDKs/EATech/rwcollision/volumelinequery.cpp. Opaque here -- the fields
// AddQueryResult touches are reached through a TU-local layout view (see the
// .cpp), so no second class definition is introduced.
class VolumeLineQuery;

// @ 0x82BB1588 -- append one triangle hit to lpQuery's result list. X360
// __fastcall: r3=lpQuery, r4=lpResult, r5=lpVolume, r6=lpTransform (4 rows),
// then the vector arguments in VMX registers (v1=rLocalPosition,
// v2=rLineParam, v3=rVolParam, v4=rNormal), r7..r10 the four tag-composition
// pointers and the ninth integer argument (lpNumClusterTagBits) on the stack.
// Returns 1 while the query can accept more results, else 0 (the caller's
// keep-going condition).
int AddQueryResult(VolumeLineQuery*              lpQuery,             // r3
                   VolumeLineSegIntersectResult* lpResult,            // r4
                   const TriangleVolume*         lpVolume,            // r5
                   const Vec4*                   lpTransform,         // r6 (4 rows)
                   const Vec4&                   rLocalPosition,      // v1
                   const Vec4&                   rLineParam,          // v2 (lane X)
                   const Vec4&                   rVolParam,           // v3
                   const Vec4&                   rNormal,             // v4
                   const u32*                    lpClusterId,         // r7
                   const u32*                    lpUnitOffset,        // r8
                   const u8*                     lpNumTagBits,        // r9
                   const u32*                    lpNumMeshTagBits,    // r10
                   const u32*                    lpNumClusterTagBits);// stack

} // namespace collision
} // namespace rw

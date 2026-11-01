#pragma once

#include "types.hpp"
#include "vendor/renderware/collision/AABBox.hpp"
#include "vendor/renderware/collision/FeatureEdge.hpp"        // Vec4
#include "vendor/renderware/collision/LineSegIntersect.hpp"   // VolumeLineSegIntersectResult
#include "vendor/renderware/collision/TriangleVolume.hpp"     // rw::collision::TriangleVolume (real home)
#include "vendor/renderware/collision/ClusteredMesh.hpp"      // rw::collision::ClusteredMesh

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
// ARTIST passes v1–v4 by value. Vector3/VecFloat spellings follow the
// corresponding triangle-query outputs; same-register alternatives remain possible.
int AddQueryResult(VolumeLineQuery* lpQuery,
                   VolumeLineSegIntersectResult* lpResult,
                   const TriangleVolume* lpVolume,
                   const math::vpu::Matrix44Affine* lpTransform,
                   math::vpu::Vector3 localPosition,
                   math::vpu::VecFloat lineParam,
                   math::vpu::Vector3 volParam,
                   math::vpu::Vector3 normal,
                   const u32* lpClusterId,
                   const u32* lpUnitOffset,
                   const u8* lpNumTagBits,
                   const u32* lpNumMeshTagBits,
                   const u32* lpNumClusterTagBits);// stack

} // namespace collision
} // namespace rw

#ifndef CGS_TRIANGLE_BOX_H
#define CGS_TRIANGLE_BOX_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3 / VecFloat (rw::math::vpu aliases)

// ============================================================================
// GameShared/GameClasses/Geometric/Intersection/CgsTriangleBox.h
//
// CgsGeometric::BT -- "Box/Triangle" clip-test scratch data used by the
// triangle-vs-AABB clipper (CgsGeometric::ClipTriWithAABB /
// ClipBoxEdgeAgainstTriangle / ClipTriEdgeAgainstBox, all in this same X360
// TU). This header is the OWNING home for the class shape; only
// BT::TriangleData::Construct is bodied here (its own ledger TU) -- the rest
// of CgsTriangleBox.cpp's free functions and BT::BoxVertexData::Construct are
// declared to match the DWARF/asm shape but live in their own TUs.
//
// LAYOUT (DecFIGS DWARF for GameShared/GameClasses/Geometric/Intersection/
// CgsTriangleBox.cpp, confirmed against the Construct asm's stvx128 store
// offsets -- see CgsTriangleBox.cpp for the per-store mapping):
//
//   +0x00  Vector3  mVertex0        triangle vertex 0
//   +0x10  Vector3  mVertex1        triangle vertex 1
//   +0x20  Vector3  mVertex2        triangle vertex 2
//   +0x30  Vector3  mNormal         unit face normal
//   +0x40  Vector3  mEdge01         mVertex1 - mVertex0
//   +0x50  Vector3  mEdge12         mVertex2 - mVertex1
//   +0x60  Vector3  mEdge20         mVertex0 - mVertex2
//   +0x70  Vector3  mOuter01        outward in-plane normal of edge01 (edge01 x mNormal, normalized)
//   +0x80  Vector3  mOuter12        outward in-plane normal of edge12 (edge12 x mNormal, normalized)
//   +0x90  Vector3  mOuter20        outward in-plane normal of edge20 (edge20 x mNormal, normalized)
//   +0xA0  VecFloat mPlaneOffset    dot3(mVertex0, mNormal) -- signed distance of the triangle plane from the origin
//   sizeof == 0xB0 (176)
//
// The mOuterXX vectors are the standard "triangle prism" clip planes used by
// separating-axis box/triangle clipping: each lies in the triangle's plane,
// is perpendicular to its edge, and points away from the triangle interior --
// exactly the data ClipBoxEdgeAgainstTriangle/ClipTriEdgeAgainstBox need to
// clip a box edge against the triangle's three side "walls" plus its face.
// ============================================================================

namespace CgsGeometric
{
    namespace BT
    {
        struct TriangleData
        {
            Vector3  mVertex0;      // +0x00  triangle vertex 0
            Vector3  mVertex1;      // +0x10  triangle vertex 1
            Vector3  mVertex2;      // +0x20  triangle vertex 2
            Vector3  mNormal;       // +0x30  unit face normal
            Vector3  mEdge01;       // +0x40  mVertex1 - mVertex0
            Vector3  mEdge12;       // +0x50  mVertex2 - mVertex1
            Vector3  mEdge20;       // +0x60  mVertex0 - mVertex2
            Vector3  mOuter01;      // +0x70  normalize(Cross(mEdge01, mNormal))
            Vector3  mOuter12;      // +0x80  normalize(Cross(mEdge12, mNormal))
            Vector3  mOuter20;      // +0x90  normalize(Cross(mEdge20, mNormal))
            VecFloat mPlaneOffset;  // +0xA0  Dot(mVertex0, mNormal)

            // Build the clip-test scratch data for triangle (lVert0, lVert1, lVert2):
            // vertices, face normal, three edges, three outward in-plane edge normals,
            // and the plane offset. (Body @ 0x82839D30.)
            void Construct(const Vector3& lVert0, const Vector3& lVert1, const Vector3& lVert2);
        };

        // BT::BoxVertexData -- the sibling clip-scratch struct for a box vertex
        // (per-vertex signed distance to the triangle plane + a caller-plane
        // offset). Declared to match the DWARF class shape for this TU; its
        // Construct body lives in its own TU (CgsTriangleBox.cpp @ ~L84-96).
        struct BoxVertexData
        {
            Vector3  mPosition;                  // +0x00
            VecFloat mPlaneOffset;                // +0x10
            VecFloat mPlaneOffsetFromTriangle;    // +0x20

            void Construct(const VecFloat& lX, const VecFloat& lY, const VecFloat& lZ,
                            const Vector3& lTriangleNormal, const VecFloat& lTriangleOffset);
        };
    }
}

#endif // CGS_TRIANGLE_BOX_H

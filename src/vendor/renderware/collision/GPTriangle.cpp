#include "vendor/renderware/collision/GPInstance.hpp"

#include <cmath>   // fabs

// ===========================================================================
// rw::collision::GPTriangle -- the triangle primitive's VolumeMethods
// callbacks, reconstructed from BURNOUT_X360_ARTIST.XEX (dedicated VMX pass).
//
//   GPTriangle::GetMaximumFeature  @ 0x82BBA158   (VolumeMethods +0xA4)
//   GPTriangle::GetInterval        @ 0x82BBA6A0   (VolumeMethods +0xA8)
//   GPTriangle::GetIntervals       @ 0x82BBA6E0   (VolumeMethods +0xAC)
//
// Canonical declarations rwccore.h:1229 (non-static const members on PS3);
// reconstructed as static plain-function callbacks matching the committed
// GPInstance::VolumeMethods typedefs per the X360 delta note in GPInstance.hpp.
//
// Instance layout (the same vertex aliases AABBoxBuilder::CreateFromTriangle
// and TriangleVolume use):
//   mPos            (+0x00)  Vertex0
//   mFaceNormals[0] (+0x10)  unit face normal N (mNumFaceNormals == 1)
//   mFaceNormals[1] (+0x20)  Vertex1
//   mFaceNormals[2] (+0x30)  Vertex2
//   mEdgeDirections[0..2] (+0x40/+0x50/+0x60)  the three unit edge directions
//   mDimensions     (+0x70)  the three edge lengths in lanes x/y/z
//
// The triangle's three boundary edges are (base, direction, length):
//   E0 = (Vertex0, edgeDir0, len.x)
//   E1 = (Vertex2, edgeDir1, len.y)
//   E2 = (Vertex1, edgeDir2, len.z)
// -- exactly the ordering the FACE case (block A) memcpys into edges[0..2] and
// the EDGE case selects from; taken as ground truth from the asm.
//
// VMX decode notes (asm authoritative):
//   * vmsum3fp128 = xyz dot fold broadcast to all lanes -> scalar Dot3 + Splat.
//   * the direction rides in VMX v1 (spilled to the stack near the top and
//     handed to Feature::BuildEdgePlanes by reference at the tail).
//   * the lvsl(0, 4*lane) / vspltw / vperm triples are lane broadcasts of
//     mDimensions element 0/1/2 (the per-edge lengths) -- NOT rodata permute
//     tables.
//   * vspltisw(-1) + vslw builds the 0x80000000 lane mask: vandc against it is
//     fabs, vxor against it is per-lane negate.
//   * every branch is a vcmpgtfp. CR6[0] (all-lanes-true) bit test, extracted
//     by mfocrf r11,2 + extrwi. r11,r11,1,24 (mfocrf mask 2 selects CR field 6;
//     bit 24 of the 32-bit CR word is CR6[0], the ALL-four-lanes-greater flag --
//     CR6[2] is the none-true flag). It is NOT a lane-0 result test. All the
//     operands are lane-broadcast anyway (vmsum3fp128 replicates the dot3 into
//     all four words, the epsilons are vspltw-broadcast, and vandc against a
//     broadcast sign mask preserves that), so all-lanes-true and lane-0-true
//     coincide and this lowers exactly to a scalar `>`.
// ===========================================================================

namespace rw
{
namespace collision
{

namespace
{
    // dot3 of the xyz lanes (the asm's vmsum3fp128, whose single result is
    // replicated into all four words -- not just lane 0).
    inline f32 Dot3(const Vec4& a, const Vec4& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    // A vmsum3fp128 / vminfp / vmaxfp broadcast row: the scalar in all lanes.
    inline Vec4 Splat(f32 s)
    {
        Vec4 r;
        r.x = s;
        r.y = s;
        r.z = s;
        r.w = s;
        return r;
    }

    // vspltisw(-1)/vslw sign mask + vxor: flip every lane's sign.
    inline Vec4 Negate(const Vec4& a)
    {
        Vec4 r;
        r.x = -a.x;
        r.y = -a.y;
        r.z = -a.z;
        r.w = -a.w;
        return r;
    }

    // The FACE case's three edge records (block A build order): base row,
    // direction row, and the per-edge length broadcast. pn (+0x20) is left for
    // Feature::BuildEdgePlanes to fill.
    inline void SetEdge(FeatureEdge& arEdge, const Vec4& arBase,
                        const Vec4& arDir, f32 afLength)
    {
        arEdge.base   = arBase;
        arEdge.dir    = arDir;
        arEdge.length = Splat(afLength);
    }

    // The EDGE feature: one record in edges[0] + numedges = 1 (the asm memcpys
    // a base/dir/length triple to out+0x10 and stores 1 at +0x230).
    inline void EmitEdge(Feature& arFeature, const Vec4& arBase,
                         const Vec4& arDir, f32 afLength)
    {
        SetEdge(arFeature.edges[0], arBase, arDir, afLength);
        arFeature.numedges = 1;
    }

    // The VERTEX feature: numedges = 0 and the point stored at +0x220.
    inline void EmitVertex(Feature& arFeature, const Vec4& arPoint)
    {
        arFeature.numedges = 0;
        arFeature.pt       = arPoint;
    }
}

// flt_82004FDC: the face-alignment gate -- |dot3(dir, N)| above this makes the
// whole face the maximum feature (value from the export's pseudocode literal).
static const f32 KF_FACE_ALIGN_THRESHOLD = 0.94999999f;

// flt_82180A90: the edge-perpendicularity gate -- an edge direction whose
// |dot3(dir, edgeDir)| is below this counts as "perpendicular to the query"
// and promotes the winning vertex to that edge (pseudocode literal).
static const f32 KF_EDGE_PERP_THRESHOLD = 0.050000001f;

// ===========================================================================
// rw::collision::GPTriangle::GetMaximumFeature @ 0x82BBA158
// Reached through mMethods.mGetMaximumFeature by the committed
// PrimitiveIntersect.cpp batch kernels (pass 2) and ComputeContactPoints.
//
// Phase 1 tests the query direction against the face normal: when it is nearly
// parallel (|dot| > 0.95) the whole triangle FACE (all three edges) is the
// feature, wound one way or the other by (sign(faceDot) == abCcw). Otherwise
// the best-projecting VERTEX is found and, if one of its two incident edges is
// nearly perpendicular to the query (and more so than the other), that EDGE is
// promoted to the feature. Every path ends by building the boundary-edge prism
// planes with the query direction as the extrusion axis.
// ===========================================================================
void GPTriangle::GetMaximumFeature(const GPInstance* lpThis, RwBool abCcw,
                                   const Vec4& arDir, Feature& arFeature)
{
    const Vec4& lrV0  = lpThis->mPos;                 // +0x00  Vertex0
    const Vec4& lrN   = lpThis->mFaceNormals[0];      // +0x10  face normal
    const Vec4& lrV1  = lpThis->mFaceNormals[1];      // +0x20  Vertex1
    const Vec4& lrV2  = lpThis->mFaceNormals[2];      // +0x30  Vertex2
    const Vec4& lrE0  = lpThis->mEdgeDirections[0];   // +0x40  edge dir 0
    const Vec4& lrE1  = lpThis->mEdgeDirections[1];   // +0x50  edge dir 1
    const Vec4& lrE2  = lpThis->mEdgeDirections[2];   // +0x60  edge dir 2
    const Vec4& lrLen = lpThis->mDimensions;          // +0x70  edge lengths xyz

    // lvx128 [this+0x10] + vmsum3fp128 v0,v1,v0 -> dot3(dir, N).
    const f32 lfFaceDot = Dot3(arDir, lrN);

    if (std::fabs(lfFaceDot) > KF_FACE_ALIGN_THRESHOLD)   // fabs / fcmpu / ble
    {
        // ===================================================================
        // FACE feature: all three boundary edges.
        // Winding: block A when (faceDot < 0 ? 1 : 0) == abCcw, else block B
        // (which reverses the ring: negated edge directions, swapped bases).
        // ===================================================================
        const s32 liSignFlag = (lfFaceDot < 0.0f) ? 1 : 0;   // blt / li 1 / mr 0

        if (liSignFlag == abCcw)
        {
            // Block A (memcpys to edges[0..2]; region = 8).
            SetEdge(arFeature.edges[0], lrV0, lrE0, lrLen.x);
            SetEdge(arFeature.edges[1], lrV2, lrE1, lrLen.y);
            SetEdge(arFeature.edges[2], lrV1, lrE2, lrLen.z);
            arFeature.region = 8;
        }
        else
        {
            // Block B (negated edge directions; region = 0).
            SetEdge(arFeature.edges[0], lrV0, Negate(lrE2), lrLen.z);
            SetEdge(arFeature.edges[1], lrV1, Negate(lrE1), lrLen.y);
            SetEdge(arFeature.edges[2], lrV2, Negate(lrE0), lrLen.x);
            arFeature.region = 0;
        }

        arFeature.numedges  = 3;      // stw 3 -> +0x230
        arFeature.ownNormal = lrN;    // lvx128 [this+0x10] -> stvx128 +0x210
    }
    else
    {
        // ===================================================================
        // VERTEX / EDGE feature. Project the three vertices along the query
        // and take the |edge . dir| magnitudes for the perpendicularity test.
        // ===================================================================
        const f32 lfDotV0 = Dot3(arDir, lrV0);   // vmsum3fp128 v13
        const f32 lfDotV1 = Dot3(arDir, lrV1);   // vmsum3fp128 v12
        const f32 lfDotV2 = Dot3(arDir, lrV2);   // vmsum3fp128 v5

        const f32 lfAbsE0 = std::fabs(Dot3(arDir, lrE0));   // vandc v0
        const f32 lfAbsE1 = std::fabs(Dot3(arDir, lrE1));   // vandc v4
        const f32 lfAbsE2 = std::fabs(Dot3(arDir, lrE2));   // vandc v3

        if (lfDotV0 > lfDotV1 && lfDotV0 > lfDotV2)
        {
            // Vertex0 is the support vertex; its incident edges are E0 and E2.
            if (KF_EDGE_PERP_THRESHOLD > lfAbsE0 && lfAbsE2 > lfAbsE0)
            {
                EmitEdge(arFeature, lrV0, lrE0, lrLen.x);   // E0
            }
            else if (KF_EDGE_PERP_THRESHOLD > lfAbsE2)
            {
                EmitEdge(arFeature, lrV1, lrE2, lrLen.z);   // E2
            }
            else
            {
                EmitVertex(arFeature, lrV0);
            }
        }
        else if (lfDotV1 > lfDotV2)
        {
            // Vertex1 is the support vertex; its incident edges are E1 and E2.
            if (KF_EDGE_PERP_THRESHOLD > lfAbsE1 && lfAbsE2 > lfAbsE1)
            {
                EmitEdge(arFeature, lrV2, lrE1, lrLen.y);   // E1
            }
            else if (KF_EDGE_PERP_THRESHOLD > lfAbsE2)
            {
                EmitEdge(arFeature, lrV1, lrE2, lrLen.z);   // E2
            }
            else
            {
                EmitVertex(arFeature, lrV1);
            }
        }
        else
        {
            // Vertex2 is the support vertex; its incident edges are E0 and E1.
            if (KF_EDGE_PERP_THRESHOLD > lfAbsE0 && lfAbsE1 > lfAbsE0)
            {
                EmitEdge(arFeature, lrV0, lrE0, lrLen.x);   // E0
            }
            else if (KF_EDGE_PERP_THRESHOLD > lfAbsE1)
            {
                EmitEdge(arFeature, lrV2, lrE1, lrLen.y);   // E1
            }
            else
            {
                EmitVertex(arFeature, lrV2);
            }
        }
    }

    // All paths: build the boundary-edge prism-wall planes, extruding along the
    // query direction (v1, spilled to the stack near the top and passed by
    // reference). BuildEdgePlanes iterates edges[0, numedges) -- a no-op for the
    // vertex case (numedges == 0).
    arFeature.BuildEdgePlanes(abCcw, arDir);
}

// ===========================================================================
// rw::collision::GPTriangle::GetInterval @ 0x82BBA6A0
//
// Projection interval of the triangle onto a direction: the min/max of the
// three vertices' dot products with the query. The vertices are loaded from
// this+0x00 / this+0x20 / this+0x30 (Vertex0 / Vertex1 / Vertex2). Both rows
// are lane-broadcast VecFloats (min -> +0x00, max -> +0x10).
// ===========================================================================
void GPTriangle::GetInterval(const GPInstance* lpThis,
                             const Vec4& arDir, Interval& arInterval)
{
    // vmsum3fp128 of each vertex row against v1 (the query direction).
    const f32 lfV0 = Dot3(arDir, lpThis->mPos);            // lvx128 [this+0x00]
    const f32 lfV1 = Dot3(arDir, lpThis->mFaceNormals[1]); // lvx128 [this+0x20]
    const f32 lfV2 = Dot3(arDir, lpThis->mFaceNormals[2]); // lvx128 [this+0x30]

    // vminfp/vmaxfp folds: (v0,v1) then v2 (asm op order preserved).
    const f32 lfMin01 = (lfV1 < lfV0) ? lfV1 : lfV0;   // vminfp v11, v0, v13
    const f32 lfMin   = (lfV2 < lfMin01) ? lfV2 : lfMin01;   // vminfp v13, v11, v12
    const f32 lfMax01 = (lfV1 > lfV0) ? lfV1 : lfV0;   // vmaxfp v0, v0, v13
    const f32 lfMax   = (lfV2 > lfMax01) ? lfV2 : lfMax01;   // vmaxfp v0, v0, v12

    arInterval.min = Splat(lfMin);   // stvx128 v13, r0, r4
    arInterval.max = Splat(lfMax);   // stvx128 v0, r4, r9  (r4+0x10)
}

// ===========================================================================
// rw::collision::GPTriangle::GetIntervals @ 0x82BBA6E0
// Called by the committed FindBestSeparatingDirection through
// mMethods.mGetIntervals.
//
// Batched form of GetInterval: one projection interval per direction, dirs
// advancing 0x10 and intervals 0x30 (the console Interval array stride).
// ===========================================================================
void GPTriangle::GetIntervals(const GPInstance* lpThis, const Vec4* lapDirs,
                              u32 auNumDirs, Interval* lapIntervals)
{
    // cmplwi cr6, r5, 0 / beqlr cr6.
    if (auNumDirs == 0)
    {
        return;
    }

    const Vec4& lrV0 = lpThis->mPos;            // r3      (this)
    const Vec4& lrV1 = lpThis->mFaceNormals[1]; // r8 = r3+0x20  (hoisted base)
    const Vec4& lrV2 = lpThis->mFaceNormals[2]; // r7 = r3+0x30

    // addic. r10, r5, -1 / bne loop.
    u32         luRemaining = auNumDirs;
    const Vec4* lpDir       = lapDirs;    // r9
    Interval*   lpOut       = lapIntervals;   // r11
    do
    {
        const Vec4& lrDir = *lpDir;   // lvx128 [r9]

        const f32 lfV0 = Dot3(lrDir, lrV0);   // vmsum3fp128 v13
        const f32 lfV1 = Dot3(lrDir, lrV1);   // vmsum3fp128 v12
        const f32 lfV2 = Dot3(lrDir, lrV2);   // vmsum3fp128 v0

        const f32 lfMin01 = (lfV1 < lfV0) ? lfV1 : lfV0;   // vminfp v11, v13, v12
        const f32 lfMin   = (lfV2 < lfMin01) ? lfV2 : lfMin01;   // vminfp v12, v11, v0
        const f32 lfMax01 = (lfV1 > lfV0) ? lfV1 : lfV0;   // vmaxfp v13, v13, v12
        const f32 lfMax   = (lfV2 > lfMax01) ? lfV2 : lfMax01;   // vmaxfp v0, v13, v0

        lpOut->min = Splat(lfMin);   // stvx128 v12, r0, r11
        lpOut->max = Splat(lfMax);   // stvx128 v0, r11, r6  (r11+0x10)

        ++lpDir;    // addi r9, r9, 0x10
        ++lpOut;    // addi r11, r11, 0x30
    }
    while (--luRemaining != 0);
}

} // namespace collision
} // namespace rw

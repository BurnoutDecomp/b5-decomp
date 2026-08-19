#include "vendor/renderware/collision/GPInstance.hpp"

#include "vendor/renderware/collision/CollisionVolume.hpp"   // Volume (128-byte image)

#include <cmath>     // sqrt
#include <cstring>   // memmove

// ===========================================================================
// rw::collision narrow-phase batch kernels -- reconstructed from
// BURNOUT_X360_ARTIST.XEX (dedicated VMX pass; every hand-vectorised body is
// lowered to portable per-lane scalar maths per the committed Feature /
// FeatureEdge precedent, preserving branch polarity, store order and every
// caller-visible store).
//
//   rw::collision::PrimitiveBatchIntersect       @ 0x82BABC78
//   rw::collision::GPInstanceBatchIntersect1xN   @ 0x82BAB4A8
//   rw::collision::GPInstanceBatchIntersectNx1   @ 0x82BAACD8
//   rw::collision::ComputeContactPoints          @ 0x82BABDA8
//
// Shared vocabulary (GPInstance / Interval / PrimitivePairIntersectResult /
// rwc_FeatureIntersectionPrism / VolRef1xN) lives in GPInstance.hpp.
// ===========================================================================

namespace rw
{
namespace collision
{

// X360 flt_8218025C -- the degenerate single-contact gap guard (value attested
// by the export's literal 0.00000011920929 == FLT_EPSILON). Shared by both
// batch kernels and ComputeContactPoints.
static const f32 KF_DEGENERATE_GAP_EPSILON = 1.1920929e-7f;

namespace
{
    // dot3 of the xyz lanes (the asm's vmsum3fp128; the broadcast result's
    // lane 0 is what every consumer reads).
    inline f32 Dot3(const Vec4& a, const Vec4& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    // vsubfp: per-lane a - b (all four lanes, w included).
    inline Vec4 Sub(const Vec4& a, const Vec4& b)
    {
        Vec4 r;
        r.x = a.x - b.x;
        r.y = a.y - b.y;
        r.z = a.z - b.z;
        r.w = a.w - b.w;
        return r;
    }

    // vaddfp: per-lane a + b.
    inline Vec4 Add(const Vec4& a, const Vec4& b)
    {
        Vec4 r;
        r.x = a.x + b.x;
        r.y = a.y + b.y;
        r.z = a.z + b.z;
        r.w = a.w + b.w;
        return r;
    }

    // vmulfp128 against a lvlx/vspltw splat: per-lane a * s.
    inline Vec4 Scale(const Vec4& a, f32 s)
    {
        Vec4 r;
        r.x = a.x * s;
        r.y = a.y * s;
        r.z = a.z * s;
        r.w = a.w * s;
        return r;
    }

    // vmaddfp against a splat multiplier: per-lane a*s + b.
    inline Vec4 MaddScalar(const Vec4& a, f32 s, const Vec4& b)
    {
        Vec4 r;
        r.x = a.x * s + b.x;
        r.y = a.y * s + b.y;
        r.z = a.z * s + b.z;
        r.w = a.w * s + b.w;
        return r;
    }

    // vspltisw v0,-1 / vslw v0,v0,v0 / vxor: build the 0x80000000 sign-bit
    // splat and flip every lane's sign.
    inline Vec4 Negate(const Vec4& a)
    {
        Vec4 r;
        r.x = -a.x;
        r.y = -a.y;
        r.z = -a.z;
        r.w = -a.w;
        return r;
    }

    // The shared support-interval orientation test of the two batch kernels
    // (inlined twice in each: the single-contact and multi-contact paths):
    //   d1 = interval1.min - interval2.max      (vsubfp on the broadcast rows)
    //   d2 = interval2.min - interval1.max      (vsubfp)
    //   mask           = d1 > d2                (vcmpgtfp)
    //   maxSep         = vsel(d2, d1, mask)     (per-lane max)
    //   lbAllSeparated = ALL lanes: splat(distance) > maxSep  (vcmpgtfp. CR6[all])
    //   lbAnyFlip      = ANY lane of mask set; the asm tests it as
    //                    ALL lanes: (mask-as-float == 0)       (vcmpeqfp. CR6[all])
    //                    -- a true mask lane is 0xFFFFFFFF (NaN as float), so
    //                    the all-equal-zero test is exactly "no lane was >".
    // NaN polarity matches C++: `>` is false on unordered, as vcmpgtfp is.
    inline void ClassifyIntervalSeparation(const Interval& arInterval1,
                                           const Interval& arInterval2,
                                           f32 afDistance,
                                           bool& rbAllSeparated, bool& rbAnyFlip)
    {
        const f32* laf1Min = &arInterval1.min.x;
        const f32* laf1Max = &arInterval1.max.x;
        const f32* laf2Min = &arInterval2.min.x;
        const f32* laf2Max = &arInterval2.max.x;

        rbAllSeparated = true;
        rbAnyFlip      = false;
        for (int liLane = 0; liLane < 4; ++liLane)
        {
            const f32  lfD1      = laf1Min[liLane] - laf2Max[liLane];
            const f32  lfD2      = laf2Min[liLane] - laf1Max[liLane];
            const bool lbGreater = lfD1 > lfD2;                          // vcmpgtfp
            const f32  lfMax     = lbGreater ? lfD1 : lfD2;              // vsel
            rbAllSeparated = rbAllSeparated && (afDistance > lfMax);     // vcmpgtfp. all
            rbAnyFlip      = rbAnyFlip || lbGreater;                     // !vcmpeqfp(mask,0) all
        }
    }
}

// ===========================================================================
// rw::collision::GPTriangleAcceptContactNormal @ 0x82BAA600  (waveQ5 C1)
//
// THE shared narrow-phase gate: every one of the six contact-emitting paths in
// this file (both batch kernels, ComputeContactPoints and PrimitivePairIntersect)
// ends by running it over any TRIANGLE operand -- with +normal for the first
// side and -normal for the second -- and discards the contact on a zero
// return. Until it existed no pair type could produce a contact at all.
//
// The binary symbol is UNNAMED (`sub_82BAA600`); the name is descriptive. No
// DWARF and no Feb-2007 declaration cover it (the Feb-2007 rwccore.h carries
// GPTriangle and ComputeContactPoints but not this static), so the signature
// is taken from the asm: r3 = the triangle GPInstance, v1 = the candidate
// contact normal. It is homed here, not in GPTriangle.cpp, because the binary
// puts it in THIS run (0x82BAA1A0 SAT thunks -> 0x82BAA600 -> 0x82BAACD8
// GPInstanceBatchIntersectNx1), and its three helpers below are statics that
// nothing outside the run calls.
//
// WHAT IT IS FOR. A GPTriangle is one face of a mesh. A contact normal that
// points out of the plane of the face is this triangle's business; one that
// points sideways, past an edge or a vertex, belongs to the NEIGHBOUR the
// mesh shares that edge/vertex with -- accepting it is what makes a car catch
// on an internal mesh seam. So:
//   1. Project the candidate normal on the face normal. If it is within
//      0.99985 of (anti)parallel the normal is "on the face" -- region 0.
//   2. Otherwise strip the face component, renormalise, and classify the
//      remaining in-plane direction into one of six Voronoi sectors around
//      the triangle (three edges, three vertices) -- ClassifyInPlaneRegion.
//   3. Accept or reject that sector using the instance's flags: the
//      per-edge FLAG_TRIANGLEEDGE*CONVEX bits (is the shared edge convex, i.e.
//      does this triangle own the space beyond it?), the per-vertex
//      FLAG_TRIANGLEVERT*DISABLE bits, and -- when FLAG_TRIANGLEUSEEDGECOS is
//      set -- the three stored edge cosines in mEdgeData, which give the exact
//      dihedral half-angle instead of a yes/no convexity bit.
//
// SECTOR NUMBERING (from ClassifyInPlaneRegion, and confirmed by the flag each
// sector tests on the no-edge-cos path):
//   0 = the face itself (never returned by the classifier; it is the
//       "no classification was run" value)
//   1 = edge 0     2 = edge 1     4 = edge 2
//   5 = vertex 0   3 = vertex 1   6 = vertex 2
// A vertex sector is bounded by TWO edges, and the pairing the asm uses is
// consistent across all three vertex cases: logical edge k is
// mEdgeDirections[2-k] (edge 0 <-> mEdgeDirections[2], edge 1 <->
// mEdgeDirections[1], edge 2 <-> mEdgeDirections[0]), and vertex k is bounded
// by edges k-1 and k. That pairing is taken from the asm, not assumed.
//
// RETURN VALUE: nonzero = accept. The console returns the tested flag BIT
// itself on the no-edge-cos paths (0x20 / 0x40 / 0x80), 0/1 elsewhere -- so it
// is a truthiness, never a canonical 1. Reproduced exactly.
//
// VMX lowering (asm authoritative, same conventions as the rest of this file):
//   * vmsum3fp128 = xyz dot fold broadcast to all four lanes -> scalar Dot3.
//   * vspltisw(-1)+vslw builds the 0x80000000 lane mask; vandc against it is
//     fabs, vxor against it is per-lane negate.
//   * lvsl(0, 4*k) + vspltw + vperm is a LANE BROADCAST of mEdgeData[k] out of
//     the (ec0,ec1,ec2,0) row staged on the stack -- not a rodata permute
//     table (AGENTS gotcha 5).
//   * vcmpgtfp./vcmpgefp. + mfocrf r11,2 + extrwi. r11,r11,1,24 reads CR6[0]
//     = "all four lanes". Every operand here is lane-broadcast, so it lowers
//     to a scalar compare; NaN polarity matches C++ (`>` false on unordered).
//   * vrsqrtefp + two vnmsubfp/vmaddfp Newton-Raphson steps -> exact
//     1/sqrt, i.e. a plain normalise.
//
// .rdata: flt_82F917F4 == unk_82F917F8 == 0x3F7FF62B == 0.99984998f (two
// separate literal slots holding the same value; both dumped on a private .i64
// copy), unk_821802C4 == 0x3D4CCCCD == 0.05f, flt_82001CC0 == 0.0f.
// ===========================================================================

// flt_82F917F4 / unk_82F917F8 -- |dot(normal, faceNormal)| at or above this
// counts as "the normal lies on the face", so no in-plane classification runs
// (and, on the no-edge-cos paths, accepts outright).
static const f32 KF_FACE_PARALLEL_COSINE = 0.99984998f;

// unk_821802C4 -- the in-plane sector epsilon of the Voronoi classifier.
static const f32 KF_INPLANE_SECTOR_EPSILON = 0.050000001f;

namespace
{
    // vpermwi128 0x63 selects words (1,2,0,3) == the (y,z,x,w) shuffle; the
    // vmulfp128 / vnmsubfp / vpermwi triple in both edge-fan helpers below is
    // a plain xyz cross product (its w lane cancels to zero and is only ever
    // consumed by a dot3).
    inline Vec4 Cross3(const Vec4& a, const Vec4& b)
    {
        Vec4 r;
        r.x = a.y * b.z - a.z * b.y;
        r.y = a.z * b.x - a.x * b.z;
        r.z = a.x * b.y - a.y * b.x;
        r.w = 0.0f;
        return r;
    }

    // vrsqrtefp + two Newton-Raphson refinements, then vmulfp128 over all four
    // lanes -- rendered exact.
    inline Vec4 Normalize3(const Vec4& a)
    {
        return Scale(a, 1.0f / std::sqrt(Dot3(a, a)));
    }

    // The component of arVec perpendicular to the unit axis arAxis
    // (vmsum3fp128 / vmulfp128 / vsubfp), renormalised.
    inline Vec4 NormalizedReject(const Vec4& arVec, const Vec4& arAxis)
    {
        return Normalize3(Sub(arVec, Scale(arAxis, Dot3(arVec, arAxis))));
    }

    // -----------------------------------------------------------------------
    // @ 0x82BAA2A8 -- ONE-SIDED edge-fan acceptance (static; only 0x82BAA600
    // calls it). arEdgeAxis is the NEGATED edge direction the caller stages,
    // arFaceNormal the triangle's face normal, arNormal the candidate contact
    // normal, afEdgeCos the edge's stored cosine and abEdgeConvex the edge's
    // FLAG_TRIANGLEEDGE*CONVEX bit.
    //
    // The candidate is accepted outright when it falls on the inner side of
    // the edge's half-plane (the cross product test). Otherwise it is only
    // accepted if the edge is marked convex AND the candidate, projected off
    // the edge axis and renormalised, still leans at least afEdgeCos toward
    // the face -- i.e. it is inside the dihedral wedge this triangle owns.
    // -----------------------------------------------------------------------
    RwBool AcceptEdgeFanOneSided(const Vec4& arEdgeAxis, const Vec4& arFaceNormal,
                                 const Vec4& arNormal, f32 afEdgeCos,
                                 u32 auEdgeConvex)
    {
        // vmsum3fp128 against the cross, vs flt_82001CC0 (0.0f).
        if (0.0f > Dot3(arNormal, Cross3(arEdgeAxis, arFaceNormal)))
        {
            return 1;                                   // li r3, 1
        }
        if (auEdgeConvex == 0)                          // cmplwi cr6, r7, 0
        {
            return 0;
        }
        return (Dot3(NormalizedReject(arNormal, arEdgeAxis), arFaceNormal) >= afEdgeCos)
                   ? 1 : 0;                             // vcmpgefp. CR6[0]
    }

    // -----------------------------------------------------------------------
    // @ 0x82BAA378 -- TWO-SIDED edge-fan acceptance (static; same caller).
    // Identical to the one-sided form except that a non-convex edge is not an
    // outright reject: the wedge is measured against the BACK face instead
    // (vxor sign flip of the face normal), because a two-sided triangle owns
    // the space on both sides of its plane.
    // -----------------------------------------------------------------------
    RwBool AcceptEdgeFanTwoSided(const Vec4& arEdgeAxis, const Vec4& arFaceNormal,
                                 const Vec4& arNormal, f32 afEdgeCos,
                                 u32 auEdgeConvex)
    {
        if (0.0f > Dot3(arNormal, Cross3(arEdgeAxis, arFaceNormal)))
        {
            return 1;                                   // li r3, 1
        }

        const Vec4 lvRejected = NormalizedReject(arNormal, arEdgeAxis);
        const Vec4 lvAgainst  = (auEdgeConvex != 0) ? arFaceNormal
                                                    : Negate(arFaceNormal);
        return (Dot3(lvRejected, lvAgainst) >= afEdgeCos) ? 1 : 0;
    }

    // -----------------------------------------------------------------------
    // @ 0x82BAA4A8 -- classify an IN-PLANE direction against the triangle's
    // three edge directions (static; same caller). r3 = the instance,
    // v1 = the direction; returns the sector 1..6 (never 0).
    //
    // The three dots are compared against +/-0.05 and against each other in a
    // fixed cascade; the six outcomes are the three edge sectors and the three
    // vertex sectors of the triangle's in-plane Voronoi diagram.
    // -----------------------------------------------------------------------
    u32 ClassifyInPlaneRegion(const GPInstance* lpTriangle, const Vec4& arDir)
    {
        const f32 lfD0 = Dot3(arDir, lpTriangle->mEdgeDirections[0]);   // lvx +0x40
        const f32 lfD1 = Dot3(arDir, lpTriangle->mEdgeDirections[1]);   // lvx +0x50
        const f32 lfD2 = Dot3(arDir, lpTriangle->mEdgeDirections[2]);   // lvx +0x60

        const f32 lfPosEps = KF_INPLANE_SECTOR_EPSILON;
        const f32 lfNegEps = -KF_INPLANE_SECTOR_EPSILON;   // vxor sign flip

        if (lfD0 > lfPosEps && lfNegEps > lfD1)
        {
            return 6;   // vertex 2
        }
        if (lfD1 > lfPosEps && lfNegEps > lfD2)
        {
            return 3;   // vertex 1
        }
        if (lfD2 > lfPosEps && lfNegEps > lfD0)
        {
            return 5;   // vertex 0
        }
        // vcmpgtfp then vcmpgefp against the negated siblings (`bge` is
        // `!(a < b)`, so these are written as `>=` / `>` exactly as emitted).
        if (lfD0 > -lfD2 && -lfD1 >= lfD0)
        {
            return 4;   // edge 2
        }
        if (lfD1 > -lfD0 && -lfD2 >= lfD1)
        {
            return 2;   // edge 1
        }
        return 1;       // edge 0
    }
}

u32 GPTriangleAcceptContactNormal(const GPInstance* lpTriangle, const Vec4& arNormal)
{
    const u32  luFlags   = lpTriangle->mFlags;              // lwz 0x94(r30)
    const Vec4 lvFaceN   = lpTriangle->mFaceNormals[0];     // lvx128 r30+0x10
    const f32  lfFaceDot = Dot3(arNormal, lvFaceN);         // vmsum3fp128 v7

    // Sector 0 == "the normal lies on the face": no classification is run.
    u32 luRegion = 0;                                       // li r31, 0 / mr r3, r31
    if (KF_FACE_PARALLEL_COSINE > std::fabs(lfFaceDot))     // vandc + vcmpgtfp.
    {
        luRegion = ClassifyInPlaneRegion(lpTriangle,
                                         NormalizedReject(arNormal, lvFaceN));
    }

    if ((luFlags & GPInstance::FLAG_TRIANGLEUSEEDGECOS) != 0)   // flags & 0x100
    {
        // The three stored edge cosines, staged as one (ec0,ec1,ec2,0) row and
        // lane-broadcast per use.
        const f32 lfEdgeCos0 = lpTriangle->mEdgeData[0];    // lfs 0x98(r30)
        const f32 lfEdgeCos1 = lpTriangle->mEdgeData[1];    // lfs 0x9C(r30)
        const f32 lfEdgeCos2 = lpTriangle->mEdgeData[2];    // lfs 0xA0(r30)

        const Vec4& lrEdgeDir0 = lpTriangle->mEdgeDirections[0];   // +0x40
        const Vec4& lrEdgeDir1 = lpTriangle->mEdgeDirections[1];   // +0x50
        const Vec4& lrEdgeDir2 = lpTriangle->mEdgeDirections[2];   // +0x60

        // The edge-fan helpers take the NEGATED edge direction (vxor sign
        // flip staged into the stack slot the call reads).
        const u32 luEdge0Convex = luFlags & GPInstance::FLAG_TRIANGLEEDGE0CONVEX;
        const u32 luEdge1Convex = luFlags & GPInstance::FLAG_TRIANGLEEDGE1CONVEX;
        const u32 luEdge2Convex = luFlags & GPInstance::FLAG_TRIANGLEEDGE2CONVEX;

        if ((luFlags & GPInstance::FLAG_TRIANGLEONESIDED) != 0)   // flags & 0x10
        {
            switch (luRegion)
            {
            case 0:
                // Face sector: only a front-facing normal survives.
                return (lfFaceDot > 0.0f) ? 1u : 0u;

            case 1:   // edge 0
                if (luEdge0Convex == 0)
                {
                    return 0;
                }
                return (lfFaceDot >= lfEdgeCos0) ? 1u : 0u;

            case 2:   // edge 1
                if (luEdge1Convex == 0)
                {
                    return 0;
                }
                return (lfFaceDot >= lfEdgeCos1) ? 1u : 0u;

            case 4:   // edge 2
                if (luEdge2Convex == 0)
                {
                    return 0;
                }
                return (lfFaceDot >= lfEdgeCos2) ? 1u : 0u;

            case 3:   // vertex 1 -- bounded by edge 0 and edge 1
                if ((luFlags & GPInstance::FLAG_TRIANGLEVERT1DISABLE) != 0)
                {
                    return 0;
                }
                if (!AcceptEdgeFanOneSided(Negate(lrEdgeDir2), lvFaceN, arNormal,
                                           lfEdgeCos0, luEdge0Convex))
                {
                    return 0;
                }
                return AcceptEdgeFanOneSided(Negate(lrEdgeDir1), lvFaceN, arNormal,
                                             lfEdgeCos1, luEdge1Convex);

            case 5:   // vertex 0 -- bounded by edge 2 and edge 0
                if ((luFlags & GPInstance::FLAG_TRIANGLEVERT0DISABLE) != 0)
                {
                    return 0;
                }
                if (!AcceptEdgeFanOneSided(Negate(lrEdgeDir0), lvFaceN, arNormal,
                                           lfEdgeCos2, luEdge2Convex))
                {
                    return 0;
                }
                return AcceptEdgeFanOneSided(Negate(lrEdgeDir2), lvFaceN, arNormal,
                                             lfEdgeCos0, luEdge0Convex);

            case 6:   // vertex 2 -- bounded by edge 1 and edge 2
                if ((luFlags & GPInstance::FLAG_TRIANGLEVERT2DISABLE) != 0)
                {
                    return 0;
                }
                if (!AcceptEdgeFanOneSided(Negate(lrEdgeDir1), lvFaceN, arNormal,
                                           lfEdgeCos1, luEdge1Convex))
                {
                    return 0;
                }
                return AcceptEdgeFanOneSided(Negate(lrEdgeDir0), lvFaceN, arNormal,
                                             lfEdgeCos2, luEdge2Convex);

            default:
                // Sector >= 7 is unreachable from the classifier; the console
                // still carries the accept-everything arm (cmplwi 7 / bge).
                return 1;
            }
        }

        // Two-sided + edge cosines.
        switch (luRegion)
        {
        case 0:
            return 1;

        case 1:   // edge 0
            return ((luEdge0Convex != 0) ? (lfFaceDot >= lfEdgeCos0)
                                         : (-lfFaceDot >= lfEdgeCos0)) ? 1u : 0u;

        case 2:   // edge 1
            return ((luEdge1Convex != 0) ? (lfFaceDot >= lfEdgeCos1)
                                         : (-lfFaceDot >= lfEdgeCos1)) ? 1u : 0u;

        case 4:   // edge 2
            return ((luEdge2Convex != 0) ? (lfFaceDot >= lfEdgeCos2)
                                         : (-lfFaceDot >= lfEdgeCos2)) ? 1u : 0u;

        case 3:   // vertex 1
            if ((luFlags & GPInstance::FLAG_TRIANGLEVERT1DISABLE) != 0)
            {
                return 0;
            }
            if (!AcceptEdgeFanTwoSided(Negate(lrEdgeDir2), lvFaceN, arNormal,
                                       lfEdgeCos0, luEdge0Convex))
            {
                return 0;
            }
            return AcceptEdgeFanTwoSided(Negate(lrEdgeDir1), lvFaceN, arNormal,
                                         lfEdgeCos1, luEdge1Convex);

        case 5:   // vertex 0
            if ((luFlags & GPInstance::FLAG_TRIANGLEVERT0DISABLE) != 0)
            {
                return 0;
            }
            if (!AcceptEdgeFanTwoSided(Negate(lrEdgeDir0), lvFaceN, arNormal,
                                       lfEdgeCos2, luEdge2Convex))
            {
                return 0;
            }
            return AcceptEdgeFanTwoSided(Negate(lrEdgeDir2), lvFaceN, arNormal,
                                         lfEdgeCos0, luEdge0Convex);

        case 6:   // vertex 2
            if ((luFlags & GPInstance::FLAG_TRIANGLEVERT2DISABLE) != 0)
            {
                return 0;
            }
            if (!AcceptEdgeFanTwoSided(Negate(lrEdgeDir1), lvFaceN, arNormal,
                                       lfEdgeCos1, luEdge1Convex))
            {
                return 0;
            }
            return AcceptEdgeFanTwoSided(Negate(lrEdgeDir0), lvFaceN, arNormal,
                                         lfEdgeCos2, luEdge2Convex);

        default:
            return 1;
        }
    }

    // ---- no edge cosines: the FLAG_TRIANGLEOLDMASK path --------------------
    // Only the convexity/disable bits arbitrate, after a cheap "the normal is
    // essentially the face normal" accept.
    //
    // NOTE (kept because the console keeps it): on the non-zero-sector arms
    // both KF_FACE_PARALLEL_COSINE tests below are PROVABLY DEAD -- a non-zero
    // sector only exists because KF_FACE_PARALLEL_COSINE > |lfFaceDot| already
    // held at the top, so neither `lfFaceDot >` nor `|lfFaceDot| >` can fire.
    // They are reproduced rather than folded away: they are real instructions
    // in the binary (0x82BAABB0 / 0x82BAAC24 re-load unk_82F917F8), and the
    // original source clearly wrote the two tests independently.
    if ((luFlags & GPInstance::FLAG_TRIANGLEONESIDED) != 0)
    {
        if (luRegion == 0)
        {
            return (lfFaceDot > 0.0f) ? 1u : 0u;        // splat(flt_82001CC0)
        }
        if (lfFaceDot > KF_FACE_PARALLEL_COSINE)        // unk_82F917F8
        {
            return 1;
        }
        if (0.0f > lfFaceDot)
        {
            return 0;   // one-sided: a normal pointing behind the face is never ours
        }
    }
    else
    {
        if (luRegion == 0)
        {
            return 1;
        }
        if (std::fabs(lfFaceDot) > KF_FACE_PARALLEL_COSINE)   // vandc + vcmpgtfp.
        {
            return 1;
        }
    }

    // The console returns the tested flag BIT here (rlwinm / not+extrwi), not a
    // canonical 1 -- preserved.
    switch (luRegion)
    {
    case 1:   // edge 0
        return luFlags & GPInstance::FLAG_TRIANGLEEDGE0CONVEX;
    case 2:   // edge 1
        return luFlags & GPInstance::FLAG_TRIANGLEEDGE1CONVEX;
    case 4:   // edge 2
        return luFlags & GPInstance::FLAG_TRIANGLEEDGE2CONVEX;
    case 3:   // vertex 1
        return ((luFlags & GPInstance::FLAG_TRIANGLEVERT1DISABLE) == 0) ? 1u : 0u;
    case 5:   // vertex 0
        return ((luFlags & GPInstance::FLAG_TRIANGLEVERT0DISABLE) == 0) ? 1u : 0u;
    case 6:   // vertex 2
        return ((luFlags & GPInstance::FLAG_TRIANGLEVERT2DISABLE) == 0) ? 1u : 0u;
    default:
        return 1;
    }
}

// ===========================================================================
// rw::collision::GPInstanceBatchIntersectNx1 @ 0x82BAACD8
//
// Three passes over min(aiNum, aiResBufMaxSize) pairs:
//   1. dispatch-table separation: per pair, the [typeN][type1] entry writes
//      the separating direction into the slot (sepDir @+0x4A0) and its lane-0
//      distance lands in the slot's distance field (+0x4F0, `addi r30, r26,
//      0x4F0` -- NOT the DWARF sepDist slot; see GPInstance.hpp).
//   2. fatness cull + feature extraction: pairs with
//      distance > fatnessN + padding + fatness1 are marked dead
//      (numPoints = 0, vNindex = -1); live pairs get their maximum features
//      built into f1/f2 (+dir for the N side, -dir for the 1 side).
//   3. prism intersection + contact resolution: live pairs run
//      FindFeatureIntersectionPrism; hits are compacted into
//      lapResults[hitCount] (the output cursor advances only on acceptance).
// Returns the number of accepted intersections.
// ===========================================================================
s32 GPInstanceBatchIntersectNx1(PrimitivePairIntersectResult* lapResults,
                                s32 aiResBufMaxSize,
                                const GPInstance* lapInsts1, s32 aiNum,
                                const GPInstance& arInst2,
                                f32 afPadding)
{
    s32 liNumIntersections = 0;

    // r28 = min(aiNum, aiResBufMaxSize)
    s32 liCount = aiNum;
    if (liCount > aiResBufMaxSize)
    {
        liCount = aiResBufMaxSize;
    }

    // ---- pass 1: separating-direction dispatch (0x82BAAD34 loop) ----------
    if (liCount > 0)
    {
        for (s32 liPair = 0; liPair < liCount; ++liPair)
        {
            const GPInstance*             lpInstance = &lapInsts1[liPair];
            PrimitivePairIntersectResult* lpSlot     = &lapResults[liPair];

            // off_82F91800[6*typeN + type1](sepDir, N, 1); the returned
            // broadcast's lane 0 (stvx128 v1 + lfs) is the separating distance.
            const FindBestSeparatingDirectionFn lpfnSeparate =
                gapFindBestSeparatingDirection[lpInstance->mVolumeType][arInst2.mVolumeType];
            lpSlot->distance = lpfnSeparate(lpSlot->sepDir, *lpInstance, arInst2);
        }
    }

    // ---- passes 2 + 3 (both under the same count guard, 0x82BAAD84) -------
    if (liCount > 0)
    {
        // ---- pass 2: fatness cull + maximum-feature extraction ------------
        for (s32 liPair = 0; liPair < liCount; ++liPair)
        {
            const GPInstance*             lpInstance = &lapInsts1[liPair];
            PrimitivePairIntersectResult* lpSlot     = &lapResults[liPair];

            // fcmpu cr6, distance, (fatnessN + padding) + fatness1; bgt -> cull.
            const f32 lfThreshold = (lpInstance->mFatness + afPadding) + arInst2.mFatness;
            if (lpSlot->distance > lfThreshold)
            {
                lpSlot->numPoints = 0;
                lpSlot->vNindex   = -1;
            }
            else
            {
                lpSlot->vNindex = liPair;

                // N side: maximum feature along +sepDir, ccw = 1.
                const Vec4 lvDir = lpSlot->sepDir;   // lvx128 slot+0x4A0
                lpInstance->mMethods.mGetMaximumFeature(lpInstance, 1, lvDir, lpSlot->f1);

                // 1 side: maximum feature along -sepDir (vspltisw/vslw/vxor
                // sign flip of the reloaded slot direction), ccw = 0.
                const Vec4 lvNegDir = Negate(lpSlot->sepDir);
                arInst2.mMethods.mGetMaximumFeature(&arInst2, 0, lvNegDir, lpSlot->f2);
            }
        }

        // ---- pass 3: prism intersection + contact resolution --------------
        // The asm reuses ONE stack work buffer (sp+0x1C0) across all
        // iterations, so it is hoisted here; only the normalOverride word is
        // re-zeroed per pair (stw r22, var_AC at 0x82BAAE54).
        rwc_FeatureIntersectionPrism lWork;

        for (s32 liPair = 0; liPair < liCount; ++liPair)
        {
            const GPInstance*             lpInstance = &lapInsts1[liPair];
            PrimitivePairIntersectResult* lpIn       = &lapResults[liPair];

            lWork.normalOverride = 0;

            // Fatness-culled pairs (vNindex < 0, signed test) are skipped.
            if (lpIn->vNindex < 0)
            {
                continue;
            }
            if (!FindFeatureIntersectionPrism(lWork, lpIn->f1, lpIn->f2, lpIn->sepDir))
            {
                continue;
            }
            // Capacity check happens AFTER the prism call, and it does not
            // terminate the loop (bge -> loop bottom): remaining pairs still
            // run FindFeatureIntersectionPrism.
            if (liNumIntersections >= aiResBufMaxSize)
            {
                continue;
            }

            // Output slot: results are compacted to lapResults[hitCount]
            // (r31 = output cursor; r23/r27 keep tracking the input pair).
            // NOTE: vNindex of the output slot is NOT rewritten on
            // compaction -- it keeps whatever pass 2 stored there, exactly
            // as the binary leaves it.
            PrimitivePairIntersectResult* lpOut = &lapResults[liNumIntersections];

            // numPoints is published to the output slot before the copy loop.
            const u32 luNumPoints = static_cast<u32>(lWork.m_numpts);
            lpOut->numPoints = luNumPoints;
            for (u32 luPoint = 0; luPoint < luNumPoints; ++luPoint)
            {
                lpOut->pointsOn2[luPoint] = lWork.m_ptsOn2[luPoint];   // buf+0x100 -> out+0x600
                lpOut->pointsOn1[luPoint] = lWork.m_ptsOn1[luPoint];   // buf+0x000 -> out+0x500
            }

            f32 lfDistanceOut;

            if (lpOut->numPoints == 1)
            {
                // ==== single-contact branch (0x82BAAED4) ====================
                // delta = pt2 - pt1; stored to the normal slot immediately
                // (stvx128 v13 -> out+0x4C0), then length-tested.
                const Vec4 lvDelta = Sub(lpOut->pointsOn2[0], lpOut->pointsOn1[0]);
                lpOut->normal = lvDelta;
                const f32 lfLenSq = Dot3(lvDelta, lvDelta);

                bool lbUseInputDir = false;
                if (lfLenSq <= KF_DEGENERATE_GAP_EPSILON)
                {
                    // fcmpu ble -> take the dispatch direction instead.
                    lbUseInputDir = true;
                }
                else
                {
                    // Normalise: vrsqrtefp estimate + two Newton-Raphson
                    // refine steps (vspltisw/vcfsx build the 1.0/0.5
                    // constants; vmulfp128/vnmsubfp/vmaddfp chain), then
                    // delta * recipLen. Rendered as the mathematically
                    // equivalent scalar.
                    const f32 lfInvLen = 1.0f / std::sqrt(lfLenSq);
                    lpOut->normal = Scale(lvDelta, lfInvLen);

                    // Support intervals of both primitives along the
                    // normalised direction (v1 = dir on both calls).
                    Interval lInterval1;
                    Interval lInterval2;
                    lpInstance->mMethods.mGetInterval(lpInstance, lpOut->normal, lInterval1);
                    arInst2.mMethods.mGetInterval(&arInst2, lpOut->normal, lInterval2);

                    // Orientation resolve against the pass-1 distance of the
                    // INPUT slot (lfs 0x50(r23) = in+0x4F0).
                    bool lbAllSeparated;
                    bool lbAnyFlip;
                    ClassifyIntervalSeparation(lInterval1, lInterval2, lpIn->distance,
                                               lbAllSeparated, lbAnyFlip);
                    if (lbAllSeparated)
                    {
                        lbUseInputDir = true;                          // -> LABEL_24
                    }
                    else if (lbAnyFlip)
                    {
                        // vxor sign flip of the stored direction.
                        lpOut->normal = Negate(lpOut->normal);
                    }
                    // else: keep the normalised direction (no store).
                }
                if (lbUseInputDir)
                {
                    // lvx128 v0, r0, r23 (input slot sepDir) -> out+0x4C0.
                    lpOut->normal = lpIn->sepDir;
                }

                // ==== LABEL_26: fatten both sides along the direction =======
                const f32  lfFatness1 = lpInstance->mFatness;   // lvlx + vspltw splat
                const f32  lfFatness2 = arInst2.mFatness;
                const Vec4 lvNormal   = lpOut->normal;

                // pointOn1 = dir*fat1 + pt1[0] (vmaddfp); pointOn2 = pt2[0]
                // - dir*fat2 (vmulfp128 + vsubfp) -- both from the UNfattened
                // point 0.
                lpOut->pointOn1 = MaddScalar(lvNormal, lfFatness1, lpOut->pointsOn1[0]);
                lpOut->pointOn2 = Sub(lpOut->pointsOn2[0], Scale(lvNormal, lfFatness2));

                // Per-point fatten + separation (loop guarded on numPoints,
                // count re-read from out+0x740 each iteration).
                for (u32 luPoint = 0; luPoint < lpOut->numPoints; ++luPoint)
                {
                    lpOut->pointsOn1[luPoint] =
                        MaddScalar(lvNormal, lfFatness1, lpOut->pointsOn1[luPoint]);
                    lpOut->pointsOn2[luPoint] =
                        Sub(lpOut->pointsOn2[luPoint], Scale(lvNormal, lfFatness2));
                    // separation = dot3(pt2'[i] - pt1'[i], dir) (vmsum3fp128 lane 0).
                    lpOut->distances[luPoint] =
                        Dot3(Sub(lpOut->pointsOn2[luPoint], lpOut->pointsOn1[luPoint]),
                             lvNormal);
                }

                // distance = dot3(pointOn2 - pointOn1, dir).
                lfDistanceOut = Dot3(Sub(lpOut->pointOn2, lpOut->pointOn1), lvNormal);
            }
            else
            {
                // ==== multi-contact branch (0x82BAB10C) =====================
                // Seed both averages with the 0.0f constant (flt_82001CC0)
                // and accumulate every contact point (vaddfp loop).
                Vec4 lvZero;
                lvZero.x = 0.0f;
                lvZero.y = 0.0f;
                lvZero.z = 0.0f;
                lvZero.w = 0.0f;
                lpOut->pointOn1 = lvZero;
                lpOut->pointOn2 = lvZero;
                for (u32 luPoint = 0; luPoint < lpOut->numPoints; ++luPoint)
                {
                    lpOut->pointOn1 = Add(lpOut->pointOn1, lpOut->pointsOn1[luPoint]);
                    lpOut->pointOn2 = Add(lpOut->pointOn2, lpOut->pointsOn2[luPoint]);
                }

                // 1/count: std/lfd/fcfid (exact s64->f64) + frsp + fdivs of
                // the 1.0f constant (flt_82001C98), splatted via lvlx+vspltw.
                // This kernel performs the divide once and reuses it.
                const f32 lfInvCount = 1.0f / static_cast<f32>(lpOut->numPoints);
                lpOut->pointOn1 = Scale(lpOut->pointOn1, lfInvCount);
                lpOut->pointOn2 = Scale(lpOut->pointOn2, lfInvCount);

                bool lbUseInputDir = false;
                if (lWork.normalOverride != 0)
                {
                    // dir = the prism normal (lvx128 buf+0x200 -> out+0x4C0).
                    lpOut->normal = lWork.normal;

                    Interval lInterval1;
                    Interval lInterval2;
                    lpInstance->mMethods.mGetInterval(lpInstance, lpOut->normal, lInterval1);
                    arInst2.mMethods.mGetInterval(&arInst2, lpOut->normal, lInterval2);

                    bool lbAllSeparated;
                    bool lbAnyFlip;
                    ClassifyIntervalSeparation(lInterval1, lInterval2, lpIn->distance,
                                               lbAllSeparated, lbAnyFlip);
                    if (lbAllSeparated)
                    {
                        lbUseInputDir = true;                          // -> LABEL_37
                    }
                    else if (lbAnyFlip)
                    {
                        lpOut->normal = Negate(lpOut->normal);
                    }
                }
                else
                {
                    lbUseInputDir = true;                              // -> LABEL_37
                }
                if (lbUseInputDir)
                {
                    lpOut->normal = lpIn->sepDir;
                }

                // ==== LABEL_39: fatten around the averaged points ===========
                const f32  lfFatness1 = lpInstance->mFatness;
                const f32  lfFatness2 = arInst2.mFatness;
                const Vec4 lvNormal   = lpOut->normal;

                lpOut->pointOn1 = MaddScalar(lvNormal, lfFatness1, lpOut->pointOn1);
                lpOut->pointOn2 = Sub(lpOut->pointOn2, Scale(lvNormal, lfFatness2));

                for (u32 luPoint = 0; luPoint < lpOut->numPoints; ++luPoint)
                {
                    lpOut->pointsOn1[luPoint] =
                        MaddScalar(lvNormal, lfFatness1, lpOut->pointsOn1[luPoint]);
                    lpOut->pointsOn2[luPoint] =
                        Sub(lpOut->pointsOn2[luPoint], Scale(lvNormal, lfFatness2));
                    lpOut->distances[luPoint] =
                        Dot3(Sub(lpOut->pointsOn2[luPoint], lpOut->pointsOn1[luPoint]),
                             lvNormal);
                }

                lfDistanceOut = Dot3(Sub(lpOut->pointOn2, lpOut->pointOn1), lvNormal);
            }

            // ==== LABEL_43: distance publish + triangle direction gates =====
            lpOut->distance = lfDistanceOut;                 // stfs -> out+0x4F0

            // N side: one-sided triangle rejection with +dir (v1 = dir).
            if (lpInstance->mVolumeType == GPInstance::TRIANGLE &&
                !GPTriangleAcceptContactNormal(lpInstance, lpOut->normal))
            {
                lpOut->numPoints = 0;
            }
            // 1 side: same test with -dir (vspltisw/vslw/vxor sign flip).
            if (arInst2.mVolumeType == GPInstance::TRIANGLE)
            {
                const Vec4 lvNegNormal = Negate(lpOut->normal);
                if (!GPTriangleAcceptContactNormal(&arInst2, lvNegNormal))
                {
                    lpOut->numPoints = 0;
                }
            }

            // ==== acceptance: publish tags/direction/features, advance ======
            if (lpOut->numPoints != 0)
            {
                lpOut->v1   = lpInstance->mVolumeTag;   // N +0x84 -> out+0x00
                lpOut->tag1 = lpInstance->mUserTag;     // N +0x88 -> out+0x04
                lpOut->v2   = arInst2.mVolumeTag;       // 1 +0x84 -> out+0x08
                lpOut->tag2 = arInst2.mUserTag;         // 1 +0x88 -> out+0x0C
                lpOut->sepDir = lpIn->sepDir;           // in+0x4A0 -> out+0x4A0

                // bl memcpy x2: both 0x240-byte Feature blocks from the input
                // slot. When no pair has been culled the ranges coincide
                // exactly (dst == src), so memmove is used for well-defined
                // x64 behaviour; the console memcpy is a no-op there too.
                memmove(&lpOut->f1, &lpIn->f1, sizeof(Feature));
                memmove(&lpOut->f2, &lpIn->f2, sizeof(Feature));

                ++liNumIntersections;
            }
        }
    }

    return liNumIntersections;
}

// ===========================================================================
// rw::collision::GPInstanceBatchIntersect1xN @ 0x82BAB4A8
//
// The 1-vs-N sibling of the kernel above (same three-pass structure; the
// structural deltas the asm carries are kept: the output cursor advances as a
// pointer, the multi-contact average performs the 1/count divide once per
// average, the feature copies are memcpy-shaped, and the orientation/fatten
// helpers are the two inlined blocks below).
// (X360 __fastcall: r3=results, r4=maxResults, r5=inst1, r6=instsN, r7=num,
//  f1=padding; returns the surviving pair count in r3.)
// ===========================================================================

namespace
{
    // ---------------------------------------------------------------------
    // SelectContactNormal -- the interval sanity check the 1xN body inlines
    // twice (0x82BAB728 single-point path, 0x82BAB9E0 multi-point path).
    // Projects both instances onto the candidate normal and either keeps it,
    // flips it, or falls back to the coarse separating direction:
    //   sep1 = i1.min - i2.max            (vsubfp on the broadcast rows)
    //   sep2 = i2.min - i1.max
    //   best = max(sep1, sep2)            (vcmpgtfp mask + vsel)
    //   if (distance > best)   normal = sepDir     (vcmpgtfp. all-lanes CR6)
    //   else if (sep1 > sep2)  normal = -normal    (vcmpeqfp. mask==0 failed
    //                                               -> vxor sign flip)
    // (Lane-identical inputs: the interval rows are broadcasts, so the lane-0
    // scalar compare equals the all-lanes test here.)
    // ---------------------------------------------------------------------
    void SelectContactNormal(const GPInstance& arInst1,
                             const GPInstance& arInstN,
                             f32 afDistance,
                             const Vec4& arSepDir,
                             Vec4& rNormal)
    {
        Interval lInterval1;
        Interval lInterval2;
        arInst1.mMethods.mGetInterval(&arInst1, rNormal, lInterval1);
        arInstN.mMethods.mGetInterval(&arInstN, rNormal, lInterval2);

        const f32 lfSep1 = lInterval1.min.x - lInterval2.max.x;
        const f32 lfSep2 = lInterval2.min.x - lInterval1.max.x;
        const f32 lfBestSep = (lfSep1 > lfSep2) ? lfSep1 : lfSep2;

        if (afDistance > lfBestSep)
        {
            rNormal = arSepDir;
        }
        else if (lfSep1 > lfSep2)
        {
            rNormal = Negate(rNormal);
        }
    }

    // ---------------------------------------------------------------------
    // FattenContactPoints -- the contact-point push-out the 1xN body inlines
    // twice (0x82BAB7F0 single-point path, 0x82BABABC multi-point path). Both
    // reference points and every clipped point pair are pushed apart along
    // the contact normal by the instances' fatness (lvlx+vspltw fatness
    // splats feeding vmaddfp / vmulfp128+vsubfp), each pair's penetration
    // along the normal is stored (vmsum3fp128), and the reference-pair
    // penetration is returned.
    // ---------------------------------------------------------------------
    f32 FattenContactPoints(PrimitivePairIntersectResult* lpResult,
                            const GPInstance& arInst1,
                            const GPInstance& arInstN,
                            const Vec4& arNormal,
                            const Vec4& arSeedOn1,
                            const Vec4& arSeedOn2)
    {
        const f32 lfFatness1 = arInst1.mFatness;
        const f32 lfFatnessN = arInstN.mFatness;

        // vmaddfp(normal, fat1, seed1) / vsubfp(seed2, vmulfp128(normal, fatN))
        lpResult->pointOn1 = MaddScalar(arNormal, lfFatness1, arSeedOn1);
        lpResult->pointOn2 = Sub(arSeedOn2, Scale(arNormal, lfFatnessN));

        // The asm reloads numPoints as the loop bound every pass; it is not
        // modified inside the loop, so a plain bound read is equivalent.
        for (u32 luPoint = 0; luPoint < lpResult->numPoints; ++luPoint)
        {
            lpResult->pointsOn1[luPoint] =
                MaddScalar(arNormal, lfFatness1, lpResult->pointsOn1[luPoint]);
            lpResult->pointsOn2[luPoint] =
                Sub(lpResult->pointsOn2[luPoint], Scale(arNormal, lfFatnessN));
            lpResult->distances[luPoint] =
                Dot3(Sub(lpResult->pointsOn2[luPoint], lpResult->pointsOn1[luPoint]),
                     arNormal);
        }

        return Dot3(Sub(lpResult->pointOn2, lpResult->pointOn1), arNormal);
    }
}

s32 GPInstanceBatchIntersect1xN(PrimitivePairIntersectResult* lapResults,
                                s32 aiResBufMaxSize,
                                const GPInstance& arInst1,
                                const GPInstance* lapInstsN, s32 aiNum,
                                f32 afPadding)
{
    s32 liNumCandidates = aiNum;
    if (liNumCandidates > aiResBufMaxSize)
    {
        liNumCandidates = aiResBufMaxSize;
    }

    s32 liNumResults = 0;

    // ---- pass 1 (0x82BAB504): coarse separating direction + distance -------
    // Dispatch through the [type1][typeN] table; the callee writes the
    // direction into the candidate slot (r3 = &slot.sepDir) and returns the
    // distance in v1, whose first lane lands in slot.distance
    // (stvx128 to a stack row + lfs/stfs of lane 0 -- +0x4F0, see the header).
    for (s32 liPair = 0; liPair < liNumCandidates; ++liPair)
    {
        PrimitivePairIntersectResult* lpCandidate = &lapResults[liPair];
        const GPInstance* lpInstanceN = &lapInstsN[liPair];

        const FindBestSeparatingDirectionFn lpfnSeparate =
            gapFindBestSeparatingDirection[arInst1.mVolumeType][lpInstanceN->mVolumeType];
        lpCandidate->distance = lpfnSeparate(lpCandidate->sepDir, arInst1, *lpInstanceN);
    }

    // ---- pass 2 (0x82BAB564): fatness cull + support features --------------
    for (s32 liPair = 0; liPair < liNumCandidates; ++liPair)
    {
        PrimitivePairIntersectResult* lpCandidate = &lapResults[liPair];
        const GPInstance* lpInstanceN = &lapInstsN[liPair];

        // fcmpu: separated further than both fatnesses plus the caller's pad.
        if (lpCandidate->distance > (lpInstanceN->mFatness + afPadding) + arInst1.mFatness)
        {
            lpCandidate->numPoints = 0;
            lpCandidate->vNindex   = -1;
        }
        else
        {
            lpCandidate->vNindex = liPair;
            // Support feature on instance 1 along +direction (r4 = 1) and on
            // instance N along -direction (r4 = 0; vspltisw(-1)+vslw+vxor
            // sign-flip of the direction register).
            arInst1.mMethods.mGetMaximumFeature(&arInst1, 1, lpCandidate->sepDir,
                                                lpCandidate->f1);
            const Vec4 lvNegDir = Negate(lpCandidate->sepDir);
            lpInstanceN->mMethods.mGetMaximumFeature(lpInstanceN, 0, lvNegDir,
                                                     lpCandidate->f2);
        }
    }

    // ---- pass 3 (0x82BAB620): prism clip, normal, push-out, compaction -----
    if (liNumCandidates > 0)
    {
        PrimitivePairIntersectResult* lpResult = lapResults;   // compacted output cursor

        for (s32 liPair = 0; liPair < liNumCandidates; ++liPair)
        {
            PrimitivePairIntersectResult* lpCandidate = &lapResults[liPair];
            const GPInstance* lpInstanceN = &lapInstsN[liPair];

            // The caller zeroes the "normal computed" flag every iteration
            // (stw r22, var_AC) before anything else runs.
            rwc_FeatureIntersectionPrism lPrism;
            lPrism.normalOverride = 0;

            if (lpCandidate->vNindex < 0)
            {
                continue;
            }
            if (!FindFeatureIntersectionPrism(lPrism, lpCandidate->f1, lpCandidate->f2,
                                              lpCandidate->sepDir))
            {
                continue;
            }
            if (liNumResults >= aiResBufMaxSize)
            {
                continue;   // capacity reached: keep scanning, keep nothing
            }

            // Stage the clipped points into the output slot (the r20=-0x100
            // twin-array copy loop).
            lpResult->numPoints = static_cast<u32>(lPrism.m_numpts);
            for (s32 liPoint = 0; liPoint < lPrism.m_numpts; ++liPoint)
            {
                lpResult->pointsOn2[liPoint] = lPrism.m_ptsOn2[liPoint];
                lpResult->pointsOn1[liPoint] = lPrism.m_ptsOn1[liPoint];
            }

            f32 lfDistance;
            if (lpResult->numPoints == 1)
            {
                // --- single contact (0x82BAB6A4): normal = normalize(p2-p1) -
                const Vec4 lvGap = Sub(lpResult->pointsOn2[0], lpResult->pointsOn1[0]); // vsubfp
                const f32 lfLenSq = Dot3(lvGap, lvGap);                 // vmsum3fp128
                lpResult->normal = lvGap;                               // raw gap staged first

                if (lfLenSq <= KF_DEGENERATE_GAP_EPSILON)               // flt_8218025C
                {
                    // Degenerate gap: keep the coarse separating direction.
                    lpResult->normal = lpCandidate->sepDir;
                }
                else
                {
                    // vrsqrtefp estimate + two vnmsubfp/vmaddfp Newton-Raphson
                    // refinements (est += 0.5*est*(1 - d*est*est), twice)
                    // == 1/sqrt(d) to full precision.
                    const f32 lfInvLen = 1.0f / std::sqrt(lfLenSq);
                    lpResult->normal = Scale(lvGap, lfInvLen);          // vmulfp128
                    SelectContactNormal(arInst1, *lpInstanceN, lpCandidate->distance,
                                        lpCandidate->sepDir, lpResult->normal);
                }

                lfDistance = FattenContactPoints(lpResult, arInst1, *lpInstanceN,
                                                 lpResult->normal,
                                                 lpResult->pointsOn1[0],
                                                 lpResult->pointsOn2[0]);
            }
            else
            {
                // --- multi contact (0x82BAB8DC): reference points = averages -
                Vec4 lvZero;                       // the asm stores four zero
                lvZero.x = 0.0f;                   // lanes (stfs flt_82001CC0
                lvZero.y = 0.0f;                   // x3 + stw 0)
                lvZero.z = 0.0f;
                lvZero.w = 0.0f;
                lpResult->pointOn1 = lvZero;
                lpResult->pointOn2 = lvZero;

                for (u32 luPoint = 0; luPoint < lpResult->numPoints; ++luPoint)  // vaddfp
                {
                    lpResult->pointOn1 = Add(lpResult->pointOn1, lpResult->pointsOn1[luPoint]);
                    lpResult->pointOn2 = Add(lpResult->pointOn2, lpResult->pointsOn2[luPoint]);
                }

                // 1/count via std+lfd+fcfid+frsp then fdivs (flt_82001C98 =
                // 1.0f numerator); this kernel reloads/reconverts the count
                // and re-divides once per average, exactly as the asm does.
                lpResult->pointOn1 = Scale(lpResult->pointOn1,
                    1.0f / static_cast<f32>(lpResult->numPoints));
                lpResult->pointOn2 = Scale(lpResult->pointOn2,
                    1.0f / static_cast<f32>(lpResult->numPoints));

                if (lPrism.normalOverride)
                {
                    lpResult->normal = lPrism.normal;
                    SelectContactNormal(arInst1, *lpInstanceN, lpCandidate->distance,
                                        lpCandidate->sepDir, lpResult->normal);
                }
                else
                {
                    lpResult->normal = lpCandidate->sepDir;
                }

                lfDistance = FattenContactPoints(lpResult, arInst1, *lpInstanceN,
                                                 lpResult->normal,
                                                 lpResult->pointOn1,
                                                 lpResult->pointOn2);
            }

            // --- LABEL_43 (0x82BABB9C): finalize / reject / compact ----------
            lpResult->distance = lfDistance;

            // One-sided triangle rejection (X360 sub_82BAA600): instance 1
            // against the normal, instance N against the flipped normal.
            if (arInst1.mVolumeType == GPInstance::TRIANGLE
                && !GPTriangleAcceptContactNormal(&arInst1, lpResult->normal))
            {
                lpResult->numPoints = 0;
            }
            if (lpInstanceN->mVolumeType == GPInstance::TRIANGLE
                && !GPTriangleAcceptContactNormal(lpInstanceN, Negate(lpResult->normal)))
            {
                lpResult->numPoints = 0;
            }

            if (lpResult->numPoints != 0)
            {
                lpResult->v1   = arInst1.mVolumeTag;
                lpResult->tag1 = arInst1.mUserTag;
                lpResult->v2   = lpInstanceN->mVolumeTag;
                lpResult->tag2 = lpInstanceN->mUserTag;
                lpResult->sepDir = lpCandidate->sepDir;
                // 0x240-byte block copies of the staged features into the
                // compacted slot (a self-copy when the slots coincide, exactly
                // as the console's memcpy performed; memmove keeps that
                // well-defined on x64).
                memmove(&lpResult->f1, &lpCandidate->f1, sizeof(Feature));
                memmove(&lpResult->f2, &lpCandidate->f2, sizeof(Feature));
                ++liNumResults;
                ++lpResult;
            }
        }
    }

    return liNumResults;
}

// ===========================================================================
// rw::collision::PrimitiveBatchIntersect @ 0x82BABC78
//
// Narrow-phase batch driver: walks the overlap-report buffer that
// VolumeVolumeQuery::GetPrimitiveBBoxOverlaps (0x82BB3AB0) staged, instances
// each report group's volumes into the caller's GPInstance scratch buffer via
// the collision-volume vtable, and hands every group to the matching batch
// kernel. Sole caller: VolumeVolumeQuery::GetPrimitiveIntersections
// (0x82BB3FF0, currently declaration-only in VolumeQuery.hpp).
//
// VMX NOTE (dedicated VMX pass): despite living in the hand-vectorised
// narrow-phase TU group, this driver itself contains NO vector instructions.
// The only FP ops are `fmr f31, f1` / `fmr f1, f31` -- a bit-preserving save/
// restore of the incoming padding value across the virtual instancing calls so
// it can be re-passed in f1 to the batch callees. That is rendered as an
// ordinary f32 pass-through parameter.
// ===========================================================================

namespace
{
    // Collision-volume dispatch table, reached through the pointer at
    // Volume+0x40 (`lwz r11, 0x40(r3)`), inside the opaque pad of the
    // committed 128-byte Volume image (CollisionVolume.hpp). Slot INDICES (word slots on
    // X360, pointer slots here) are the preserved layout:
    //   slot 1 (+0x04)  GetBBox            (VolumeVolumeQuery @ 0x82BB3AB0)
    //   slot 5 (+0x14)  CreateGPInstance   (this driver)
    // Unreferenced slots stay untyped. (TU-local view; the full Volume::VTable
    // shape -- DWARF volume.h:1507 -- lands with the Volume TU.)
    typedef void (*CreateGPInstanceFn)(const Volume* lpVolume,
                                       GPInstance*   lpInstance,
                                       const void*   lpTransform);

    struct VolumeVTable
    {
        void*              mpSlot00;           // slot 0 (unreferenced here)
        void*              mpGetBBox;          // slot 1
        void*              mpSlot08;           // slot 2
        void*              mpSlot0C;           // slot 3
        void*              mpSlot10;           // slot 4
        CreateGPInstanceFn mpCreateGPInstance; // slot 5
    };

    // The 0x82BABC78 asm attests the two leading VolRef words at this
    // consumer: +0x00 -> the Volume* the vtable dispatch runs on
    // (lwz r3, 0(rRef)); +0x04 -> the cached transform pointer passed in r5
    // (lwz r5, 4(rRef)). Both are HOST-width in VolRef since waveQ5 C1 (they
    // are pointers, not console values -- see the VolRef.hpp banner), so these
    // views are plain reinterpretations: what AddPrimitiveRef/AddVolumeRef
    // stored is exactly what CreateGPInstance dereferences here.
    const Volume* VolRefVolume(const VolRef& lrRef)
    {
        return reinterpret_cast<const Volume*>(lrRef.muVolumePtr);
    }

    const void* VolRefTransform(const VolRef& lrRef)
    {
        return reinterpret_cast<const void*>(lrRef.muTransformPtr);
    }

    // The dispatch-table pointer lives at X360 byte +0x40 of the volume image
    // (`lwz r11, 0x40(r3)`). On the host that slot holds the type ENUM and the
    // descriptor is gVolumeVTable[enum] (CollisionVolume.hpp GetVolumeDescriptor;
    // wave Q5 integration 2026-08-18) -- reinterpreted into this TU's slot view.
    const VolumeVTable* GetVolumeVTable(const Volume* lpVolume)
    {
        return reinterpret_cast<const VolumeVTable*>(GetVolumeDescriptor(lpVolume));
    }
}

s32 PrimitiveBatchIntersect(PrimitivePairIntersectResult* lapResults,
                            s32 aiResBufMaxSize,
                            GPInstance* lapInstancingBuffer,
                            VolRef1xN* lapPairs, s32 aiNumPairs,
                            f32 afPadding)
{
    s32 liNumIntersections = 0;                                     // r30

    if (aiNumPairs > 0)                                             // cmpwi/ble (signed)
    {
        VolRef1xN* lpGroup           = lapPairs;                    // r31
        s32        liGroupsRemaining = aiNumPairs;                  // r23
        do
        {
            if (liNumIntersections < aiResBufMaxSize)               // cmpw/bge (signed)
            {
                // ---- instance the "1"-side volume into slot 0 ----
                const VolRef* lpRef1    = lpGroup->vRef1;
                const Volume* lpVolume1 = VolRefVolume(*lpRef1);
                GetVolumeVTable(lpVolume1)->mpCreateGPInstance(
                    lpVolume1, &lapInstancingBuffer[0], VolRefTransform(*lpRef1));
                lapInstancingBuffer[0].mVolumeTag = lpRef1->muTag;  // stw r11, 0x88(r29)

                // ---- instance the N-side volumes into slots 1..N ----
                u32 luRefIndex = 0;                                 // r26
                if (lpGroup->vRefsNCount != 0)                      // cmplwi/ble (unsigned)
                {
                    GPInstance* lpInstanceN = &lapInstancingBuffer[1]; // r27 (0xC0 stride)
                    VolRef**    lppRefN     = &lpGroup->vRefsN[0];     // r28
                    do
                    {
                        const VolRef* lpRefN    = *lppRefN;
                        const Volume* lpVolumeN = VolRefVolume(*lpRefN);
                        GetVolumeVTable(lpVolumeN)->mpCreateGPInstance(
                            lpVolumeN, lpInstanceN, VolRefTransform(*lpRefN));
                        ++luRefIndex;                               // addi r26, r26, 1
                        lpInstanceN->mVolumeTag = lpRefN->muTag;    // stw r11, 0(r27)
                        ++lppRefN;                                  // addi r28, r28, 4
                        ++lpInstanceN;                              // addi r27, r27, 0xC0
                    }
                    while (luRefIndex < lpGroup->vRefsNCount);      // count reloaded per pass
                }

                // ---- narrow-phase batch on the staged instances ----
                const s32 liRemaining = aiResBufMaxSize - liNumIntersections; // subf
                PrimitivePairIntersectResult* lpOut =
                    lapResults + liNumIntersections;                // mulli 0x750 + add

                s32 liFound;
                if (lpGroup->volumesSwapped != 0)                   // cmplwi/beq on word 2
                {
                    liFound = GPInstanceBatchIntersectNx1(
                        lpOut, liRemaining,
                        &lapInstancingBuffer[1], static_cast<s32>(lpGroup->vRefsNCount),
                        lapInstancingBuffer[0], afPadding);
                }
                else
                {
                    liFound = GPInstanceBatchIntersect1xN(
                        lpOut, liRemaining,
                        lapInstancingBuffer[0],
                        &lapInstancingBuffer[1], static_cast<s32>(lpGroup->vRefsNCount),
                        afPadding);
                }
                liNumIntersections += liFound;                      // add r30, r3, r30
            }

            // Group advance runs even when the result buffer is full
            // (console: lpuGroup += lpuGroup[1] + 3).
            lpGroup = lpGroup->NextGroup();
            --liGroupsRemaining;                                    // addic.
        }
        while (liGroupsRemaining != 0);                             // bne
    }

    return liNumIntersections;                                      // mr r3, r30
}

// ===========================================================================
// rw::collision::ComputeContactPoints @ 0x82BABDA8
// Called by: ContactGeneratorJob::CollideGPInstances.
//
// Narrow-phase contact generation for one GP-instance pair:
//   1. dispatch off_82F91800[type1*6+type2] for the best separating direction
//      and separation distance;
//   2. early-out when distance > fatness1 + fatness2 + padding;
//   3. build each instance's maximal feature along +/-direction and intersect
//      the two feature prisms;
//   4. when the prisms meet in exactly one point pair, derive a fresh normal
//      from the (p2 - p1) delta if it is long enough;
//   5. with a valid derived normal, interval-test both instances along it and
//      adopt +/-normal as the contact normal when the pair is separated to at
//      least the dispatch distance along it;
//   6. re-validate one-sided-triangle instances against the final normal
//      (X360 sub_82BAA600);
//   7. emit tags, the NEGATED normal, the pair count, and the point pairs
//      pushed out along the normal by each instance's fatness.
//
// Returns the number of contact-point pairs written (0 on any rejection).
// The X360 body also parks a zero in a dead stack slot (stw r28, var_790,
// twice) that nothing reads back; no C++ equivalent is emitted.
// ===========================================================================
u32 ComputeContactPoints(const GPInstance& arGP1, const GPInstance& arGP2,
                         const f32& arPadding, GPInstance::ContactPoints& arResult)
{
    // --- 1. best separating direction via the 6x6 type dispatch -----------
    // (*(&off_82F91800[6 * gp1.type] + gp2.type))(&direction, gp1, gp2); the
    // distance comes back as a v1 lane broadcast (stvx128 to var_780, lfs
    // lane 0 -> f31).
    Vec4 lvDirection;
    const FindBestSeparatingDirectionFn lpfnFindSepDir =
        gapFindBestSeparatingDirection[arGP1.mVolumeType][arGP2.mVolumeType];
    const f32 lfDistance = lpfnFindSepDir(lvDirection, arGP1, arGP2);

    // --- 2. range early-out (fadds/fadds/fcmpu, bgt -> return 0) ----------
    // Scalar add ORDER preserved: (fatness2 + padding) + fatness1.
    if (lfDistance > ((arGP2.mFatness + arPadding) + arGP1.mFatness))
    {
        return 0;
    }

    // --- 3. per-instance maximal features, then the prism intersection ----
    // First instance: the +0xA4 callback with r4=1 and the direction in v1.
    // Second instance: r4=0 and the NEGATED direction (vxor sign-bit splat).
    Feature lFeature1;
    Feature lFeature2;
    arGP1.mMethods.mGetMaximumFeature(&arGP1, 1, lvDirection, lFeature1);
    arGP2.mMethods.mGetMaximumFeature(&arGP2, 0, Negate(lvDirection), lFeature2);

    rwc_FeatureIntersectionPrism lIntersection;
    lIntersection.normalOverride = 0;    // stw r28, var_4EC before the call
    if (!FindFeatureIntersectionPrism(lIntersection, lFeature1, lFeature2, lvDirection))
    {
        return 0;
    }

    // v127 = the direction, reloaded from var_770 only after the prism call.
    Vec4 lvNormal = lvDirection;

    // --- 4. single point pair: derive the normal from the point delta -----
    if (lIntersection.m_numpts == 1)
    {
        // delta = p2[0] - p1[0]                             (vsubfp, all lanes)
        const Vec4 lvDelta = Sub(lIntersection.m_ptsOn2[0], lIntersection.m_ptsOn1[0]);

        // lenSq = dot3(delta, delta)                        (vmsum3fp128)
        const f32 lfLenSq = Dot3(lvDelta, lvDelta);

        // length = lenSq * rsqrt(lenSq), i.e. sqrt(lenSq): vrsqrtefp estimate
        // + TWO Newton-Raphson refines (vmulfp128/vnmsubfp/vmaddfp x2, with
        // the vcfsx-built 1.0/0.5 splats), zero-guarded by vcmpeqfp/vsel when
        // lenSq == 0. Rendered as the mathematically equivalent scalar.
        const f32 lfInvLength = (lfLenSq == 0.0f) ? 0.0f : (1.0f / std::sqrt(lfLenSq));
        const f32 lfLength    = lfLenSq * lfInvLength;

        // fcmpu against flt_8218025C; ble skips the normal adoption.
        if (lfLength > KF_DEGENERATE_GAP_EPSILON)
        {
            // normal = delta * rsqrt(lenSq)                 (vmulfp128, all lanes)
            lIntersection.normalOverride = 1;
            lIntersection.normal = Scale(lvDelta, lfInvLength);
        }
    }

    // --- 5. interval test along the derived normal --------------------------
    if (lIntersection.normalOverride != 0)
    {
        // Both +0xA8 callbacks take the SAME (un-negated) derived normal in v1.
        Interval lInterval1;
        Interval lInterval2;
        arGP1.mMethods.mGetInterval(&arGP1, lIntersection.normal, lInterval1);
        arGP2.mMethods.mGetInterval(&arGP2, lIntersection.normal, lInterval2);

        // sepA = i1.min - i2.max / sepB = i2.min - i1.max   (vsubfp, all lanes)
        // mask = sepA > sepB per lane (vcmpgtfp); sel = max (vsel);
        // vcmpgefp. sel >= splat(distance) with the CR6 "all lanes" bit.
        const f32* laf1Min = &lInterval1.min.x;
        const f32* laf1Max = &lInterval1.max.x;
        const f32* laf2Min = &lInterval2.min.x;
        const f32* laf2Max = &lInterval2.max.x;

        bool lbSeparatedToDistance = true;
        bool lbNoneGreater         = true;
        for (int liLane = 0; liLane < 4; ++liLane)
        {
            const f32  lfSepA    = laf1Min[liLane] - laf2Max[liLane];
            const f32  lfSepB    = laf2Min[liLane] - laf1Max[liLane];
            const bool lbGreater = lfSepA > lfSepB;
            const f32  lfSel     = lbGreater ? lfSepA : lfSepB;
            lbSeparatedToDistance = lbSeparatedToDistance && (lfSel >= lfDistance);
            lbNoneGreater         = lbNoneGreater && !lbGreater;
        }

        if (lbSeparatedToDistance)
        {
            if (lbNoneGreater)
            {
                // vcmpeqfp. mask == 0 (all lanes): no lane had sepA > sepB.
                lvNormal = lIntersection.normal;             // lvx128 v127, var_500
            }
            else
            {
                lvNormal = Negate(lIntersection.normal);     // vxor128 sign flip
            }
        }
    }

    // --- 6. one-sided-triangle re-validation --------------------------------
    if (arGP1.mVolumeType == GPInstance::TRIANGLE)
    {
        // sub_82BAA600(gp1) with v1 = +normal.
        if (!GPTriangleAcceptContactNormal(&arGP1, lvNormal))
        {
            return 0;
        }
    }
    if (arGP2.mVolumeType == GPInstance::TRIANGLE)
    {
        // sub_82BAA600(gp2) with v1 = -normal (vxor sign flip).
        if (!GPTriangleAcceptContactNormal(&arGP2, Negate(lvNormal)))
        {
            return 0;
        }
    }

    // --- 7. emit the result --------------------------------------------------
    arResult.volumeTag1 = arGP1.mVolumeTag;      // stw 0x84(r31) -> 0x00(r29)
    arResult.volumeTag2 = arGP2.mVolumeTag;      // stw 0x84(r30) -> 0x04(r29)
    arResult.userTag1   = arGP1.mUserTag;        // stw 0x88(r31) -> 0x08(r29)
    arResult.userTag2   = arGP2.mUserTag;        // stw 0x88(r30) -> 0x0C(r29)
    arResult.normal     = Negate(lvNormal);      // stvx128 -normal -> 0x20(r29)
    arResult.numPoints  = static_cast<u32>(lIntersection.m_numpts);   // stw -> 0x10(r29)

    // Fatness push-out vectors: lvlx/vspltw broadcast each instance's fatness,
    // then vmulfp128 by the (un-negated) final normal -- all four lanes.
    const Vec4 lvOffset1 = Scale(lvNormal, arGP1.mFatness);
    const Vec4 lvOffset2 = Scale(lvNormal, arGP2.mFatness);

    // Pair loop (skipped entirely when the count is 0; the bound is re-read
    // from the result block each iteration, as the asm's lwz 0x10(r29) does).
    for (u32 luPoint = 0; luPoint < arResult.numPoints; ++luPoint)
    {
        GPInstance::ContactPoints::PointPair& lrPair = arResult.pointPairs[luPoint];

        // Store order per iteration: p2 (+0x40 + 0x20*i) first, then
        // p1 (+0x30 + 0x20*i).
        lrPair.p2 = Sub(lIntersection.m_ptsOn2[luPoint], lvOffset2);
        lrPair.p1 = Add(lIntersection.m_ptsOn1[luPoint], lvOffset1);
    }

    // lwz r3, 0x10(r29)
    return arResult.numPoints;
}

// ===========================================================================
// rw::collision::PrimitivePairIntersect @ 0x82BAC130   (wave 2)
// Called by: BrnPhysics::Vehicle::VehicleManager::PredictCarCarIntersection,
//            CgsSceneManager::OverlapCullingModule::DoPairQuery,
//            CgsSceneManager::OverlapCullingModule::IsInsideEscapeVolume.
//
// The single-pair narrow-phase entry point (canonical rwccore.h:3001):
//   1. gate: both volumes must have flags bit 0 set (+0x5C, clrlwi. 31);
//   2. instance both volumes through vtable slot 5 (CreateGPInstance, with
//      the caller's transform pointers riding through r5/r7);
//   3. direction: caller-supplied (xyz + distance in w) or the 6x6 SAT
//      dispatch off the two instance types;
//   4. range cull: distance > (fatness2 + fatness1) + padding -> 0
//      (VOLUME fatness words @ +0x50, not the GPInstance copies);
//   5. publish sepDist (+0x4B0!) + sepDir (+0x4A0), build both maximal
//      features (+dir ccw=1 / -dir ccw=0), run FindFeatureIntersectionPrism;
//      the two 0x240 Feature blocks are memcpy'd into the result on BOTH the
//      hit and the miss path;
//   6. contact normal: single point pair -> normalised (p2-p1) delta with
//      the interval orientation test above (all-separated -> fall back to
//      the coarse direction, any-flip -> negate); multi point -> the prism
//      override normal through the same test, or the coarse direction;
//   7. emit header words (v1/v2 = the console Volume pointer words, tags =
//      0), numPoints, the copied point lists, the (single or averaged)
//      reference points, fatness push-out on every point, per-point +
//      reference separations along the normal;
//   8. one-sided triangle gates (GPTriangleAcceptContactNormal with +normal
//      on side 1 / -normal on side 2) -- a reject here returns 0 AFTER the
//      result block has been fully written (preserved).
//
// DELTA vs the batch kernels (both attested here): sepDist (+0x4B0) IS
// written by this function (stfs f31, 0x4B0(r31) -- the kernels leave it
// untouched), and v1/v2 (+0x00/+0x08) receive the raw Volume pointer words
// with zeroed tag words (the kernels store mVolumeTag/mUserTag instead). See
// the pairing note in GPInstance.hpp.
//
// rodata: flt_8218025C == KF_DEGENERATE_GAP_EPSILON; flt_82001CC0 = 0.0f;
// flt_82001C98 = 1.0f; off_82F91800 == gapFindBestSeparatingDirection.
// unk_82CDA350 is a 16-byte vperm control whose bytes are NOT dumped --
// FLAG (triangulated, not fabricated): every attested consumer uses it as a
// SAME-LANE blend/gather control (the 0x82B57DE0 CreateFromBox row assembly,
// the 0x8291AE64 SatNav-family merge), and here BOTH vperm operands are the
// same register (vperm v1, v0, v0, v7) with lane 2 re-inserted from v0 by
// vrlimi128 v1,v0,2,0 -- under the attested same-lane semantics the sequence
// is a verbatim copy of *apSepDir (the compiler's generic three-source
// Vector3 lane-gather with all three sources equal). Only the w byte-source
// ordering is unattested, and w of a direction row is inert downstream.
// ===========================================================================

namespace
{
    // --- TU-local Volume field views (same raw-offset convention as
    //     GetVolumeVTable above; promote to members when the full Volume
    //     layout lands) ------------------------------------------------------

    // Volume +0x5C: the flags word (DWARF volume.h Volume::m_flags). Bit 0 is
    // the enabled gate this function tests (lwz 0x5C / clrlwi. r11, r11, 31).
    inline u32 VolumeFlags(const Volume* lpVolume)
    {
        return *reinterpret_cast<const u32*>(
            reinterpret_cast<const u8*>(lpVolume) + 0x5C);
    }

    // Volume +0x50: the volume's fatness/radius word (lfs 0x50(r28)/0x50(r27);
    // the same word the batch path mirrors into GPInstance::mFatness).
    inline f32 VolumeFatness(const Volume* lpVolume)
    {
        return *reinterpret_cast<const f32*>(
            reinterpret_cast<const u8*>(lpVolume) + 0x50);
    }

    // Console pointer image of a Volume* for the result header words
    // (stw r28/r27 into +0x00/+0x08).
    //
    // FLAG (64->32 TRUNCATION, deliberate, UNRESOLVED): unlike VolRef -- whose
    // two leading pointer words were promoted to host width in waveQ5 C1
    // because the console left +0x08..+0x0F unwritten -- PrimitivePairIntersect-
    // Result has NO hole to grow into: v1/tag1/v2/tag2/vNindex are five packed
    // words at +0x00..+0x13 inside a record whose 0x750 stride is pinned by the
    // batch kernels' `mulli rN, 0x750`, and the SAME two slots are written by
    // the batch kernels with a genuine 32-bit GPInstance::mVolumeTag (see
    // :447/:449 and :743/:745). So the field is a tag-or-pointer union that
    // cannot be widened without relaying the record out from under both
    // producers. The truncation is currently INERT -- no consumer anywhere in
    // the tree reads v1/v2 back as a pointer (grepped) -- but a future consumer
    // that does will get a wild pointer on x64. Fix belongs with a PPIR
    // relayout, not here.
    inline u32 VolumePointerImage(const Volume* lpVolume)
    {
        return static_cast<u32>(reinterpret_cast<uintptr_t>(lpVolume));
    }
}

RwBool PrimitivePairIntersect(PrimitivePairIntersectResult& arResult,   // r3 (r31)
                              const Volume* apVolume1,                  // r4 (r28)
                              const void*   apMtx1,                     // r5 (rides through)
                              const Volume* apVolume2,                  // r6 (r27)
                              const void*   apMtx2,                     // r7 (r29)
                              f32           afPadding,                  // f1 (f30)
                              const Vec4*   apSepDir)                   // r9 (r30)
{
    // ---- 1. enabled gates (lwz 0x5C / clrlwi. / beq -> return 0) -----------
    if ((VolumeFlags(apVolume1) & 1u) == 0)
    {
        return 0;
    }
    if ((VolumeFlags(apVolume2) & 1u) == 0)
    {
        return 0;
    }

    // ---- 2. instance both volumes (vtable slot 5, +0x14) --------------------
    // First call: r5 = the UNTOUCHED incoming mtx1; second call: r5 = r29.
    GPInstance lInst1;                                  // var_880
    GPInstance lInst2;                                  // var_7C0
    GetVolumeVTable(apVolume1)->mpCreateGPInstance(apVolume1, &lInst1, apMtx1);
    GetVolumeVTable(apVolume2)->mpCreateGPInstance(apVolume2, &lInst2, apMtx2);

    // ---- 3. separating direction + distance ---------------------------------
    Vec4 lvDirection;                                   // var_950
    f32  lfDistance;                                    // f31
    if (apSepDir != nullptr)                            // cmplwi cr6, r30, 0
    {
        // lvx128 v0 = *apSepDir; lfs f31, 0xC(r30) = the w-lane distance.
        // vperm v1, v0, v0, [unk_82CDA350] + vrlimi128 v1, v0, 2, 0: the
        // generic three-source Vector3 lane-gather with all sources the same
        // register == verbatim copy (see the rodata FLAG above).
        lvDirection = *apSepDir;
        lfDistance  = apSepDir->w;
    }
    else
    {
        // (*(&off_82F91800[6 * type1] + type2))(&dir, &inst1, &inst2); the
        // returned broadcast's lane 0 (stvx128 v1 -> var_970, lfs f31) is the
        // separating distance.
        const FindBestSeparatingDirectionFn lpfnFindSepDir =
            gapFindBestSeparatingDirection[lInst1.mVolumeType][lInst2.mVolumeType];
        lfDistance = lpfnFindSepDir(lvDirection, lInst1, lInst2);
    }

    // ---- 4. range cull (VOLUME fatness words, +0x50) -------------------------
    // Scalar add ORDER preserved: (fatness2 + fatness1) + padding
    // (lfs f0, 0x50(r27) / lfs f13, 0x50(r28) / fadds / fadds / fcmpu / bgt).
    const f32 lfFatness1 = VolumeFatness(apVolume1);    // r26 = r28 + 0x50
    const f32 lfFatness2 = VolumeFatness(apVolume2);    // r25 = r27 + 0x50
    if (lfDistance > ((lfFatness2 + lfFatness1) + afPadding))
    {
        return 0;
    }

    // ---- 5. publish the coarse pair state, features, prism ------------------
    arResult.sepDist = lfDistance;                      // stfs f31, 0x4B0(r31)
    arResult.sepDir  = lvDirection;                     // stvx128 v1, r31, 0x4A0

    // Side 1: maximal feature along +dir, ccw = 1 (vtable copy word +0xA4);
    // side 2: along -dir (vspltisw128/vslw128/vxor sign flip), ccw = 0.
    Feature lFeature1;                                  // var_4E0
    Feature lFeature2;                                  // var_2A0
    lInst1.mMethods.mGetMaximumFeature(&lInst1, 1, lvDirection, lFeature1);
    lInst2.mMethods.mGetMaximumFeature(&lInst2, 0, Negate(lvDirection), lFeature2);

    rwc_FeatureIntersectionPrism lPrism;                // var_700
    lPrism.normalOverride = 0;                          // stw r29, var_4EC
    const RwBool lbPrismFound =
        FindFeatureIntersectionPrism(lPrism, lFeature1, lFeature2, lvDirection);

    // Both 0x240 Feature blocks are copied into the result on BOTH outcomes
    // (the miss path at 0x82BAC2C8 runs the same two memcpys before return 0).
    std::memcpy(&arResult.f1, &lFeature1, sizeof(Feature));  // r31+0x20
    std::memcpy(&arResult.f2, &lFeature2, sizeof(Feature));  // r31+0x260
    if (!lbPrismFound)                                  // cmplwi r3, 0 / bne
    {
        return 0;
    }

    // ---- 6. contact normal (r30 = &arResult.normal, +0x4C0) -----------------
    bool lbUseCoarseDir = false;                        // -> LABEL_19/20

    if (lPrism.m_numpts == 1)                           // cmpwi cr6 (signed)
    {
        // ==== single point pair (0x82BAC31C) ================================
        // delta = pt2[0] - pt1[0]; stored to the normal slot immediately
        // (stvx128 v13, r0, r30), then length-tested against FLT_EPSILON.
        const Vec4 lvDelta = Sub(lPrism.m_ptsOn2[0], lPrism.m_ptsOn1[0]);
        arResult.normal = lvDelta;
        const f32 lfLenSq = Dot3(lvDelta, lvDelta);     // vmsum3fp128 -> var_970

        if (lfLenSq <= KF_DEGENERATE_GAP_EPSILON)       // fcmpu vs flt_8218025C / ble
        {
            lbUseCoarseDir = true;
        }
        else
        {
            // Normalise: vrsqrtefp + two Newton-Raphson steps (vspltisw/vcfsx
            // 1.0/0.5 splats; @ 0x82BAC354..0x82BAC39C), rendered exact.
            arResult.normal = Scale(lvDelta, 1.0f / std::sqrt(lfLenSq));

            // Support intervals of both instances along the candidate normal
            // (v1 = the normal on both +0xA8 calls; interval rows on the stack
            // at var_910/var_900 and var_8E0/var_8D0).
            Interval lInterval1;
            Interval lInterval2;
            lInst1.mMethods.mGetInterval(&lInst1, arResult.normal, lInterval1);
            lInst2.mMethods.mGetInterval(&lInst2, arResult.normal, lInterval2);

            // The shared orientation test (vsubfp/vcmpgtfp/vsel row math +
            // vcmpgtfp. / vcmpeqfp. CR6 all-lanes bits, distance splatted
            // from (f31, 0, 0, 0) lane 0).
            bool lbAllSeparated;
            bool lbAnyFlip;
            ClassifyIntervalSeparation(lInterval1, lInterval2, lfDistance,
                                       lbAllSeparated, lbAnyFlip);
            if (lbAllSeparated)                         // vcmpgtfp. all -> LABEL_19
            {
                lbUseCoarseDir = true;
            }
            else if (lbAnyFlip)                         // vcmpeqfp. all failed
            {
                arResult.normal = Negate(arResult.normal);   // vxor sign flip
            }
            // else: keep the normalised delta (bne skips the store).
        }
    }
    else if (lPrism.normalOverride != 0)                // lwz var_4EC / cmplwi
    {
        // ==== multi point pair with a prism override normal (0x82BAC464) ====
        arResult.normal = lPrism.normal;                // lvx128 var_500 -> +0x4C0

        Interval lInterval1;                            // var_940/var_930
        Interval lInterval2;                            // var_8B0/var_8A0
        lInst1.mMethods.mGetInterval(&lInst1, arResult.normal, lInterval1);
        lInst2.mMethods.mGetInterval(&lInst2, arResult.normal, lInterval2);

        bool lbAllSeparated;
        bool lbAnyFlip;
        ClassifyIntervalSeparation(lInterval1, lInterval2, lfDistance,
                                   lbAllSeparated, lbAnyFlip);
        if (lbAllSeparated)
        {
            lbUseCoarseDir = true;
        }
        else if (lbAnyFlip)
        {
            arResult.normal = Negate(arResult.normal);
        }
    }
    else
    {
        lbUseCoarseDir = true;                          // beq -> LABEL_19
    }

    if (lbUseCoarseDir)
    {
        // LABEL_19/20: lvx128 var_950 -> stvx128 +0x4C0.
        arResult.normal = lvDirection;
    }

    // ---- 7. emit the result block (LABEL_21, 0x82BAC504) ---------------------
    arResult.v1        = VolumePointerImage(apVolume1); // stw r28, 0x00(r31)
    arResult.tag1      = 0;                             // stw r29, 0x04(r31)
    arResult.v2        = VolumePointerImage(apVolume2); // stw r27, 0x08(r31)
    arResult.tag2      = 0;                             // stw r29, 0x0C(r31)
    arResult.numPoints = static_cast<u32>(lPrism.m_numpts);   // stw -> +0x740

    // Point copy (skipped when the count is 0; the bound is re-read from
    // +0x740 each pass, unchanged inside the loop). Store order per pass:
    // pointsOn2[i] (+0x600) first, then pointsOn1[i] (+0x500, via r5 = -0x100).
    for (u32 luPoint = 0; luPoint < arResult.numPoints; ++luPoint)
    {
        arResult.pointsOn2[luPoint] = lPrism.m_ptsOn2[luPoint];
        arResult.pointsOn1[luPoint] = lPrism.m_ptsOn1[luPoint];
    }

    if (arResult.numPoints == 1)                        // lwz r6, 0x740 / cmplwi 1
    {
        // Reference points straight from the copied slot-0 pair (loads from
        // the RESULT arrays, r31+0x500 / r31+0x600).
        arResult.pointOn1 = arResult.pointsOn1[0];      // -> +0x4D0
        arResult.pointOn2 = arResult.pointsOn2[0];      // -> +0x4E0
    }
    else
    {
        // Zero-seeded averages (flt_82001CC0 x3 + stw 0 stack rows stored to
        // both reference slots, then accumulated in place).
        Vec4 lvZero;
        lvZero.x = 0.0f;
        lvZero.y = 0.0f;
        lvZero.z = 0.0f;
        lvZero.w = 0.0f;
        arResult.pointOn1 = lvZero;
        arResult.pointOn2 = lvZero;
        for (u32 luPoint = 0; luPoint < arResult.numPoints; ++luPoint)
        {
            arResult.pointOn1 = Add(arResult.pointOn1, arResult.pointsOn1[luPoint]);
            arResult.pointOn2 = Add(arResult.pointOn2, arResult.pointsOn2[luPoint]);
        }
        // 1/count: std/lfd/fcfid (exact s64->f64) + frsp + fdivs of the 1.0f
        // constant (flt_82001C98), lvlx+vspltw splatted. The asm computes the
        // identical quotient TWICE (0x82BAC62C and 0x82BAC664, same +0x740
        // count both times); folded to one computation of the same value.
        const f32 lfInvCount = 1.0f / static_cast<f32>(arResult.numPoints);
        arResult.pointOn1 = Scale(arResult.pointOn1, lfInvCount);
        arResult.pointOn2 = Scale(arResult.pointOn2, lfInvCount);
    }

    // Fatness push-out along the final normal (lvlx+vspltw splats of the
    // VOLUME fatness words r26/r25; the normal is reloaded from +0x4C0 --
    // value unchanged). vmaddfp v0, v0, v12, v13 == normal*fat1 + point
    // (multiplier = LAST operand); side 2 is vmulfp128 + vsubfp.
    const Vec4 lvNormal = arResult.normal;
    arResult.pointOn1 = MaddScalar(lvNormal, lfFatness1, arResult.pointOn1);
    arResult.pointOn2 = Sub(arResult.pointOn2, Scale(lvNormal, lfFatness2));

    // Per-point push-out + separation (guarded by numPoints; the count is
    // re-read from +0x740 each pass). Per pass: pointsOn1[i] fattened first
    // (r8 = r11 - 0x100), then pointsOn2[i], then
    // distances[i] (+0x700) = dot3(pointsOn2[i]' - pointsOn1[i]', normal).
    for (u32 luPoint = 0; luPoint < arResult.numPoints; ++luPoint)
    {
        arResult.pointsOn1[luPoint] =
            MaddScalar(lvNormal, lfFatness1, arResult.pointsOn1[luPoint]);
        arResult.pointsOn2[luPoint] =
            Sub(arResult.pointsOn2[luPoint], Scale(lvNormal, lfFatness2));
        arResult.distances[luPoint] =
            Dot3(Sub(arResult.pointsOn2[luPoint], arResult.pointsOn1[luPoint]),
                 lvNormal);
    }

    // Reference separation (vsubfp/vmsum3fp128 -> stfs +0x4F0).
    arResult.distance = Dot3(Sub(arResult.pointOn2, arResult.pointOn1), lvNormal);

    // ---- 8. one-sided triangle gates (AFTER the block is fully written) -----
    // Side 1: +normal (v1 = lvx128 +0x4C0); side 2: -normal (vxor sign flip).
    // A reject returns 0 but leaves every store above in place (preserved).
    if (lInst1.mVolumeType == GPInstance::TRIANGLE &&
        !GPTriangleAcceptContactNormal(&lInst1, lvNormal))   // bl sub_82BAA600
    {
        return 0;
    }
    if (lInst2.mVolumeType == GPInstance::TRIANGLE &&
        !GPTriangleAcceptContactNormal(&lInst2, Negate(lvNormal)))
    {
        return 0;
    }

    return 1;                                           // li r3, 1
}

} // namespace collision
} // namespace rw

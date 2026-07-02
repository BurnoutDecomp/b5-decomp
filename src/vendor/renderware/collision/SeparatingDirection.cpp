#include "vendor/renderware/collision/GPInstance.hpp"

#include <cmath>   // sqrt, fabs

// ===========================================================================
// rw::collision separating-axis (SAT) family -- reconstructed from
// BURNOUT_X360_ARTIST.XEX (dedicated VMX pass; the hand-vectorised bodies are
// lowered to portable per-lane scalar maths per the committed Feature /
// FeatureEdge precedent, preserving branch polarity, store order and every
// caller-visible store).
//
//   rw::collision::FindBestSepDir_FindCandidates       @ 0x82BB4038
//   rw::collision::FindBestSeparatingDirection         @ 0x82BB4660
//   rw::collision::FindBestSeparatingDirectionBoxBox   @ 0x82BB47F0
//   rw::collision::FindBestSeparatingDirectionBoxTri   @ 0x82BAA1A0
//   rw::collision::FindBestSeparatingDirectionSphSph   @ 0x82BAA1F0
//   rw::collision::FindBestSepDirWithCylinder          @ 0x82BB76E8
//   rw::collision::FindBestSeparatingDirCylVol         @ 0x82BAA250
//   rw::collision::AddRimToEdgeCandidate               @ 0x82BB6118
//   rw::collision::AddRimToRimCandidates               @ 0x82BB61C0
//
// Canonical declarations: Feb-2007 rwccore.h:3144-3166 (generic/box/tri/sph)
// and :3830-3845 (cylinder). Each entry writes the best separating direction
// through the out-reference and returns the signed separation along it
// (VecFloat broadcast in v1 on console; the scalar lane here -- Hex-Rays's
// "int in r3" on these is the usual vector-return artifact).
// ===========================================================================

namespace rw
{
namespace collision
{

namespace
{
    // dot3 of the xyz lanes (the asm's vmsum3fp128; broadcast lane 0).
    inline f32 Dot3(const Vec4& a, const Vec4& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    // vsubfp: per-lane a - b.
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

    // vmulfp128 against a splat: per-lane a * s.
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

    // Cross product a x b via the VMX two-permute idiom (vpermwi128 0x63 +
    // vmulfp128 + vnmsubfp + vpermwi128); the w lane cancels to 0.
    inline Vec4 Cross(const Vec4& a, const Vec4& b)
    {
        Vec4 r;
        r.x = a.y * b.z - a.z * b.y;
        r.y = a.z * b.x - a.x * b.z;
        r.z = a.x * b.y - a.y * b.x;
        r.w = a.w * b.w - a.w * b.w;   // == 0 (carried for fidelity)
        return r;
    }

    // Zero-guarded vector length: lenSq * rsqrt(lenSq) with the vcmpeqfp/vsel
    // zero mask (== the committed rw::math::vpu::Magnitude lowering).
    inline f32 Magnitude(const Vec4& v)
    {
        const f32 lfLenSq = Dot3(v, v);
        return (lfLenSq == 0.0f) ? 0.0f : lfLenSq * (1.0f / std::sqrt(lfLenSq));
    }
}

// ===========================================================================
// rw::collision::FindBestSepDir_FindCandidates @ 0x82BB4038
//
// Collects the SAT candidate directions for a primitive pair:
//   1. both instances' face normals (copied highest slot first, switch
//      fallthrough),
//   2. the normalised cross product of every edge-direction pair,
//   3. edge-vs-edge (both single-edge, e.g. capsule/capsule): the two
//      directions perpendicular to each edge inside the edge/edge cross plane,
//   4. if still empty: the centre-delta rejected against the single edge
//      (both-single-edge case, then the exactly-one-edge case),
//   5. if still empty: the raw centre delta.
// Returns the end pointer of the filled candidate array, or NULL if no
// candidate survived the degeneracy filter.
//
// The console stores each raw candidate into the output slot BEFORE the
// degeneracy test and only bumps the write pointer after overwriting it with
// the normalised vector; those unconditional stores are preserved (the
// caller's buffer has slack). rodata: flt_8218081C = 1.1920929e-07f
// (FLT_EPSILON), from the dossier compare literal.
// ===========================================================================

// flt_8218081C: the degeneracy threshold every candidate length (or squared
// length, for the edge-pair loop) must exceed. == FLT_EPSILON.
static const f32 KF_SEP_DIR_EPSILON = 1.1920929e-07f;

Vec4* FindBestSepDir_FindCandidates(Vec4* lapCandidates,
                                    const GPInstance& arGP1, const GPInstance& arGP2)
{
    Vec4* lpOut = lapCandidates;

    // ---- 1. Face normals, copied straight in (highest slot first, fallthrough).
    switch (arGP1.mNumFaceNormals)
    {
        case 3: *lpOut++ = arGP1.mFaceNormals[2];  // lvx128 [r4+0x30]
        case 2: *lpOut++ = arGP1.mFaceNormals[1];  // lvx128 [r4+0x20]
        case 1: *lpOut++ = arGP1.mFaceNormals[0];  // lvx128 [r4+0x10]
        default: break;
    }
    switch (arGP2.mNumFaceNormals)
    {
        case 3: *lpOut++ = arGP2.mFaceNormals[2];  // lvx128 [r5+0x30]
        case 2: *lpOut++ = arGP2.mFaceNormals[1];  // lvx128 [r5+0x20]
        case 1: *lpOut++ = arGP2.mFaceNormals[0];  // lvx128 [r5+0x10]
        default: break;
    }

    // ---- 2. Normalised cross product of every edge-direction pair.
    // Filtered on SQUARED length here (the later sections filter on length).
    for (s32 li1 = 0; li1 < static_cast<s32>(arGP1.mNumEdgeDirections); ++li1)
    {
        for (s32 li2 = 0; li2 < static_cast<s32>(arGP2.mNumEdgeDirections); ++li2)
        {
            // vpermwi128 0x63 / vmulfp128 / vnmsubfp / vpermwi128 cross recipe.
            const Vec4 lvCross = Cross(arGP1.mEdgeDirections[li1],
                                       arGP2.mEdgeDirections[li2]);

            *lpOut = lvCross;                        // raw store before the test
            const f32 lfLenSq = Dot3(lvCross, lvCross);   // vmsum3fp128
            if (lfLenSq > KF_SEP_DIR_EPSILON)
            {
                // vrsqrtefp + two Newton-Raphson refine steps, rendered exact.
                *lpOut = Scale(lvCross, 1.0f / std::sqrt(lfLenSq));
                ++lpOut;
            }
        }
    }

    // ---- 3. Both single-edge (capsule-like vs capsule-like): add the directions
    // perpendicular to each edge within the edge/edge cross plane.
    if (arGP1.mNumEdgeDirections == 1 && arGP2.mNumEdgeDirections == 1)
    {
        const Vec4& lrEdge1 = arGP1.mEdgeDirections[0];   // lvx128 [r4+0x40]
        const Vec4& lrEdge2 = arGP2.mEdgeDirections[0];   // lvx128 [r5+0x40]

        const Vec4 lvCross = Cross(lrEdge1, lrEdge2);
        // Zero-guarded length: lenSq * rsqrt(lenSq) forced to 0 by vcmpeqfp/vsel
        // when lenSq == 0.
        const f32 lfCrossLen = Magnitude(lvCross);
        if (lfCrossLen > KF_SEP_DIR_EPSILON)
        {
            const Vec4 lvPerp1 = Cross(lrEdge1, lvCross);
            const Vec4 lvPerp2 = Cross(lrEdge2, lvCross);

            *lpOut = lvPerp1;                        // raw store before the test
            if (Magnitude(lvPerp1) > KF_SEP_DIR_EPSILON)
            {
                // vmsum3fp128 recomputed, then the unguarded rsqrt pipeline.
                *lpOut = Scale(lvPerp1, 1.0f / std::sqrt(Dot3(lvPerp1, lvPerp1)));
                ++lpOut;
            }

            *lpOut = lvPerp2;                        // raw store before the test
            if (Magnitude(lvPerp2) > KF_SEP_DIR_EPSILON)
            {
                *lpOut = Scale(lvPerp2, 1.0f / std::sqrt(Dot3(lvPerp2, lvPerp2)));
                ++lpOut;
            }
        }
    }

    if (lpOut != lapCandidates)
    {
        return lpOut;
    }

    // ---- 4a. Nothing yet, both single-edge: reject the centre delta against
    // gp1's edge, i.e. the component of (pos1 - pos2) perpendicular to it.
    if (arGP1.mNumEdgeDirections == 1 && arGP2.mNumEdgeDirections == 1)
    {
        const Vec4& lrEdge  = arGP1.mEdgeDirections[0];
        const Vec4  lvDelta = Sub(arGP1.mPos, arGP2.mPos);   // vsubfp
        const Vec4  lvPerp  = Cross(Cross(lvDelta, lrEdge), lrEdge);

        *lpOut = lvPerp;                             // raw store before the test
        if (Magnitude(lvPerp) > KF_SEP_DIR_EPSILON)
        {
            *lpOut = Scale(lvPerp, 1.0f / std::sqrt(Dot3(lvPerp, lvPerp)));
            ++lpOut;
        }
    }

    // ---- 4b. Exactly one edge between the two instances: same rejection
    // against that single edge. (When gp1 has none the console loads an
    // uninitialised stack quad first and unconditionally overwrites it with
    // gp2's edge -- with the counts summing to 1 that is exactly "the one edge
    // either instance owns".)
    if (arGP1.mNumEdgeDirections + arGP2.mNumEdgeDirections == 1)
    {
        const Vec4& lrEdge = (arGP1.mNumEdgeDirections != 0)
                                 ? arGP1.mEdgeDirections[0]
                                 : arGP2.mEdgeDirections[0];
        const Vec4 lvDelta = Sub(arGP1.mPos, arGP2.mPos);
        const Vec4 lvPerp  = Cross(Cross(lvDelta, lrEdge), lrEdge);

        *lpOut = lvPerp;                             // raw store before the test
        if (Magnitude(lvPerp) > KF_SEP_DIR_EPSILON)
        {
            *lpOut = Scale(lvPerp, 1.0f / std::sqrt(Dot3(lvPerp, lvPerp)));
            ++lpOut;
        }
    }

    if (lpOut != lapCandidates)
    {
        return lpOut;
    }

    // ---- 5. Last resort: the raw centre delta itself.
    {
        const Vec4 lvDelta = Sub(arGP1.mPos, arGP2.mPos);

        *lpOut = lvDelta;                            // raw store before the test
        if (Magnitude(lvDelta) > KF_SEP_DIR_EPSILON)
        {
            *lpOut = Scale(lvDelta, 1.0f / std::sqrt(Dot3(lvDelta, lvDelta)));
            ++lpOut;
        }
    }

    return (lpOut != lapCandidates) ? lpOut : 0;
}

// ===========================================================================
// rw::collision::FindBestSeparatingDirection @ 0x82BB4660
//
// Generic SAT driver:
//   1. FindBestSepDir_FindCandidates fills a 15-slot stack table
//      (sp+0x80..0x170, 16-byte stride, count = (end-begin)>>4).
//   2. If the collector returned NULL, seed a single +Z fallback candidate
//      ({0,0,1,0} built lane-by-lane on the stack).
//   3. Call each instance's batched projection-interval callback (the
//      GPInstance +0xAC VolumeMethods slot; one 0x30-stride Interval per
//      candidate).
//   4. Per candidate: distFlip = i1.min - i2.max, distForward = i2.min -
//      i1.max (vsubfp on the broadcast rows, lane X compared); keep the larger
//      (flag = 1 when distFlip wins), and the running strict maximum across
//      candidates (first iteration always taken via the r28==0 test).
//   5. Store the winning direction, sign-flipped when the winning lane was
//      distFlip, so the result points from gp1 toward gp2.
//   6. Return the best separation (negative = penetration depth); the console
//      splats it to v1 (stfs f31 / lvlx / vspltw).
//
// rodata: flt_82001CC0 = 0.0f, flt_82001C98 = 1.0f.
//
// Degenerate note (asm-faithful): the `beq cr6, loc_82BB47D0` at 0x82BB4720
// skips the scan when the candidate count is zero, then loads the best
// candidate from r28 == 0 -- a guaranteed NULL dereference on the console.
// The path is dead in practice: the collector never returns its own start
// pointer (it returns NULL instead), and the NULL fallback seeds exactly one
// candidate, so the count is always >= 1. The translation keeps the same
// shape (lpBest stays NULL and is dereferenced unguarded on that dead path).
// ===========================================================================
f32 FindBestSeparatingDirection(Vec4& arBestSepDir,
                                const GPInstance& arGP1, const GPInstance& arGP2)
{
    // sp+0x80: 15 x 16-byte candidate slots (0xF0 bytes -- exactly the
    // collector's 6-normal + 9-edge-cross worst case).
    Vec4 laCandidates[15];
    // sp+0x170 / sp+0x440: one 0x30-byte interval record per candidate.
    Interval laIntervals1[15];
    Interval laIntervals2[15];

    Vec4* lpEnd = FindBestSepDir_FindCandidates(laCandidates, arGP1, arGP2);
    if (lpEnd == 0)
    {
        // Fallback: a single +Z candidate. The console builds {0,0,1,0}
        // lane-by-lane on the stack (0.0f = flt_82001CC0, 1.0f = flt_82001C98,
        // W stored as integer 0) and lvx128/stvx128-copies it into slot 0.
        laCandidates[0].x = 0.0f;
        laCandidates[0].y = 0.0f;
        laCandidates[0].z = 1.0f;
        laCandidates[0].w = 0.0f;
        lpEnd = &laCandidates[1];
    }

    f32         lfBestDist      = 0.0f;                                    // f31
    const Vec4* lpBest          = 0;                                       // r28
    s32         liBestFlip      = 0;                                       // r27
    const s32   liNumCandidates = static_cast<s32>(lpEnd - laCandidates);  // srawi >>4

    // Batched projection-interval callbacks (VolumeMethods +0xAC of each
    // instance). Both run before the count test, exactly as on the console.
    arGP1.mMethods.mGetIntervals(&arGP1, laCandidates,
                                 static_cast<u32>(liNumCandidates), laIntervals1);
    arGP2.mMethods.mGetIntervals(&arGP2, laCandidates,
                                 static_cast<u32>(liNumCandidates), laIntervals2);

    for (s32 li = 0; li < liNumCandidates; ++li)
    {
        // vsubfp on the splatted interval quads (the console also spills both
        // difference quads to stack temporaries); lane X is what it compares.
        const f32 lfDistFlip    = laIntervals1[li].min.x - laIntervals2[li].max.x;
        const f32 lfDistForward = laIntervals2[li].min.x - laIntervals1[li].max.x;

        // fcmpu + ble: strict greater-than keeps the flipped lane.
        f32 lfDist;
        s32 liFlip;
        if (lfDistFlip > lfDistForward)
        {
            lfDist = lfDistFlip;
            liFlip = 1;
        }
        else
        {
            lfDist = lfDistForward;
            liFlip = 0;
        }

        // First candidate always wins (r28 == 0 test), then strict maximum.
        if (lpBest == 0 || lfDist > lfBestDist)
        {
            lpBest     = &laCandidates[li];
            lfBestDist = lfDist;
            liBestFlip = liFlip;
        }
    }

    // lpBest is always set on the live paths (see the degenerate note above).
    if (liBestFlip != 0)
    {
        // vspltisw v0,-1 / vslw v0,v0,v0 -> 0x80000000 per lane; vxor flips
        // every sign bit.
        arBestSepDir = Negate(*lpBest);
    }
    else
    {
        arBestSepDir = *lpBest;
    }

    // stfs f31 / lvlx / vspltw v1: the best distance splatted into the vector
    // return register.
    return lfBestDist;
}

// ===========================================================================
// rw::collision::FindBestSeparatingDirectionBoxBox @ 0x82BB47F0
//
// SAT search over the 15 box/box candidate axes (3 A faces, 3 B faces, 9
// normalised edge crosses; the last cross duplicated into a 16th pad slot so
// the counted loop covers 4x4 lanes). Keeps the axis with the strictly
// greatest interval separation, stores it oriented so it points from the box
// on the axis' positive side toward the other (sign = -1 when gp1 is that
// box), and returns the best separation (`vmr v1, v11` before the final
// store: the running best rides out in the vector return register).
//
// The edge-cross normalisation uses a RAW vrsqrtefp reciprocal-sqrt ESTIMATE
// (NO Newton-Raphson refine, NO degenerate guard): a zero-length cross
// (parallel edges) yields inf/NaN lanes on both the console and here, and the
// NaN separations then lose every vcmpgtfp/> comparison, so the degenerate
// axis is never selected. rodata: the unk_82CDA400/unk_82CDA3C0 vperm lane-
// merge controls only pack four per-axis vmsum3fp128 broadcasts into one
// register; in this scalar rendering the pack/reduce collapses to an in-order
// scan, so no table value is needed or fabricated.
// ===========================================================================

namespace
{
    // The per-axis SAT interval test shared by the pre-loop initialiser and
    // the loop lanes (identical instruction mix in both):
    //   dA = dot3(L, A.pos)  dB = dot3(L, B.pos)          (vmsum3fp128)
    //   rA = (|dot3(projA0,L)| + |dot3(projA1,L)|) + |dot3(projA2,L)|
    //   rB likewise                                        (vandc sign mask +
    //                                                       vaddfp, this order)
    //   sep1 = (dA - rA) - (dB + rB)     A entirely on the +L side of B
    //   sep2 = (dB - rB) - (dA + rA)     B entirely on the +L side of A
    //   sep  = sep1 > sep2 ? sep1 : sep2       (vcmpgtfp + vsel)
    //   sign = sep1 > sep2 ? -1.0 : +1.0       (vsel of vcfsx(+/-1) splats)
    inline f32 ComputeBoxAxisSeparation(const Vec4& arAxis,
                                        const Vec4& arPos1, const Vec4& arPos2,
                                        const Vec4* lapProj1, const Vec4* lapProj2,
                                        f32& rfSign)
    {
        const f32 lfDist1 = Dot3(arAxis, arPos1);
        const f32 lfDist2 = Dot3(arAxis, arPos2);

        const f32 lfRadius1 = (std::fabs(Dot3(lapProj1[0], arAxis))
                             + std::fabs(Dot3(lapProj1[1], arAxis)))
                             + std::fabs(Dot3(lapProj1[2], arAxis));
        const f32 lfRadius2 = (std::fabs(Dot3(lapProj2[0], arAxis))
                             + std::fabs(Dot3(lapProj2[1], arAxis)))
                             + std::fabs(Dot3(lapProj2[2], arAxis));

        const f32 lfSep1 = (lfDist1 - lfRadius1) - (lfDist2 + lfRadius2);
        const f32 lfSep2 = (lfDist2 - lfRadius2) - (lfDist1 + lfRadius1);

        if (lfSep1 > lfSep2)
        {
            rfSign = -1.0f;
            return lfSep1;
        }
        rfSign = 1.0f;
        return lfSep2;
    }
}

f32 FindBestSeparatingDirectionBoxBox(Vec4& arBestSepDir,
                                      const GPInstance& arGP1, const GPInstance& arGP2)
{
    // Projection rows: face normal i scaled by half-dimension lane i
    // (vspltw [inst+0x70] lane + vmulfp128 against [inst+0x10+0x10*i]).
    Vec4 laProj1[3];
    Vec4 laProj2[3];
    laProj1[0] = Scale(arGP1.mFaceNormals[0], arGP1.mDimensions.x);
    laProj1[1] = Scale(arGP1.mFaceNormals[1], arGP1.mDimensions.y);
    laProj1[2] = Scale(arGP1.mFaceNormals[2], arGP1.mDimensions.z);
    laProj2[0] = Scale(arGP2.mFaceNormals[0], arGP2.mDimensions.x);
    laProj2[1] = Scale(arGP2.mFaceNormals[1], arGP2.mDimensions.y);
    laProj2[2] = Scale(arGP2.mFaceNormals[2], arGP2.mDimensions.z);

    // The asm's 0x100-byte stack table of candidate axes, in its exact slot
    // order: A's face normals in REVERSE row order, B's face normals in
    // REVERSE row order, then the 9 normalised crosses A.e[i] x B.e[j] in
    // row-major (i,j) order, then slot 14 duplicated into slot 15 so the
    // counted loop covers 4x4 lanes.
    Vec4 laCandidates[16];
    laCandidates[0] = arGP1.mFaceNormals[2];   // sp+0x00
    laCandidates[1] = arGP1.mFaceNormals[1];   // sp+0x10
    laCandidates[2] = arGP1.mFaceNormals[0];   // sp+0x20
    laCandidates[3] = arGP2.mFaceNormals[2];   // sp+0x30
    laCandidates[4] = arGP2.mFaceNormals[1];   // sp+0x40
    laCandidates[5] = arGP2.mFaceNormals[0];   // sp+0x50
    for (s32 li = 0; li < 3; ++li)             // sp+0x60 .. sp+0xE0
    {
        for (s32 lj = 0; lj < 3; ++lj)
        {
            const Vec4 lvCross = Cross(arGP1.mEdgeDirections[li],
                                       arGP2.mEdgeDirections[lj]);
            // RAW vrsqrtefp estimate normalisation (see the header note).
            laCandidates[6 + 3 * li + lj] =
                Scale(lvCross, 1.0f / std::sqrt(Dot3(lvCross, lvCross)));
        }
    }
    laCandidates[15] = laCandidates[14];       // sp+0xF0 pad slot

    // Pre-loop initialiser: the running best starts as gp1's face normal 2
    // (vmr v26, v13 = [r4+0x30]) with its own separation/sign -- the loop
    // re-tests that axis as its first lane, and the strictly-greater compare
    // keeps the initialiser on the tie, exactly as vcmpgtfp does.
    f32  lfBestSign = 0.0f;
    f32  lfBestSep  = ComputeBoxAxisSeparation(laCandidates[0], arGP1.mPos, arGP2.mPos,
                                               laProj1, laProj2, lfBestSign);
    Vec4 lvBestAxis = laCandidates[0];

    // 4 iterations x 4 lanes; the vperm+vsldoi packing and the per-lane
    // vspltw/vcmpgtfp/vsel reduction walk the slots in ascending memory
    // order, so the pipeline collapses to one in-order scan with a strict >
    // replacement test.
    for (s32 lu = 0; lu < 16; ++lu)
    {
        f32 lfSign = 0.0f;
        const f32 lfSep = ComputeBoxAxisSeparation(laCandidates[lu], arGP1.mPos, arGP2.mPos,
                                                   laProj1, laProj2, lfSign);
        if (lfSep > lfBestSep)
        {
            lfBestSep  = lfSep;
            lfBestSign = lfSign;
            lvBestAxis = laCandidates[lu];
        }
    }

    // vmulfp128 v0, v26, v25 + stvx128 to r3: orient the winning axis by the
    // +/-1 sign and store all four lanes. The best separation rides out in v1
    // (vmr v1, v11 at 0x82BB4C94).
    arBestSepDir = Scale(lvBestAxis, lfBestSign);
    return lfBestSep;
}

// ===========================================================================
// rw::collision::FindBestSeparatingDirectionBoxTri @ 0x82BAA1A0
//
// The canonical inline (rwccore.h:3151) emitted out of line:
//   mr r11,r4 / mr r4,r5 / mr r5,r11    swap gp1/gp2 for the TriBox call
//   bl  FindBestSeparatingDirectionTriBox
//   vspltisw/vslw/vxor + lvx128/stvx128  flip the stored direction's sign
// The VecFloat return travels through untouched in v1.
// ===========================================================================
f32 FindBestSeparatingDirectionBoxTri(Vec4& arBestSepDir,
                                      const GPInstance& arGP1, const GPInstance& arGP2)
{
    const f32 lfSeparation = FindBestSeparatingDirectionTriBox(arBestSepDir, arGP2, arGP1);

    // bestSepDir = -bestSepDir (sign-bit flip of all four lanes).
    arBestSepDir = Negate(arBestSepDir);

    return lfSeparation;
}

// ===========================================================================
// rw::collision::FindBestSeparatingDirectionSphSph @ 0x82BAA1F0
//
// The canonical inline (rwccore.h:3158) emitted out of line: the normalised
// centre1->centre2 axis, returning the centre distance. The 23-instruction
// branch-free body is exactly the committed NormalizeReturnMagnitude lowering
// (vsubfp; vmsum3fp128; vrsqrtefp + two Newton-Raphson refines; vmulfp128;
// vcmpeqfp/vsel zero guard on the RETURNED magnitude only -- the stored
// direction is NOT guarded and holds NaN lanes for coincident centres,
// exactly as 0 * (1/sqrt(0) = inf) does here).
// ===========================================================================
f32 FindBestSeparatingDirectionSphSph(Vec4& arBestSepDir,
                                      const GPInstance& arGP1, const GPInstance& arGP2)
{
    // vsubfp: direction = centre2 - centre1 (the GP sphere image's mPos rows).
    const Vec4 lvDelta = Sub(arGP2.mPos, arGP1.mPos);

    const f32 lfLenSq  = Dot3(lvDelta, lvDelta);        // vmsum3fp128
    const f32 lfInvLen = 1.0f / std::sqrt(lfLenSq);     // vrsqrtefp + 2x NR

    // stvx128 [r3]: the direction store is unconditional (NaN lanes when the
    // centres coincide).
    arBestSepDir = Scale(lvDelta, lfInvLen);

    // vsel v1 = (lenSq == 0) ? 0 : lenSq * invLen  -- the guarded magnitude.
    return (lfLenSq == 0.0f) ? 0.0f : lfLenSq * lfInvLen;
}

// ===========================================================================
// rw::collision::FindBestSepDirWithCylinder @ 0x82BB76E8
//
// Driver for the cylinder SAT (canonical rwccore.h:3831). The body carries no
// vector arithmetic of its own:
//   addi r3,r1,0x50 / bl sub_82BB64C0     build the rim-aware candidate table
//                                         (18 x 16-byte slots) -- the builder
//                                         calls AddRimToEdgeCandidate /
//                                         AddRimToRimCandidates / RimToEdge
//   mr r5,r3 / bl sub_82BB75B0            evaluate the candidates and write
//                                         the winner through arBestSepDir
// Both helpers are unnamed statics of this TU, PENDING (bodies not delivered
// this wave); their register shapes are attested at the two call sites.
// ===========================================================================

// PENDING @ 0x82BB64C0 -- fills the caller's 18-slot candidate table
// (r3 = table; r4/r5 = the two instances, untouched). Returns r3, forwarded
// verbatim as the evaluator's third argument (candidate count or end cursor
// -- opaque until the body is reconstructed, hence intptr_t). FLAGGED name.
extern intptr_t FindBestSepDirWithCylinder_FindCandidates(
    Vec4* lapCandidates, const GPCylinder& arGPCylinder, const GPInstance& arGPOther);

// PENDING @ 0x82BB75B0 -- evaluates the candidate directions and writes the
// winner through arBestSepDir (r3 = out, r4 = table, r5 = builder result,
// r6/r7 = the instances); the separation returns in the vector register per
// the family contract (Hex-Rays's r3 int is the artifact). FLAGGED name.
extern f32 FindBestSepDirWithCylinder_Evaluate(
    Vec4& arBestSepDir, Vec4* lapCandidates, intptr_t aiCandidateResult,
    const GPCylinder& arGPCylinder, const GPInstance& arGPOther);

f32 FindBestSepDirWithCylinder(Vec4& arBestSepDir,
                               const GPCylinder& arGPCylinder, const GPInstance& arGPOther)
{
    // sp+0x50: 288 bytes = 18 candidate-direction slots x 16 bytes, 16-aligned.
    Vec4 laCandidates[18];

    // addi r3,r1,0x50 / bl sub_82BB64C0 (r4/r5 ride along untouched).
    const intptr_t liCandidateResult =
        FindBestSepDirWithCylinder_FindCandidates(laCandidates, arGPCylinder, arGPOther);

    // mr r5,r3 / bl sub_82BB75B0.
    return FindBestSepDirWithCylinder_Evaluate(arBestSepDir, laCandidates,
                                               liCandidateResult, arGPCylinder, arGPOther);
}

// ===========================================================================
// rw::collision::FindBestSeparatingDirCylVol @ 0x82BAA250
//
// The canonical inline (rwccore.h:3834) emitted out of line -- the entire
// body is `b rw__collision__FindBestSepDirWithCylinder`: a single-instruction
// tail-call thunk (r3/r4/r5 and the return pass through untouched). The
// un-swapped forward means the thunk's operand order (cylinder first) is the
// worker's native order; the VolCyl entry point is the swapping wrapper.
// ===========================================================================
f32 FindBestSeparatingDirCylVol(Vec4& arBestSepDir,
                                const GPInstance& arGPCylinder, const GPInstance& arGPOther)
{
    return FindBestSepDirWithCylinder(arBestSepDir,
                                      static_cast<const GPCylinder&>(arGPCylinder),
                                      arGPOther);
}

// ===========================================================================
// rw::collision::AddRimToEdgeCandidate @ 0x82BB6118
//
// Register/asm map:
//   v1..v0 = the five vector pointers' rows (r3..r7); f1/f2 = the two floats
//   (consuming the r8/r9 GPR slots, Xenon convention); r10 = candidate output
//   array base; arg_54 (param 9, first stack argument) = the count pointer.
//   fadds/fsubs f0 = f1 +/- f2 -> stfs to a zeroed stack pad -> lvx128 +
//   vspltw lane0 broadcast.
//   vmaddfp v4 = v0*splat(f1+f2) + v13    (edge end point A)
//   vmaddfp v5 = v0*splat(f1-f2) + v13    (edge end point B)
//   bl RimToEdge (vector args v1..v5, candidate direction returned in v1)
//   stvx128 v1, (count << 4), r8 ; increment and store the count.
// (r3 is never rewritten, so Hex-Rays' "int" return is a pass-through of the
// first pointer argument: the source function is void.)
// ===========================================================================
void AddRimToEdgeCandidate(const Vec4& arRimCentre, const Vec4& arRimAxis,
                           const Vec4& arRimRadius, const Vec4& arEdgePoint,
                           const Vec4& arEdgeDirection,
                           f32 afEdgeOffset, f32 afEdgeHalfLength,
                           Vec4* lapCandidateDirs, u32* lpuNumCandidateDirs)
{
    // fadds / fsubs, then stfs + lvx128 + vspltw lane-0 broadcasts.
    const f32 lfOffsetPlusHalf  = afEdgeOffset + afEdgeHalfLength;
    const f32 lfOffsetMinusHalf = afEdgeOffset - afEdgeHalfLength;

    // vmaddfp v4/v5 = direction * splat +/- base: the two edge end points at
    // point + direction*(offset +/- half-length).
    const Vec4 lvEdgePointA = MaddScalar(arEdgeDirection, lfOffsetPlusHalf, arEdgePoint);
    const Vec4 lvEdgePointB = MaddScalar(arEdgeDirection, lfOffsetMinusHalf, arEdgePoint);

    // bl rw__collision__RimToEdge: v1..v5 = centre/axis/radius/end points;
    // the candidate separating direction comes back in v1.
    const Vec4 lvCandidateDir =
        RimToEdge(arRimCentre, arRimAxis, arRimRadius, lvEdgePointA, lvEdgePointB);

    // stvx128 v1, (count << 4), r8 then ++count: append the candidate
    // (16-byte stride, unconditional -- unlike the RimToRim path, RimToEdge
    // always yields a candidate).
    lapCandidateDirs[*lpuNumCandidateDirs] = lvCandidateDir;
    ++(*lpuNumCandidateDirs);
}

// ===========================================================================
// rw::collision::AddRimToRimCandidates @ 0x82BB61C0
//
// Enumerates separating-direction candidates between the rim circles of two
// cylinder-style GP instances. When the two axes are not parallel it probes
// all four (+/-) rim-end pairings through RimToRim, appending one 16-byte
// candidate per hit and bumping the caller's running count. Called only by
// the FindBestSepDirWithCylinder candidate builder (sub_82BB64C0).
//
// Instance rows used (asm-attested): mPos @+0x00, the cylinder axis
// mEdgeDirections[0] @+0x40, and mDimensions @+0x70 with lane X = the axis
// half-extent (vspltw 0) and lane Y = the rim radius (vspltw 1).
//
// Separating-direction sign per pairing (from the asm's r9 slot selection):
//   (+A,+B) -> +sepDir   (-A,+B) -> -sepDir   (+A,-B) -> -sepDir
//   (-A,-B) -> +sepDir (the asm reuses the un-negated var_40 slot).
//
// Disassembly nit: the two `vxor128 v0, v95, v0` sign flips print register
// v95, which is never written; dataflow attests it is v63, the normalised
// cross (an IDA VMX128 extended-register decode glitch).
// Return type: Hex-Rays shows `int`, but the early-out path returns the
// untouched incoming r3 -- reconstructed as void (the caller consumes only
// the candidate array/count). A dead 4-byte stack zeroing (stw r10, var_70)
// is not modelled.
// ===========================================================================

// Parallel-axis rejection threshold: flt_82180894 = 1.1754944e-38f (FLT_MIN),
// value present in the export's pseudocode.
static const f32 KF_RIM_PARALLEL_EPSILON = 1.1754944e-38f;

void AddRimToRimCandidates(const GPInstance* lpGP1, const GPInstance* lpGP2,
                           Vec4* lapCandidateDirs, u32* lpuNumCandidateDirs)
{
    // crossDir = axis1 x axis2 (the vpermwi128 0x63 / vnmsubfp idiom).
    const Vec4 lvCrossDir = Cross(lpGP1->mEdgeDirections[0], lpGP2->mEdgeDirections[0]);

    // crossLenSq = dot3 splat (vmsum3fp128 v13, v9, v9).
    const f32 lfCrossLenSq = Dot3(lvCrossDir, lvCrossDir);

    // vcmpgtfp. all-lanes test on two broadcasts == the plain scalar compare.
    if (!(lfCrossLenSq > KF_RIM_PARALLEL_EPSILON))
    {
        return; // axes (near-)parallel: no rim-to-rim cross direction exists
    }

    // 1/sqrt(crossLenSq): vrsqrtefp estimate + two Newton-Raphson refines,
    // rendered as the exact reciprocal square root the refine converges to.
    const Vec4 lvSepDir = Scale(lvCrossDir, 1.0f / std::sqrt(lfCrossLenSq));

    // Rim-circle data: dimensions lane X = half-extent along the axis
    // (vspltw 0), lane Y = rim radius (vspltw 1), broadcast across the lanes.
    const f32 lfHalfExtent1 = lpGP1->mDimensions.x;
    const f32 lfHalfExtent2 = lpGP2->mDimensions.x;

    Vec4 lvRadius1;
    lvRadius1.x = lpGP1->mDimensions.y;
    lvRadius1.y = lpGP1->mDimensions.y;
    lvRadius1.z = lpGP1->mDimensions.y;
    lvRadius1.w = lpGP1->mDimensions.y;
    Vec4 lvRadius2;
    lvRadius2.x = lpGP2->mDimensions.y;
    lvRadius2.y = lpGP2->mDimensions.y;
    lvRadius2.z = lpGP2->mDimensions.y;
    lvRadius2.w = lpGP2->mDimensions.y;

    const Vec4 lvAxisOffset1 = Scale(lpGP1->mEdgeDirections[0], lfHalfExtent1); // v17
    const Vec4 lvAxisOffset2 = Scale(lpGP2->mEdgeDirections[0], lfHalfExtent2); // v16

    // (+A, +B): both positive rim ends, +sepDir.
    if (RimToRim(Add(lpGP1->mPos, lvAxisOffset1), lpGP1->mEdgeDirections[0], lvRadius1,
                 Add(lpGP2->mPos, lvAxisOffset2), lpGP2->mEdgeDirections[0], lvRadius2,
                 lvSepDir, &lapCandidateDirs[*lpuNumCandidateDirs]))
    {
        ++(*lpuNumCandidateDirs);
    }

    // (-A, +B): negative A rim end, A axis and sepDir sign-flipped (vxor).
    if (RimToRim(Sub(lpGP1->mPos, lvAxisOffset1), Negate(lpGP1->mEdgeDirections[0]), lvRadius1,
                 Add(lpGP2->mPos, lvAxisOffset2), lpGP2->mEdgeDirections[0], lvRadius2,
                 Negate(lvSepDir), &lapCandidateDirs[*lpuNumCandidateDirs]))
    {
        ++(*lpuNumCandidateDirs);
    }

    // (+A, -B): negative B rim end, B axis and sepDir sign-flipped.
    if (RimToRim(Add(lpGP1->mPos, lvAxisOffset1), lpGP1->mEdgeDirections[0], lvRadius1,
                 Sub(lpGP2->mPos, lvAxisOffset2), Negate(lpGP2->mEdgeDirections[0]), lvRadius2,
                 Negate(lvSepDir), &lapCandidateDirs[*lpuNumCandidateDirs]))
    {
        ++(*lpuNumCandidateDirs);
    }

    // (-A, -B): both negative rim ends, both axes flipped, +sepDir again.
    if (RimToRim(Sub(lpGP1->mPos, lvAxisOffset1), Negate(lpGP1->mEdgeDirections[0]), lvRadius1,
                 Sub(lpGP2->mPos, lvAxisOffset2), Negate(lpGP2->mEdgeDirections[0]), lvRadius2,
                 lvSepDir, &lapCandidateDirs[*lpuNumCandidateDirs]))
    {
        ++(*lpuNumCandidateDirs);
    }
}

} // namespace collision
} // namespace rw

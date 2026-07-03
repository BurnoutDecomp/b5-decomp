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
// Cylinder-rim SAT candidate workers (wave 2): RimToEdge @ 0x82BB53F8 and
// RimToRim @ 0x82BB56C8. X360-era additions with no Feb-2007 canonical
// source; the shapes are asm-derived. Shared vocabulary below.
// ===========================================================================

// X360 unk_8327EFB0 -- the shared degenerate-length-squared guard row of the
// rim workers (RimToEdge + RimToRim). Address-only in the export; value
// FLAGGED AS INFERRED: 1.0e-12f, matching the conductor-approved inference
// for the neighbouring guard rows of the same .rdata cluster (unk_8327EEA0 ->
// FeatureEdge::KF_DEGENERATE_EPSILON, unk_8327EFD0 -> Feature::
// KF_DEGENERATE_EPSILON, unk_8327EFC0 in FeaturePrism.cpp).
static const f32 KF_RIM_DEGENERATE_EPSILON = 1.0e-12f;

// flt_820F5E68: the rims' score/degeneracy tolerance scale (tol = minR * this;
// value present in the export's pseudocode, "0.0000001").
static const f32 KF_RIM_RADIUS_TOLERANCE_SCALE = 1.0e-7f;

// flt_82180894: FLT_MIN, the hard zero guards of RimToRim's coplanar/Heron
// paths (same label/value as KF_RIM_PARALLEL_EPSILON below).
static const f32 KF_RIM_FLT_MIN = 1.1754944e-38f;

namespace
{
    // Full-vector vmulfp128: per-lane a * b (RimToRim's probe1 tangent
    // scaling by the broadcast radius row).
    inline Vec4 Mul(const Vec4& a, const Vec4& b)
    {
        Vec4 r;
        r.x = a.x * b.x;
        r.y = a.y * b.y;
        r.z = a.z * b.z;
        r.w = a.w * b.w;
        return r;
    }

    // Full-vector vmaddfp: per-lane a*b + c (the rim-point builds multiply by
    // the broadcast radius ROW, all four lanes carried).
    inline Vec4 Madd(const Vec4& a, const Vec4& b, const Vec4& c)
    {
        Vec4 r;
        r.x = a.x * b.x + c.x;
        r.y = a.y * b.y + c.y;
        r.z = a.z * b.z + c.z;
        r.w = a.w * b.w + c.w;
        return r;
    }

    // lenSq * rsqrt(lenSq) with the vcmpeqfp/vsel zero mask == sqrt(lenSq)
    // guarded to 0 (the rw::math::vpu::Magnitude lowering; used by RimToRim's
    // Heron seed distance and area roots).
    inline f32 GuardedMagnitudeOf(f32 afLenSq)
    {
        return (afLenSq == 0.0f) ? 0.0f
                                 : afLenSq * (1.0f / std::sqrt(afLenSq));
    }

    // The rim-radial select pipeline -- RETAIL-BINARY QUIRK, preserved
    // verbatim (three sites in RimToEdge: 0x82BB543C / 0x82BB5498 /
    // 0x82BB5594; five more in RimToRim). The guarded radial pipeline
    // computes TWO PARALLEL IDENTICAL one-refine rsqrt values r1 and selects
    //     (lenSq >= eps) ? arV*r1*r1 : arV*r1        (vcmpgefp/vnot/vsel)
    // i.e. the VALID path scales by ~1/lenSq, NOT the unit 1/len -- the "rim
    // point" rides at radius R/|arV| instead of R. This is what the retail
    // binary computes (the generator's intent was clearly a guarded
    // normalize, but the emitted second factor multiplies the refine in
    // again); it is harmless downstream -- every consumer feeds the returned
    // direction into SAT interval evaluation -- and it is preserved per the
    // no-improving rule. Rendered with the refine exact per the committed
    // sibling precedent:
    //   valid path      arV * (1/lenSq)
    //   degenerate path arV * (1/sqrt(lenSq))
    // (NaN lenSq fails the vcmpgefp exactly as the scalar >= does.)
    inline Vec4 RimRadialDirection(const Vec4& arV)
    {
        const f32 lfLenSq = Dot3(arV, arV);
        if (lfLenSq >= KF_RIM_DEGENERATE_EPSILON)
        {
            return Scale(arV, 1.0f / lfLenSq);            // V * r1 * r1
        }
        return Scale(arV, 1.0f / std::sqrt(lfLenSq));     // V * r1
    }

    // reject(v, unit axis a) = v - a*dot3(v, a): the vmsum3fp128 + vmulfp128
    // + vsubfp triple every rim projection in RimToRim uses. (The double-
    // cross form a x (v x a) of the rejection tests folds to the same value;
    // both idioms appear in the asm and are noted at their sites.)
    inline Vec4 RejectAxis(const Vec4& arV, const Vec4& arAxis)
    {
        return Sub(arV, Scale(arAxis, Dot3(arV, arAxis)));
    }
}

// ===========================================================================
// rw::collision::RimToEdge @ 0x82BB53F8
// Called by (2): AddRimToEdgeCandidate @ 0x82BB6118 (below) and the pending
// FindBestSepDirWithCylinder candidate builder (sub_82BB64C0).
//
// Candidate SAT separating direction between the rim circle (centre
// aRimCentre, unit axis aRimAxis, lane-broadcast radius aRimRadius) and the
// edge segment [aEdgeP1, aEdgeP2]. X360 passes all five rows by value in VMX
// v1..v5 and returns the direction in v1 (the by-value Vec4 signature is the
// x64 rendering; at both call sites every argument register carries the
// address of a fresh caller-stack copy).
//
//   1. For each edge end E in {P1, P2}: build the rim point nearest E and its
//      score s = dot3(rimPoint - E, edgeVec) -- the signed component of the
//      rim->edge-end gap along the edge.
//   2. Bracket: the (rim point, score, edge end) tuple with the LARGER score
//      becomes the "high" side (vcmpgtfp. swap block at 0x82BB54F8).
//   3. Exactly 10 damped regula-falsi iterations (r11 counter, NO early
//      exit): lambda = (sHigh/(sHigh - sLow) + t/2) / (1 + t) with the
//      damping t = 2^-iteration; the interpolated edge point M gets its
//      nearest rim point and score s_M, which replaces the side matching its
//      sign (s_M < 0 -> low side, s_M >= 0 -> high side, NaN -> low side --
//      preserved by the !(s_M >= 0) polarity below).
//   4. Result: pick the side with the smaller |score| (vcmpgtfp.
//      sHigh > -sLow at 0x82BB561C); direction = normalize(edgePt - rimPt),
//      or, when that delta is degenerate (guard row), normalize(cross(
//      rimTangent, edgeVec)) with rimTangent = cross(rimPt - centre, axis).
//
// VMX lowering: vcfsx-built 1.0/0.5 splats; all broadcast-lane compares
// lower to scalar compares with identical NaN polarity; vrefp + two
// SEQUENTIAL Newton-Raphson refines (the lambda reciprocals) and vrsqrtefp +
// one refine (the tail normalisations) rendered exact per the committed
// sibling precedent. Dead leaf-frame stores are not modelled.
// ===========================================================================
Vec4 RimToEdge(Vec4 aRimCentre, Vec4 aRimAxis, Vec4 aRimRadius,
               Vec4 aEdgeP1, Vec4 aEdgeP2)
{
    // vsubfp v31, v5, v4: the edge vector.
    const Vec4 lvEdge = Sub(aEdgeP2, aEdgeP1);

    // ---- 1. rim point + score for each edge end -----------------------------
    // vsubfp v11/v9; vmsum3fp128 v13/v7 = dot3(end - centre, axis).
    const Vec4 lvDeltaP1 = Sub(aEdgeP1, aRimCentre);          // v11
    const Vec4 lvDeltaP2 = Sub(aEdgeP2, aRimCentre);          // v9
    const f32  lfAxialP1 = Dot3(lvDeltaP1, aRimAxis);         // v13 splat
    const f32  lfAxialP2 = Dot3(lvDeltaP2, aRimAxis);         // v7 splat

    // vmulfp128 v13, v2, v13 / vsubfp v10: perp of (P1 - C) wrt the axis,
    // then the guarded radial pipeline (0x82BB543C..547C) and the rim point
    // vmaddfp v8 = v13*v3 + v1 = centre + radius*radial.
    const Vec4 lvPerpP1 = Sub(lvDeltaP1, Scale(aRimAxis, lfAxialP1));   // v10
    Vec4 lvRimHigh = Madd(RimRadialDirection(lvPerpP1), aRimRadius, aRimCentre); // v8

    // vsubfp v11, v8, v4 / vmsum3fp128 v11, v11, v31: score of the P1 side.
    f32 lfSHigh = Dot3(Sub(lvRimHigh, aEdgeP1), lvEdge);      // v11 splat

    // Same for the P2 side (0x82BB5484..54E4).
    const Vec4 lvPerpP2 = Sub(lvDeltaP2, Scale(aRimAxis, lfAxialP2));   // v9
    Vec4 lvRimLow = Madd(RimRadialDirection(lvPerpP2), aRimRadius, aRimCentre); // v9
    f32  lfSLow   = Dot3(Sub(lvRimLow, aEdgeP2), lvEdge);     // v10 splat

    // Register roles pre-swap: high tuple = (v8 rimP1, v11 sP1, v30 P1),
    // low tuple = (v9 rimP2, v10 sP2, v5 P2).
    Vec4 lvEdgeHigh = aEdgeP1;                                // v30
    Vec4 lvEdgeLow  = aEdgeP2;                                // v5

    // ---- 2. bracket: larger score to the high side --------------------------
    // vcmpgtfp. v13, v10, v11 + mfocrf/extrwi CR6[0]: all-lanes strict
    // greater on broadcasts == the scalar compare (NaN -> no swap).
    if (lfSLow > lfSHigh)
    {
        // The 8x vmr swap block at 0x82BB54F8..5514.
        const Vec4 lvTmpRim  = lvRimHigh;
        const f32  lfTmpS    = lfSHigh;
        const Vec4 lvTmpEdge = lvEdgeHigh;
        lvRimHigh  = lvRimLow;
        lfSHigh    = lfSLow;
        lvEdgeHigh = lvEdgeLow;
        lvRimLow   = lvTmpRim;
        lfSLow     = lfTmpS;
        lvEdgeLow  = lvTmpEdge;
    }

    // ---- 3. 10 damped regula-falsi iterations (no early exit) ---------------
    // vmr v6, v0: the damping factor t starts at 1.0; li r11, 0xA counts.
    f32 lfT = 1.0f;                                           // v6
    for (s32 li = 10; li != 0; --li)                          // addic./bne
    {
        // vsubfp v13 = sHigh - sLow; vaddfp v7 = t + 1; vmulfp128 v28 = t*0.5.
        const f32 lfSpread   = lfSHigh - lfSLow;              // v13
        const f32 lfHalfT    = lfT * 0.5f;                    // v28
        // vrefp + two sequential Newton-Raphson refines on both reciprocals
        // (0x82BB5534..5568), rendered exact:
        //   lambda = (sHigh/(sHigh - sLow) + t/2) / (1 + t)
        const f32 lfLambda = (lfSHigh * (1.0f / lfSpread) + lfHalfT)
                           * (1.0f / (lfT + 1.0f));           // v13

        // vsubfp v7 = 1 - lambda; vmulfp128 v7 = edgeHigh*(1-lambda);
        // vmaddfp v7 = edgeLow*lambda + that.
        const Vec4 lvM = MaddScalar(lvEdgeLow, lfLambda,
                                    Scale(lvEdgeHigh, 1.0f - lfLambda));   // v7

        // Nearest rim point of M (vsubfp/vmsum3fp128/vmulfp128/vsubfp + the
        // guarded radial pipeline 0x82BB5594..55D4 + vmaddfp v6).
        const Vec4 lvDeltaM = Sub(lvM, aRimCentre);           // v13
        const f32  lfAxialM = Dot3(lvDeltaM, aRimAxis);       // v6 splat
        const Vec4 lvPerpM  = Sub(lvDeltaM, Scale(aRimAxis, lfAxialM));    // v4
        const Vec4 lvRimM   = Madd(RimRadialDirection(lvPerpM),
                                   aRimRadius, aRimCentre);   // v6

        // vsubfp v13 = RM - M; vmsum3fp128 v4 = score of M.
        const f32 lfSM = Dot3(Sub(lvRimM, lvM), lvEdge);      // v4 splat

        // vcmpgefp v13, v4, v27(0) + vnot + the six vsel lane selects:
        // s_M < 0 (or NaN) replaces the low side, else the high side.
        if (!(lfSM >= 0.0f))
        {
            lvRimLow  = lvRimM;                               // vsel v9
            lvEdgeLow = lvM;                                  // vsel v5
            lfSLow    = lfSM;                                 // vsel v10
        }
        else
        {
            lvRimHigh  = lvRimM;                              // vsel v8
            lvEdgeHigh = lvM;                                 // vsel v30
            lfSHigh    = lfSM;                                // vsel v11
        }

        lfT = lfHalfT;                                        // vmr v6, v28
    }

    // ---- 4. direction from the smaller-|score| side --------------------------
    // vspltisw/vslw sign mask + vxor: -sLow; vcmpgtfp. v11 > -v10 (CR6[0]):
    // all-lanes strict greater picks the LOW tuple; NaN falls to HIGH.
    Vec4 lvRimPt;
    Vec4 lvEdgePt;
    if (lfSHigh > -lfSLow)
    {
        // 0x82BB5634 block: v13 = v9 - v1, v11 = v5 - v9.
        lvRimPt  = lvRimLow;
        lvEdgePt = lvEdgeLow;
    }
    else
    {
        // 0x82BB564C block: v13 = v8 - v1, v11 = v30 - v8.
        lvRimPt  = lvRimHigh;
        lvEdgePt = lvEdgeHigh;
    }

    // Rim tangent at the winning rim point: cross(rimPt - centre, axis)
    // (the vpermwi128 0x63 / vnmsubfp idiom on both branches).
    const Vec4 lvTangent = Cross(Sub(lvRimPt, aRimCentre), aRimAxis);   // v13->v9

    // vsubfp on the branch + vmsum3fp128 v10: the point delta and its lenSq.
    const Vec4 lvDelta   = Sub(lvEdgePt, lvRimPt);            // v11
    const f32  lfDeltaSq = Dot3(lvDelta, lvDelta);            // v10 splat

    // Fallback direction: cross(tangent, edgeVec) (computed unconditionally
    // at 0x82BB5674..5694, exactly as here).
    const Vec4 lvFallback = Cross(lvTangent, lvEdge);         // v10

    // vcmpgefp v8 = lenSq >= eps guard + vnot; both one-refine vrsqrtefp
    // normalisations rendered exact; vsel v1 returns the selected row.
    if (lfDeltaSq >= KF_RIM_DEGENERATE_EPSILON)
    {
        return Scale(lvDelta, 1.0f / std::sqrt(lfDeltaSq));
    }
    return Scale(lvFallback, 1.0f / std::sqrt(Dot3(lvFallback, lvFallback)));
}

// ===========================================================================
// rw::collision::RimToRim @ 0x82BB56C8
// Called by (1): AddRimToRimCandidates @ 0x82BB61C0 (below).
//
// Tests one pair of cylinder rim circles (centre/unit axis/broadcast radius
// each) for a separating-direction candidate; on success writes ONE 16-byte
// direction to lpCandidate and returns 1, else 0. X360 register contract:
// r3..r9 carry the ADDRESSES of caller-stack copies of the seven vector rows
// (each lvx128'd once), r10 = the output slot; the by-value Vec4 signature is
// the x64 rendering.
//
//   1. tol = min(R1, R2) * 1e-7; reject degenerate radii
//      (FLT_MIN > tol -> return 0).
//   2. Tangent probes at the sepDir cross direction:
//        probe1 = C1 - R1*cross(sepDir, a1)   probe2 = C2 + R2*cross(sepDir, a2)
//      Reject (return 0) when probe1 projected into rim2's plane falls
//      strictly inside rim2's circle, and symmetrically probe2 against rim1
//      -- interlocked rims have no rim-to-rim candidate.
//   3. If |dot(C1 - C2, sepDir)| < tol (centres coplanar along the sepDir):
//      one closed-form alternating projection -- P1 = rim1 point nearest
//      probe2, P2 = rim2 point nearest P1; candidate = normalize(P2 - P1),
//      or normalize(cross(a1 + a2, sepDir)) when the rims touch
//      (|P2-P1|^2 <= FLT_MIN). Returns 1. NOTE: the RAW delta is staged to
//      *lpCandidate BEFORE the degeneracy test (stvx128 at 0x82BB5948) and
//      overwritten -- caller-visible store, preserved.
//   4. General case: seed = C1, refined to the rim-sphere intersection
//      height when the centre spheres overlap: Heron's formula on the
//      triangle (d, R1, R2) -- s = (d + R1 + R2)/2, area^2 =
//      s(s-d)(s-R1)(s-R2), h = 2*area/d, seed = C1 + (probe1 - C1)*(h/R1).
//   5. Bracketed iteration on the signed ANGULAR score
//        s(P1, ref) = dot3(ref - C2, cross(a2, P1 - C2))
//      of two (rim1 point, rim2 reference point, score) tuples. If the
//      scores do not straddle zero, retry the low side with the ANTIPODAL
//      probe (2*C2 - probe2); still same-signed -> return 0. Larger score to
//      the high side, then up to 10 damped regula-falsi steps; s_M replaces
//      the side matching its sign. Convergence: the smaller-|score| side
//      within +/-tol, or 10 iterations.
//   6. Final direction from the converged (ref, P1) pair:
//      normalize(ref - P1); when degenerate (guard row), the fallback
//      normalize(cross(T1, T2)) of the two rim tangents. Stored to
//      *lpCandidate, return 1.
//
// VMX lowering: all broadcast-row compares lower to scalar compares with
// identical NaN polarity; vrefp/vrsqrtefp + two SEQUENTIAL Newton-Raphson
// refines rendered exact (the FINAL block's two one-refine normalisations
// likewise); the vspltisw(-1)/vslw/vxor sign flips are scalar negations.
// Dead leaf-frame stores (the var_60 restagings around the mfocrf blocks)
// are not modelled, matching every committed sibling.
//
// rodata: flt_820F5E68 = 1.0e-7f, flt_82180894 = FLT_MIN, flt_82001C98 =
// 1.0f, flt_82001CC0 = 0.0f, flt_82001D9C = 2.0f, flt_82001DA0 = 0.5f;
// unk_8327EFB0 = the inferred guard row (v5/v6).
// ===========================================================================
RwBool RimToRim(Vec4 aRimCentre1, Vec4 aRimAxis1, Vec4 aRimRadius1,
                Vec4 aRimCentre2, Vec4 aRimAxis2, Vec4 aRimRadius2,
                Vec4 aSepDir, Vec4* lpCandidate)
{
    // ---- 1. degenerate-radius rejection --------------------------------------
    // vminfp v13 = min(R1 row, R2 row); vmulfp128 v23 = min * splat(1e-7);
    // vcmpgtfp. splat(FLT_MIN) > tol (all lanes; broadcast rows -> scalar).
    const f32 lfTol = ((aRimRadius1.x < aRimRadius2.x) ? aRimRadius1.x
                                                       : aRimRadius2.x)
                    * KF_RIM_RADIUS_TOLERANCE_SCALE;          // v23
    if (KF_RIM_FLT_MIN > lfTol)
    {
        return 0;                                             // li r3,0 / blr
    }

    // ---- 2. interlock rejections at the sepDir tangent probes ----------------
    // cross(sepDir, a1) / cross(sepDir, a2): the vpermwi128 0x63 / vnmsubfp
    // idiom (0x82BB5750..5790).
    const Vec4 lvTangent1 = Cross(aSepDir, aRimAxis1);        // v12
    const Vec4 lvTangent2 = Cross(aSepDir, aRimAxis2);        // v6

    // vmaddfp v4 = cross2*R2row + C2; vmulfp128 v12 = cross1*R1row then
    // vsubfp v30 = C1 - that. NOTE the sign asymmetry (probe1 rides the
    // NEGATIVE tangent) -- exactly as the asm.
    const Vec4 lvProbe2 = Madd(lvTangent2, aRimRadius2, aRimCentre2);       // v4
    const Vec4 lvProbe1 = Sub(aRimCentre1, Mul(lvTangent1, aRimRadius1));   // v30

    // probe1 rejected into rim2's plane: the asm folds it as the double cross
    // a2 x ((probe1 - C2) x a2) (0x82BB57A4..57C8) == the axis rejection.
    {
        const Vec4 lvRej = RejectAxis(Sub(lvProbe1, aRimCentre2), aRimAxis2);
        // vcmpgtfp. v9(R2 row squared) > |rej|^2 -> bne return 0.
        if (aRimRadius2.x * aRimRadius2.x > Dot3(lvRej, lvRej))
        {
            return 0;
        }
    }

    // probe2 - C1, KEPT LIVE for the whole general path (v2).
    const Vec4 lvProbe2FromC1 = Sub(lvProbe2, aRimCentre1);   // v2

    // Symmetric rejection: probe2 into rim1's plane (double cross wrt a1,
    // 0x82BB57F0..5814).
    {
        const Vec4 lvRej = RejectAxis(lvProbe2FromC1, aRimAxis1);
        if (aRimRadius1.x * aRimRadius1.x > Dot3(lvRej, lvRej))
        {
            return 0;
        }
    }

    // ---- 3. coplanar fast path ------------------------------------------------
    // vsubfp v12 = C1 - C2; vmsum3fp128 + vandc(sign mask) = |dot(delta,
    // sepDir)|; vcmpgtfp. tol > that (all lanes) -> the closed form.
    const Vec4 lvCentreDelta = Sub(aRimCentre1, aRimCentre2); // v12
    const f32  lfSepDot      = Dot3(lvCentreDelta, aSepDir);
    const f32  lfSepDotAbs   = (lfSepDot < 0.0f) ? -lfSepDot : lfSepDot;   // vandc
    if (lfTol > lfSepDotAbs)
    {
        // P1 = rim1 point nearest probe2 (quirk pipeline 0x82BB5894..58D4;
        // eps row v5 = unk_8327EFB0), vmaddfp v8 = f*R1row + C1.
        const Vec4 lvPerp1 = RejectAxis(lvProbe2FromC1, aRimAxis1);
        const Vec4 lvP1    = Madd(RimRadialDirection(lvPerp1),
                                  aRimRadius1, aRimCentre1);  // v8

        // P2 = rim2 point nearest P1 (second quirk pipeline 0x82BB58F4..5938).
        const Vec4 lvPerp2 = RejectAxis(Sub(lvP1, aRimCentre2), aRimAxis2);
        const Vec4 lvP2    = Madd(RimRadialDirection(lvPerp2),
                                  aRimRadius2, aRimCentre2);  // v0

        // vsubfp v0 = P2 - P1; vmsum3fp128 v13 = lenSq; stvx128 v0, r0, r10:
        // the RAW delta is staged to the output slot BEFORE the test.
        const Vec4 lvDelta   = Sub(lvP2, lvP1);
        const f32  lfDeltaSq = Dot3(lvDelta, lvDelta);
        *lpCandidate = lvDelta;                               // staging store

        if (lfDeltaSq > KF_RIM_FLT_MIN)                       // vcmpgtfp. v11
        {
            // vrsqrtefp + two sequential Newton-Raphson refines == exact.
            *lpCandidate = Scale(lvDelta, 1.0f / std::sqrt(lfDeltaSq));
        }
        else
        {
            // Touching rims: normalize(cross(a1 + a2, sepDir)) (rows
            // reloaded through r4/r7/r9 on console; the by-value params are
            // still live here). Two sequential refines == exact.
            const Vec4 lvAxisSum  = Add(aRimAxis1, aRimAxis2);      // v13
            const Vec4 lvFallback = Cross(lvAxisSum, aSepDir);      // v13
            *lpCandidate = Scale(lvFallback,
                                 1.0f / std::sqrt(Dot3(lvFallback, lvFallback)));
        }
        return 1;                                             // li r3,1
    }

    // ---- 4. general path: Heron-seeded bracketed iteration --------------------
    // vmr v31, v8: the seed starts at C1.
    Vec4 lvSeed = aRimCentre1;                                // v31

    // vaddfp v3 = R1 + R2 rows; vmsum3fp128 v12 = d^2; vmulfp128 v0 = (R1+R2)^2.
    const f32 lfDistSq    = Dot3(lvCentreDelta, lvCentreDelta);      // v12
    const f32 lfRadiusSum = aRimRadius1.x + aRimRadius2.x;           // v3

    if (lfRadiusSum * lfRadiusSum > lfDistSq)                 // vcmpgtfp. v0
    {
        if (lfDistSq > KF_RIM_FLT_MIN)                        // vcmpgtfp. vs splat
        {
            // Heron's formula seed (0x82BB5A50..5B6C). All rsqrt/recip
            // estimate+refine pipelines rendered exact; both roots carry the
            // vcmpeqfp/vsel zero guard.
            const f32 lfDist = GuardedMagnitudeOf(lfDistSq);  // v12 (vsel v22)
            const f32 lfS    = (lfDist + lfRadiusSum) * 0.5f; // v9 (flt_82001DA0)
            const f32 lfAreaSq = ((lfS * (lfS - lfDist))
                                * (lfS - aRimRadius1.x))
                                * (lfS - aRimRadius2.x);      // v9 product order
            const f32 lfArea = GuardedMagnitudeOf(lfAreaSq);  // vsel v21
            // h = (1/d) * (2*area)  (flt_82001D9C = 2.0; vrefp+2 refines).
            const f32 lfHeight = (1.0f / lfDist) * (lfArea * 2.0f);   // v9
            // seed = C1 + (probe1 - C1) * (h/R1)   [vmaddfp v31 = v6*v0 + v31]
            const f32 lfHeightOverR1 = (1.0f / aRimRadius1.x) * lfHeight; // v0
            lvSeed = MaddScalar(Sub(lvProbe1, aRimCentre1),
                                lfHeightOverR1, aRimCentre1);
        }
    }

    // Pre-loop tuples (0x82BB5B70..5D04). eps row v6 = unk_8327EFB0.
    // low P1 candidate: rim1 point nearest probe2 (via U = reject wrt a1);
    // high P1 candidate: rim1 point nearest P2seed (via W), where P2seed is
    // the rim2 point nearest the seed (via W0 = reject wrt a2).
    const Vec4 lvU  = RejectAxis(lvProbe2FromC1, aRimAxis1);        // v3
    const Vec4 lvW0 = RejectAxis(Sub(lvSeed, aRimCentre2), aRimAxis2);   // v2
    const Vec4 lvP2Seed = Madd(RimRadialDirection(lvW0),
                               aRimRadius2, aRimCentre2);           // v2
    const Vec4 lvW  = RejectAxis(Sub(lvP2Seed, aRimCentre1), aRimAxis1); // v1

    Vec4 lvP1Low  = Madd(RimRadialDirection(lvU),
                         aRimRadius1, aRimCentre1);           // v30
    Vec4 lvP1High = Madd(RimRadialDirection(lvW),
                         aRimRadius1, aRimCentre1);           // v26
    Vec4 lvRefLow  = lvProbe2;                                // v4
    Vec4 lvRefHigh = lvP2Seed;                                // v2

    // Angular scores: s = dot3(ref - C2, cross(a2, P1 - C2))
    // (vpermwi128/vnmsubfp cross + vmsum3fp128, 0x82BB5CC8..5D00).
    f32 lfScoreLow = Dot3(Sub(lvRefLow, aRimCentre2),
                          Cross(aRimAxis2, Sub(lvP1Low, aRimCentre2)));    // v1
    f32 lfScoreHigh = Dot3(Sub(lvRefHigh, aRimCentre2),
                           Cross(aRimAxis2, Sub(lvP1High, aRimCentre2)));  // v5

    // Bracket check: vmulfp128 v3 = product; vcmpgtfp. > splat(0.0)
    // (flt_82001CC0 staged to var_40).
    if (lfScoreLow * lfScoreHigh > 0.0f)
    {
        // Retry the low side with the ANTIPODAL rim2 probe
        // (0x82BB5D14..5DC8): ref = 2*C2 - probe2.
        lvRefLow = Sub(Add(aRimCentre2, aRimCentre2), lvRefLow);     // v4
        const Vec4 lvRejA = RejectAxis(Sub(lvRefLow, aRimCentre1),
                                       aRimAxis1);            // v1
        lvP1Low = Madd(RimRadialDirection(lvRejA),
                       aRimRadius1, aRimCentre1);             // v30
        lfScoreLow = Dot3(Sub(lvRefLow, aRimCentre2),
                          Cross(aRimAxis2, Sub(lvP1Low, aRimCentre2)));    // v1

        if (lfScoreLow * lfScoreHigh > 0.0f)                  // vcmpgtfp. again
        {
            return 0;                                         // bne loc_82BB5740
        }
    }

    // Larger score to the high side (vcmpgtfp. v1 > v5 + the 9x vmr swap).
    if (lfScoreLow > lfScoreHigh)
    {
        const Vec4 lvTmpP1  = lvP1High;
        const Vec4 lvTmpRef = lvRefHigh;
        const f32  lfTmpS   = lfScoreHigh;
        lvP1High    = lvP1Low;
        lvRefHigh   = lvRefLow;
        lfScoreHigh = lfScoreLow;
        lvP1Low     = lvTmpP1;
        lvRefLow    = lvTmpRef;
        lfScoreLow  = lfTmpS;
    }

    // ---- 5. damped regula-falsi loop (max 10 iterations) ----------------------
    // t = splat(1.0) (flt_82001C98 via lvlx/vspltw); r7 counter from 0.
    f32 lfT    = 1.0f;                                        // v25
    s32 liIter = 0;                                           // r7

    Vec4 lvRefPick;
    Vec4 lvP1Pick;
    for (;;)
    {
        // vxor sign flip + vcmpgtfp.: which side has the larger |score|.
        if (lfScoreHigh > -lfScoreLow)
        {
            // Low side is the smaller-magnitude one: converged when its
            // score is within -tol (vcmpgtfp. v1 > -v23), or out of budget.
            if (lfScoreLow > -lfTol || liIter == 10)          // cmplwi cr6, 0xA
            {
                lvRefPick = lvRefLow;                         // 0x82BB5E64 block
                lvP1Pick  = lvP1Low;
                break;
            }
        }
        else
        {
            // High side: converged when its score is below +tol.
            if (lfTol > lfScoreHigh || liIter == 10)
            {
                lvRefPick = lvRefHigh;                        // 0x82BB6070 block
                lvP1Pick  = lvP1High;
                break;
            }
        }

        // LABEL_23 body (0x82BB5E8C..606C).
        // lambda = (sHigh/(sHigh - sLow) + t/2) / (1 + t)
        // (vrefp + two sequential refines on both reciprocals == exact; the
        // 1-lambda is a SCALAR lane-0 fsubs against flt_82001C98 = 1.0,
        // splat back -- identical to the scalar form here).
        const f32 lfSpread = lfScoreHigh - lfScoreLow;        // v9 = v5 - v1
        const f32 lfLambda = (lfScoreHigh * (1.0f / lfSpread) + lfT * 0.5f)
                           * (1.0f / (lfT + 1.0f));           // v9

        // M = refLow*lambda + refHigh*(1-lambda)
        // (vmulfp128 v3 = refHigh*(1-l); vmaddfp v9 = refLow*l + v3).
        const Vec4 lvM = MaddScalar(lvRefLow, lfLambda,
                                    Scale(lvRefHigh, 1.0f - lfLambda));

        // Q2 = C2 + R2*normalize(M - C2): the loop's ONE clean normalize
        // (unguarded vrsqrtefp + two sequential refines == exact).
        const Vec4 lvMDelta = Sub(lvM, aRimCentre2);          // v3
        const Vec4 lvQ2 = Madd(Scale(lvMDelta,
                                     1.0f / std::sqrt(Dot3(lvMDelta, lvMDelta))),
                               aRimRadius2, aRimCentre2);     // v29

        // Q1 = rim1 point nearest Q2 (quirk pipeline 0x82BB5F8C..5FF4).
        const Vec4 lvX  = RejectAxis(Sub(lvQ2, aRimCentre1), aRimAxis1);   // v31
        const Vec4 lvQ1 = Madd(RimRadialDirection(lvX),
                               aRimRadius1, aRimCentre1);     // v3

        // s_M = dot3(Q2 - C2, cross(a2, Q1 - C2)).
        const f32 lfSM = Dot3(Sub(lvQ2, aRimCentre2),
                              Cross(aRimAxis2, Sub(lvQ1, aRimCentre2)));   // v9

        // vcmpgtfp. splat(0.0) > s_M: negative score replaces the low side
        // (NaN falls to the high side, matching the all-lanes bit).
        if (0.0f > lfSM)
        {
            lvRefLow   = lvQ2;                                // vmr v4, v29
            lvP1Low    = lvQ1;                                // vmr v30, v3
            lfScoreLow = lfSM;                                // vmr v1, v9
        }
        else
        {
            lvRefHigh   = lvQ2;                               // vmr v2, v29
            lvP1High    = lvQ1;                               // vmr v26, v3
            lfScoreHigh = lfSM;                               // vmr v5, v9
        }

        ++liIter;                                             // addi r7, r7, 1
        lfT *= 0.5f;             // v95[0] = 0.5 splat; vmulfp128 v25
    }

    // ---- 6. final direction (LABEL_28, 0x82BB607C..6108) -----------------------
    // vsubfp v13/v8/v9 on the picked tuple, then the two rim tangents and
    // their cross as the degenerate fallback.
    const Vec4 lvRefFromC2 = Sub(lvRefPick, aRimCentre2);     // v13
    const Vec4 lvP1FromC1  = Sub(lvP1Pick, aRimCentre1);      // v8
    const Vec4 lvDelta     = Sub(lvRefPick, lvP1Pick);        // v9

    const Vec4 lvTangentRim2 = Cross(lvRefFromC2, aRimAxis2); // v11 (T2)
    const Vec4 lvTangentRim1 = Cross(lvP1FromC1, aRimAxis1);  // v8  (T1)

    const f32  lfDeltaSq  = Dot3(lvDelta, lvDelta);           // v10
    const Vec4 lvFallback = Cross(lvTangentRim1, lvTangentRim2);   // v8

    // vcmpgefp v7 = lenSq >= eps + vnot; both ONE-refine vrsqrtefp
    // normalisations rendered exact; vsel v0 + stvx128 to r10.
    if (lfDeltaSq >= KF_RIM_DEGENERATE_EPSILON)
    {
        *lpCandidate = Scale(lvDelta, 1.0f / std::sqrt(lfDeltaSq));
    }
    else
    {
        *lpCandidate = Scale(lvFallback,
                             1.0f / std::sqrt(Dot3(lvFallback, lvFallback)));
    }
    return 1;                                                 // li r3,1 / blr
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

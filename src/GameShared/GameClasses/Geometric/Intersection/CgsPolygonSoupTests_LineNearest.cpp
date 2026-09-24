// =================================================================================================
// GameShared/GameClasses/Geometric/Intersection/CgsPolygonSoupTests_LineNearest.cpp
//
// The NEAREST single-sided line-vs-polygon-soup family, reconstructed from BURNOUT_X360_ARTIST.XEX
// (scene-query wave 1b, 2026-09-02):
//
//   CgsGeometric::IntersectLinePolySoupTriangleSingleSided4   @ 0x8283B520  (152)
//   CgsGeometric::IntersectLinePolygonSoupNearestSingleSided  @ 0x8283BC98  (575)
// and, since 2026-09-24 (crash parity FX-GEOMETRIC), its ALL-HITS sibling over the same 4-wide kernel:
//   CgsGeometric::IntersectLinePolygonSoupSingleSided         @ 0x8283C598  (849)
// -- the kernel under BaseCollisionGenerator::CollideLineAgainstPolySoupList @0x82812AE0 (the scene's
// fine line test: the place-on-track drop test and the triangle-collision line tests). See its banner.
//
// This is the kernel under BaseCollisionGenerator::CollideLineAgainstPolySoupListNearest
// @0x828131C0, i.e. under every race car's 10 m above-ground ray (VehicleManager::
// GenerateAboveGroundLineTests -> SceneManagerModule::ProcessTriangleCollisionLineTestNearests).
// Until this TU landed, that ray trapped there every frame (measured: 3201 traps in 3201 frames,
// scratch/flow_run/sq1_plumb1) and UpdateDriftState's guard 8 (`!mAboveGroundTestResult.mbValid`)
// killed every drift on its first frame.
//
// NOT the Moller-Trumbore kernel that ContactGeneratorJob.cpp inlines for the streamed line
// tests. ContactGeneratorJob.cpp:70 already says so; this file is the other algorithm.
//
// ---- HOW THE VMX WAS READ ----------------------------------------------------------------------
// IDA prints VMX128 source registers wrong (one high bit of VA swapped on some forms), so every
// operand below was re-decoded from the raw instruction words (field layout
// VD = bits[25:21]|bits[3:2]<<5, VB = bits[15:11]|bits[1:0]<<5, VA = bits[20:16]|bit5<<5|bit10<<6,
// calibrated on 160 vmr128/vor128 samples). Exactly two IDA misprints mattered in the kernel:
// `vmaddfp128 v27, v95, v11, v27` @0x8283B664 and `vmaddfp128 v13, v95, v12, v13` @0x8283B738 are
// both v63 (= `vsubfp128 v63, v20, v27` = E.z - S.z), not v95 -- i.e. the hit point's z and the
// re-computed (E-S).n use the segment's z extent, as they must.
//
// ---- THE 4-WIDE KERNEL, lane by lane -----------------------------------------------------------
// Inputs on the console: v1..v12 = the three vertices of four triangles in the order
// (v1,v2,v3)=tri0, (v4,v5,v6)=tri1, (v7,v8,v9)=tri2, (v10,v11,v12)=tri3; v13 = S (segment start);
// arg_E0 (stack, 16 bytes) = E (segment end); r3..r6 = four 16-byte out slots.
//   0x8283B524..0x8283B5A4  AoS->SoA transposes (vmrghw/vmrglw): lane k of {v9,v7,v8} = V0.xyz of
//                           tri k, {v26,v25,v31} = V1, {v24,v23,v28} = V2.
//   0x8283B5A8..0x8283B5E8  e01 = V1-V0 (v1,v6,v4), e12 = V2-V1 (v3,v2,v5), e20 = V0-V2 (v21,v19,v18),
//                           d = E-S (v15,v14,v63).
//   0x8283B5EC..0x8283B604  n = e01 x e12 (v0,v13,v12) -- three vmulfp + three vnmsubfp.
//   0x8283B608..0x8283B650  E.n, S.n, V0.n; denom = E.n - S.n (v22); num = V0.n - S.n (v20);
//                           t = num * refine(vrefp(denom)) -- ONE Newton-Raphson step;
//                           P = S + d*t (v30,v29,v27); the four t lanes are splatted and stored.
//   0x8283B654..0x8283B760  f01 = (P-V0).(e01 x n), f12 = (P-V1).(e12 x n), f20 = (P-V2).(e20 x n);
//                           cXY = (0 >= fXY) via vcmpgefp against a stored zero.
//   0x8283B764..0x8283B77C  hit = ((c01&c12&c20) | !(c01|c12|c20)) & (0 >= d.n) & (denom != 0)
//                           & (t >= 0) & (1 >= t).   `vnot v12, v7` is the denom != 0 term.
//
// ---- PC LOWERING, stated once ------------------------------------------------------------------
// The tree's rw::math::vpu::Vector3 is a plain 16-byte {x,y,z,w} struct with no SIMD operations;
// the standing precedent for this family (CgsPolygonSoupTests.cpp, CgsTriangle4.cpp,
// ContactGeneratorJob.cpp) is portable scalar float math with a lane as an ARRAY INDEX. Followed.
// TWO PLACES NOT BIT-IDENTICAL, both flagged at the site: `vrefp` + 1 Newton-Raphson step is
// lowered to a divide (kernel), `vrsqrtefp` + 2 Newton-Raphson steps to 1/sqrt (driver's normal).
// Both are more accurate than the console; neither moves an accept/reject decision except in the
// last ulps. The compares are kept in the console's orientation (`0 >= f`, `best >= t`) so the
// tie-breaking and the treatment of NaN (every compare false) are the console's.
// =================================================================================================
#include "GameShared/GameClasses/Geometric/Intersection/CgsPolygonSoupTests.h"

#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoup.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupPoly.h"
#include "BrnCommonTypes.h"   // Vector3 / Vector4

#include <cmath>              // std::sqrt (the normal's length)

namespace CgsGeometric
{
    namespace
    {
        // The console's "no hit yet" line parameter: `vspltisw v0, 2 ; vcsxwfp128 v126, v0, 0` at
        // 0x8283BCB4/0x8283BCD0, also the seed CollideLineAgainstPolySoupListNearest writes.
        const f32 KF_LINE_PARAM_NO_HIT = 2.0f;

        struct NearestState
        {
            f32     mfBestT;     // v125, seeded 2.0
            Vector3 mV0;         // v123
            Vector3 mV1;         // v124
            Vector3 mV2;         // v122
            u32     mu32Tag;     // v121
        };

        // One lane of the driver's post-kernel select chain (e.g. lanes 0..3 of the quad-pair arm,
        // 0x8283BEA8..0x8283BF9C). The lanes are applied IN ORDER and each one compares against the
        // best the previous lane left (`vsel v12,...` then `vcmpgefp v13, v12, v13`), which is why
        // this is a function called per lane and not a min over four.
        inline void ConsiderLane(NearestState&  lrState,
                                 bool           lbHit,
                                 f32            lfT,
                                 const Vector3& lrV0,
                                 const Vector3& lrV1,
                                 const Vector3& lrV2,
                                 u32            lu32Tag)
        {
            // `vsel v0, v126(2.0), v13(t), v7(hit mask)` -- a miss reads as t == 2.0.
            const f32 lfCandidate = lbHit ? lfT : KF_LINE_PARAM_NO_HIT;

            // `vcmpgefp128 v8, v125, v0` (best >= t) AND `vcmpgefp128 v0, v0, v127` (t >= 0).
            // Equal t replaces: the LATER triangle wins a tie. A 2.0 candidate also "replaces" a
            // seeded 2.0 (2.0 >= 2.0), which is harmless -- it carries no hit either way.
            if ((lfCandidate >= 0.0f) && (lrState.mfBestT >= lfCandidate))
            {
                lrState.mfBestT  = lfCandidate;
                lrState.mV0      = lrV0;
                lrState.mV1      = lrV1;
                lrState.mV2      = lrV2;
                lrState.mu32Tag  = lu32Tag;
            }
        }
    }

    // @ 0x8283B520 (152)
    u32 IntersectLinePolySoupTriangleSingleSided4(const Vector3  laV0[4],
                                                  const Vector3  laV1[4],
                                                  const Vector3  laV2[4],
                                                  const Vector3& lStart,
                                                  const Vector3& lEnd,
                                                  f32            lafOutT[4])
    {
        // vspltw of the segment: v30/v29/v27 = S.xyz, v11/v22/v20 = E.xyz; d = E - S (v15,v14,v63).
        const f32 lfSx = lStart.x, lfSy = lStart.y, lfSz = lStart.z;
        const f32 lfDx = lEnd.x - lfSx;
        const f32 lfDy = lEnd.y - lfSy;
        const f32 lfDz = lEnd.z - lfSz;

        u32 lu32HitMask = 0;

        for (s32 liLane = 0; liLane < 4; ++liLane)
        {
            const Vector3& lrV0 = laV0[liLane];
            const Vector3& lrV1 = laV1[liLane];
            const Vector3& lrV2 = laV2[liLane];

            // 0x8283B5A8..0x8283B5E8: the three edges (e01 = V1-V0, e12 = V2-V1, e20 = V0-V2).
            const f32 lfE01x = lrV1.x - lrV0.x, lfE01y = lrV1.y - lrV0.y, lfE01z = lrV1.z - lrV0.z;
            const f32 lfE12x = lrV2.x - lrV1.x, lfE12y = lrV2.y - lrV1.y, lfE12z = lrV2.z - lrV1.z;
            const f32 lfE20x = lrV0.x - lrV2.x, lfE20y = lrV0.y - lrV2.y, lfE20z = lrV0.z - lrV2.z;

            // 0x8283B5EC..0x8283B604: n = e01 x e12.
            const f32 lfNx = lfE01y * lfE12z - lfE01z * lfE12y;
            const f32 lfNy = lfE01z * lfE12x - lfE01x * lfE12z;
            const f32 lfNz = lfE01x * lfE12y - lfE01y * lfE12x;

            // 0x8283B608..0x8283B640: E.n, S.n, V0.n; denom = E.n - S.n; num = V0.n - S.n.
            const f32 lfDotE  = lEnd.x * lfNx + lEnd.y * lfNy + lEnd.z * lfNz;
            const f32 lfDotS  = lfSx * lfNx + lfSy * lfNy + lfSz * lfNz;
            const f32 lfDotV0 = lrV0.x * lfNx + lrV0.y * lfNy + lrV0.z * lfNz;
            const f32 lfDenom = lfDotE - lfDotS;
            const f32 lfNum   = lfDotV0 - lfDotS;

            // 0x8283B644..0x8283B650: t = num * (1/denom). PC LOWERING: `vrefp v11, v22` + one
            // Newton-Raphson step (`vnmsubfp v17, v22, v16, v11 ; vmaddfp v11, v11, v17, v11`) is a
            // divide here. A zero denominator gives inf/NaN exactly as the console's estimate
            // does, and is masked out below by the console's own `denom != 0` term.
            const f32 lfT = lfNum / lfDenom;

            // 0x8283B658..0x8283B664: P = S + d*t. Stored (splatted) to the lane's out slot.
            const f32 lfPx = lfDx * lfT + lfSx;
            const f32 lfPy = lfDy * lfT + lfSy;
            const f32 lfPz = lfDz * lfT + lfSz;
            lafOutT[liLane] = lfT;

            // 0x8283B668..0x8283B740: the three edge functions, (P - Vi) . (ei x n).
            const f32 lfC01x = lfE01y * lfNz - lfE01z * lfNy;
            const f32 lfC01y = lfE01z * lfNx - lfE01x * lfNz;
            const f32 lfC01z = lfE01x * lfNy - lfE01y * lfNx;
            const f32 lfF01  = (lfPx - lrV0.x) * lfC01x + (lfPy - lrV0.y) * lfC01y + (lfPz - lrV0.z) * lfC01z;

            const f32 lfC12x = lfE12y * lfNz - lfE12z * lfNy;
            const f32 lfC12y = lfE12z * lfNx - lfE12x * lfNz;
            const f32 lfC12z = lfE12x * lfNy - lfE12y * lfNx;
            const f32 lfF12  = (lfPx - lrV1.x) * lfC12x + (lfPy - lrV1.y) * lfC12y + (lfPz - lrV1.z) * lfC12z;

            const f32 lfC20x = lfE20y * lfNz - lfE20z * lfNy;
            const f32 lfC20y = lfE20z * lfNx - lfE20x * lfNz;
            const f32 lfC20z = lfE20x * lfNy - lfE20y * lfNx;
            const f32 lfF20  = (lfPx - lrV2.x) * lfC20x + (lfPy - lrV2.y) * lfC20y + (lfPz - lrV2.z) * lfC20z;

            // 0x8283B6F0 / 0x8283B718 / 0x8283B75C: `vcmpgefp cXY, zero, fXY`.
            const bool lbC01 = (0.0f >= lfF01);
            const bool lbC12 = (0.0f >= lfF12);
            const bool lbC20 = (0.0f >= lfF20);

            // 0x8283B738..0x8283B750: d.n re-summed from the components (NOT the denom register),
            // then `vcmpgefp v13, zero, v13` -- single-sided: the segment must run AGAINST n.
            const f32  lfDdotN     = lfDz * lfNz + (lfDy * lfNy + lfDx * lfNx);
            const bool lbBackface  = (0.0f >= lfDdotN);

            // 0x8283B71C `vcmpeqfp v7, v22, zero` then `vnot v12, v7` at 0x8283B754.
            const bool lbDenomNonZero = !(lfDenom == 0.0f);

            // 0x8283B744 / 0x8283B748: `vcmpgefp v4, one, t` and `vcmpgefp v11, t, zero`.
            const bool lbTIn = (lfT >= 0.0f) && (1.0f >= lfT);

            // 0x8283B764..0x8283B77C.
            const bool lbAll    = lbC01 && lbC12 && lbC20;
            const bool lbNone   = !(lbC01 || lbC12 || lbC20);
            const bool lbInside = lbAll || lbNone;

            if (lbInside && lbBackface && lbDenomNonZero && lbTIn)
            {
                lu32HitMask |= (1u << liLane);
            }
        }

        return lu32HitMask;
    }

    // @ 0x8283BC98 (575)
    bool IntersectLinePolygonSoupNearestSingleSided(const PolygonSoup&         lPolygonSoup,
                                                    PolySoupLineNearestResult* lpOutResult,
                                                    const Vector3&             lStart,
                                                    const Vector3&             lEnd)
    {
        // 0x8283BCB4..0x8283BD10: state seeded -- best t 2.0, vertices/tag zero (v127).
        NearestState lState;
        lState.mfBestT = KF_LINE_PARAM_NO_HIT;
        lState.mV0.SetZero();
        lState.mV1.SetZero();
        lState.mV2.SetZero();
        lState.mu32Tag = 0;

        // 0x8283BCD4..0x8283BD00: counts. Quads come first in the soup (CgsPolygonSoup.h); the
        // triangle count is `NumPolygons - NumQuads` (u16). Pairs of quads and quartets of triangles
        // fill all four lanes; the odd quad fills lanes 0/1 (lanes 2/3 duplicate them and are
        // ignored), the odd triangles go one per call in lane 0 (lanes 1..3 duplicate it).
        const u32 lu32NumQuads     = lPolygonSoup.GetNumQuads();
        const u32 lu32NumTriangles = static_cast<u16>(lPolygonSoup.GetNumPolygons() - lu32NumQuads);
        const u32 lu32NumQuadPairs = lu32NumQuads >> 1;
        const u32 lu32OddQuad      = lu32NumQuads - 2u * lu32NumQuadPairs;
        const u32 lu32NumQuartets  = lu32NumTriangles >> 2;
        const u32 lu32OddTriangles = lu32NumTriangles - 4u * lu32NumQuartets;

        // 0x8283BD14 UnpackPolygonSoupVertices(stack, soup). The soup's vertex count is a u8.
        Vector3 laVertices[256];
        UnpackPolygonSoupVertices(laVertices, lPolygonSoup);

        // 0x8283BD20 GetPolygon(0); every arm advances the byte cursor by 12 per poly.
        const u8* lpPoly = lPolygonSoup.GetPolygon(0);

        Vector3 laV0[4], laV1[4], laV2[4];
        f32     lafT[4];

        // ---- quad pairs (0x8283BD3C..0x8283BFA0) ---------------------------------------------
        // Quad A bytes 4..7 -> tri0 = (A0,A1,A2), tri1 = (A3,A2,A1); quad B -> tri2, tri3 likewise.
        for (u32 luPair = 0; luPair < lu32NumQuadPairs; ++luPair)
        {
            const PolygonSoupPoly* lpA = reinterpret_cast<const PolygonSoupPoly*>(lpPoly);
            const PolygonSoupPoly* lpB = reinterpret_cast<const PolygonSoupPoly*>(lpPoly + 12);

            const Vector3& lrA0 = laVertices[lpA->mau8VertexIndex[0]];   // v116
            const Vector3& lrA1 = laVertices[lpA->mau8VertexIndex[1]];   // v120
            const Vector3& lrA2 = laVertices[lpA->mau8VertexIndex[2]];   // v119
            const Vector3& lrA3 = laVertices[lpA->mau8VertexIndex[3]];   // v115
            const Vector3& lrB0 = laVertices[lpB->mau8VertexIndex[0]];   // v113
            const Vector3& lrB1 = laVertices[lpB->mau8VertexIndex[1]];   // v118
            const Vector3& lrB2 = laVertices[lpB->mau8VertexIndex[2]];   // v117
            const Vector3& lrB3 = laVertices[lpB->mau8VertexIndex[3]];   // v112
            const u32      lu32TagA = lpA->muSurfaceTag;                 // v114
            const u32      lu32TagB = lpB->muSurfaceTag;                 // v111

            laV0[0] = lrA0; laV1[0] = lrA1; laV2[0] = lrA2;
            laV0[1] = lrA3; laV1[1] = lrA2; laV2[1] = lrA1;
            laV0[2] = lrB0; laV1[2] = lrB1; laV2[2] = lrB2;
            laV0[3] = lrB3; laV1[3] = lrB2; laV2[3] = lrB1;

            const u32 lu32Hits = IntersectLinePolySoupTriangleSingleSided4(laV0, laV1, laV2, lStart, lEnd, lafT);

            ConsiderLane(lState, (lu32Hits & 1u) != 0, lafT[0], lrA0, lrA1, lrA2, lu32TagA);
            ConsiderLane(lState, (lu32Hits & 2u) != 0, lafT[1], lrA3, lrA2, lrA1, lu32TagA);
            ConsiderLane(lState, (lu32Hits & 4u) != 0, lafT[2], lrB0, lrB1, lrB2, lu32TagB);
            ConsiderLane(lState, (lu32Hits & 8u) != 0, lafT[3], lrB3, lrB2, lrB1, lu32TagB);

            lpPoly += 24;
        }

        // ---- the odd quad (0x8283BFA4..0x8283C108) -- lanes 0/1 only ------------------------
        if (lu32OddQuad != 0)
        {
            const PolygonSoupPoly* lpA = reinterpret_cast<const PolygonSoupPoly*>(lpPoly);

            const Vector3& lrA0 = laVertices[lpA->mau8VertexIndex[0]];   // v118
            const Vector3& lrA1 = laVertices[lpA->mau8VertexIndex[1]];   // v120
            const Vector3& lrA2 = laVertices[lpA->mau8VertexIndex[2]];   // v119
            const Vector3& lrA3 = laVertices[lpA->mau8VertexIndex[3]];   // v117
            const u32      lu32TagA = lpA->muSurfaceTag;                 // v116

            laV0[0] = lrA0; laV1[0] = lrA1; laV2[0] = lrA2;
            laV0[1] = lrA3; laV1[1] = lrA2; laV2[1] = lrA1;
            laV0[2] = lrA0; laV1[2] = lrA1; laV2[2] = lrA2;   // duplicates, results unread
            laV0[3] = lrA3; laV1[3] = lrA2; laV2[3] = lrA1;

            const u32 lu32Hits = IntersectLinePolySoupTriangleSingleSided4(laV0, laV1, laV2, lStart, lEnd, lafT);

            ConsiderLane(lState, (lu32Hits & 1u) != 0, lafT[0], lrA0, lrA1, lrA2, lu32TagA);
            ConsiderLane(lState, (lu32Hits & 2u) != 0, lafT[1], lrA3, lrA2, lrA1, lu32TagA);

            lpPoly += 12;
        }

        // ---- triangle quartets (0x8283C10C..0x8283C3A4) -- tri k = (Pk0, Pk1, Pk2) -----------
        for (u32 luQuartet = 0; luQuartet < lu32NumQuartets; ++luQuartet)
        {
            const PolygonSoupPoly* lapP[4] =
            {
                reinterpret_cast<const PolygonSoupPoly*>(lpPoly),
                reinterpret_cast<const PolygonSoupPoly*>(lpPoly + 12),
                reinterpret_cast<const PolygonSoupPoly*>(lpPoly + 24),
                reinterpret_cast<const PolygonSoupPoly*>(lpPoly + 36),
            };

            for (s32 liLane = 0; liLane < 4; ++liLane)
            {
                laV0[liLane] = laVertices[lapP[liLane]->mau8VertexIndex[0]];
                laV1[liLane] = laVertices[lapP[liLane]->mau8VertexIndex[1]];
                laV2[liLane] = laVertices[lapP[liLane]->mau8VertexIndex[2]];
            }

            const u32 lu32Hits = IntersectLinePolySoupTriangleSingleSided4(laV0, laV1, laV2, lStart, lEnd, lafT);

            for (s32 liLane = 0; liLane < 4; ++liLane)
            {
                ConsiderLane(lState, (lu32Hits & (1u << liLane)) != 0, lafT[liLane],
                             laV0[liLane], laV1[liLane], laV2[liLane], lapP[liLane]->muSurfaceTag);
            }

            lpPoly += 48;
        }

        // ---- the odd triangles (0x8283C3A8..0x8283C4DC) -- one per call, lane 0 only ---------
        for (u32 luOdd = 0; luOdd < lu32OddTriangles; ++luOdd)
        {
            const PolygonSoupPoly* lpP = reinterpret_cast<const PolygonSoupPoly*>(lpPoly);

            const Vector3& lrP0 = laVertices[lpP->mau8VertexIndex[0]];   // v120
            const Vector3& lrP1 = laVertices[lpP->mau8VertexIndex[1]];   // v119
            const Vector3& lrP2 = laVertices[lpP->mau8VertexIndex[2]];   // v118

            for (s32 liLane = 0; liLane < 4; ++liLane)
            {
                laV0[liLane] = lrP0; laV1[liLane] = lrP1; laV2[liLane] = lrP2;
            }

            const u32 lu32Hits = IntersectLinePolySoupTriangleSingleSided4(laV0, laV1, laV2, lStart, lEnd, lafT);

            ConsiderLane(lState, (lu32Hits & 1u) != 0, lafT[0], lrP0, lrP1, lrP2, lpP->muSurfaceTag);

            lpPoly += 12;
        }

        // ---- the record (0x8283C4E0..0x8283C598) ---------------------------------------------
        lpOutResult->mVertex0 = lState.mV0;                                  // stvx128 v123, +0x00
        lpOutResult->mVertex1 = lState.mV1;                                  // stvx128 v124, +0x10
        lpOutResult->mVertex2 = lState.mV2;                                  // stvx128 v122, +0x20

        lpOutResult->mLineParam.x = lState.mfBestT;                          // stvx128 v125, +0x50
        lpOutResult->mLineParam.y = lState.mfBestT;
        lpOutResult->mLineParam.z = lState.mfBestT;
        lpOutResult->mLineParam.w = lState.mfBestT;

        // `vmaddfp128 v108, v13(E-S), v125(t), v108(S)` -> +0x40.
        lpOutResult->mPosition.x = (lEnd.x - lStart.x) * lState.mfBestT + lStart.x;
        lpOutResult->mPosition.y = (lEnd.y - lStart.y) * lState.mfBestT + lStart.y;
        lpOutResult->mPosition.z = (lEnd.z - lStart.z) * lState.mfBestT + lStart.z;
        lpOutResult->mPosition.w = (lEnd.w - lStart.w) * lState.mfBestT + lStart.w;

        // Normal: a = V2-V1 (v12), b = V1-V0 (v11); `vnmsubfp v0, b.yzx, (b*a.yzx), a` then
        // `vpermwi 0x63` (yzx) gives b x a = (V1-V0) x (V2-V1) -- the SAME orientation as the
        // kernel's n = e01 x e12, so it faces the segment's start. Then vmsum3fp128 + vrsqrtefp +
        // two Newton-Raphson steps (0.5 = `vcfsx v10, 1, 1`) and a multiply.
        // PC LOWERING: 1/sqrt. A no-hit record (all-zero vertices) yields 0*inf = NaN lanes on
        // BOTH platforms; nothing reads the normal of a record whose return was "no hit".
        {
            const f32 lfAx = lState.mV2.x - lState.mV1.x, lfAy = lState.mV2.y - lState.mV1.y, lfAz = lState.mV2.z - lState.mV1.z;
            const f32 lfBx = lState.mV1.x - lState.mV0.x, lfBy = lState.mV1.y - lState.mV0.y, lfBz = lState.mV1.z - lState.mV0.z;
            const f32 lfNx = lfBy * lfAz - lfBz * lfAy;
            const f32 lfNy = lfBz * lfAx - lfBx * lfAz;
            const f32 lfNz = lfBx * lfAy - lfBy * lfAx;
            const f32 lfLenSq  = lfNx * lfNx + lfNy * lfNy + lfNz * lfNz;
            const f32 lfInvLen = 1.0f / std::sqrt(lfLenSq);                 // vrsqrtefp + 2 NR
            lpOutResult->mNormal.x = lfNx * lfInvLen;                        // stvx128 v0, +0x30
            lpOutResult->mNormal.y = lfNy * lfInvLen;
            lpOutResult->mNormal.z = lfNz * lfInvLen;
            lpOutResult->mNormal.w = 0.0f * lfInvLen;   // the console's w lane: (0-0)*(1/len)
        }

        // `stvlx128 v121, +0x60` -- the tag, splatted.
        lpOutResult->mau32Tag[0] = lState.mu32Tag;
        lpOutResult->mau32Tag[1] = lState.mu32Tag;
        lpOutResult->mau32Tag[2] = lState.mu32Tag;
        lpOutResult->mau32Tag[3] = lState.mu32Tag;

        // `vcfsx v13, 1, 0 ; vcmpgefp128 v0, v13, v125 ; vspltw v1, v0, 0` -- 1.0 >= best t.
        return (1.0f >= lState.mfBestT);
    }

    // =============================================================================================
    // IntersectLinePolygonSoupSingleSided @ 0x8283C598 (849) -- crash parity FX-GEOMETRIC, 2026-09-24.
    // DWARF CgsPolygonSoupTests.cpp:2238 (PS3 @0xB6C1AC, a differently scheduled body with the same
    // record/early-out contract). One caller: BaseCollisionGenerator::CollideLineAgainstPolySoupList.
    //
    //   r3 = soup, v1 = start (-> v127), v2 = end (-> v126), r4 = lpResultBuffer (r31), r5 = max (r27)
    //   0x8283C5CC..0x8283C61C  counts, as in the Nearest sibling but with the TRIANGLE count a u8
    //                           (`subf ; clrlwi 24`): pairs = quads >> 1, oddQuad = quads - 2*pairs,
    //                           quartets = tris >> 2 (`extrwi 14,16`), oddTris = tris - 4*quartets
    //   0x8283C620  UnpackPolygonSoupVertices(sp+0x160, soup) ; 0x8283C62C  GetPolygon(soup, 0)
    //   batches, each one IntersectLinePolySoupTriangleSingleSided4 call (E on the stack at arg_E0):
    //     quad pairs 0x8283C660  lanes (A0,A1,A2) (A3,A2,A1) (B0,B1,B2) (B3,B2,B1), tags A,A,B,B
    //     odd quad   0x8283CA90  lanes (A0,A1,A2) (A3,A2,A1) + duplicates; lanes 0/1 read
    //                            (lanes 1..3 all write their t to var_1220 -- r4=r5=r6 -- and the
    //                            kernel stores r3,r4,r5,r6 in order, so var_1220 ends holding lane 3's
    //                            t, the SAME triangle as lane 1)
    //     quartets   0x8283CCC4  lane k = poly k's (i0,i1,i2), its own tag
    //     odd tris   0x8283D160  one poly in all four lanes; lane 0 read
    //   per lane, `vspltw v0, mask, k ; vcmpeqfp128. v0, zero` -> all equal == no hit; else (e.g.
    //   0x8283C774..0x8283C818 for pair lane 0):
    //     +0x00/+0x10/+0x20  stvx128 V0 / V1 / V2 (w lanes as unpacked)
    //     +0x50              stvx128 the kernel's t slot (t splatted)
    //     +0x40              vmaddfp128 (E - S) * t + S, all four lanes
    //     +0x30              c = (V1-V0) x (V2-V1) (vpermwi 0x63 yzx pair + vnmsubfp), vmsum3fp128,
    //                        vrsqrtefp + two Newton-Raphson steps (0.5 = `vcfsx 1,1`), c * that
    //     +0x60              stvlx128 the poly's tag splat (muSurfaceTag + the padding words)
    //     ++found ; `cmpw found, max ; bge` -> return found (tested AFTER the record)
    //   0x8283D2C8  return found (r30)
    //
    // PC LOWERING (the Nearest sibling's, stated in the banner): vrsqrtefp + 2 NR is 1/sqrt, the
    // fused multiply-adds are a multiply then an add. No accept/reject decision is made here -- the
    // kernel made it -- so these touch only the last ulps of the record.
    // =============================================================================================
    namespace
    {
        // One hit record, the block the console repeats per lane.
        void WriteSingleSidedHit(PolySoupLineNearestResult& lrResult,
                                 const Vector3&             lrV0,
                                 const Vector3&             lrV1,
                                 const Vector3&             lrV2,
                                 f32                        lfT,
                                 const Vector3&             lrLineStart,
                                 const Vector3&             lrLineEnd,
                                 u32                        lu32SurfaceTag)
        {
            lrResult.mVertex0 = lrV0;                                   // stvx128 +0x00
            lrResult.mVertex1 = lrV1;                                   // stvx128 +0x10
            lrResult.mVertex2 = lrV2;                                   // stvx128 +0x20

            lrResult.mLineParam.x = lfT;                                // stvx128 +0x50 (the t slot)
            lrResult.mLineParam.y = lfT;
            lrResult.mLineParam.z = lfT;
            lrResult.mLineParam.w = lfT;

            // `vmaddfp128 P, (E - S), t, S` -> +0x40.
            lrResult.mPosition.x = (lrLineEnd.x - lrLineStart.x) * lfT + lrLineStart.x;
            lrResult.mPosition.y = (lrLineEnd.y - lrLineStart.y) * lfT + lrLineStart.y;
            lrResult.mPosition.z = (lrLineEnd.z - lrLineStart.z) * lfT + lrLineStart.z;
            lrResult.mPosition.w = (lrLineEnd.w - lrLineStart.w) * lfT + lrLineStart.w;

            // a = V1 - V0 (v12), b = V2 - V1 (v13); c = a x b. The console's w lane is
            // a.w*b.w - a.w*b.w (the yzx permute keeps w in w) -- zero for a soup's vertices, whose w
            // lanes all carry the same unpack junk.
            const f32 lfAx = lrV1.x - lrV0.x, lfAy = lrV1.y - lrV0.y, lfAz = lrV1.z - lrV0.z, lfAw = lrV1.w - lrV0.w;
            const f32 lfBx = lrV2.x - lrV1.x, lfBy = lrV2.y - lrV1.y, lfBz = lrV2.z - lrV1.z, lfBw = lrV2.w - lrV1.w;
            const f32 lfCx = lfAy * lfBz - lfAz * lfBy;
            const f32 lfCy = lfAz * lfBx - lfAx * lfBz;
            const f32 lfCz = lfAx * lfBy - lfAy * lfBx;
            const f32 lfCw = lfAw * lfBw - lfAw * lfBw;
            const f32 lfInverseLength = 1.0f / std::sqrt(lfCx * lfCx + lfCy * lfCy + lfCz * lfCz);  // vmsum3fp128, vrsqrtefp + 2 NR
            lrResult.mNormal.x = lfCx * lfInverseLength;                // stvx128 +0x30
            lrResult.mNormal.y = lfCy * lfInverseLength;
            lrResult.mNormal.z = lfCz * lfInverseLength;
            lrResult.mNormal.w = lfCw * lfInverseLength;

            lrResult.mau32Tag[0] = lu32SurfaceTag;                      // stvlx128 +0x60 (the splat)
            lrResult.mau32Tag[1] = lu32SurfaceTag;
            lrResult.mau32Tag[2] = lu32SurfaceTag;
            lrResult.mau32Tag[3] = lu32SurfaceTag;
        }
    }

    // @ 0x8283C598 (849)
    s32 IntersectLinePolygonSoupSingleSided(const PolygonSoup&         lPolygonSoup,
                                            const Vector3&             lLineStart,
                                            const Vector3&             lLineEnd,
                                            PolySoupLineNearestResult* lpResultBuffer,
                                            s32                        liMaxResults)
    {
        // 0x8283C5CC..0x8283C61C: quads come first; the triangle count is the u8 difference.
        const u8  lu8NumQuads         = lPolygonSoup.GetNumQuads();
        const u8  lu8NumTriangles     = static_cast<u8>(lPolygonSoup.GetNumPolygons() - lu8NumQuads);
        const u16 lu16NumQuadPairs    = static_cast<u16>(lu8NumQuads >> 1);
        const u16 lu16NumOddQuads     = static_cast<u16>(lu8NumQuads - 2u * lu16NumQuadPairs);
        const u16 lu16NumQuartets     = static_cast<u16>(lu8NumTriangles >> 2);
        const u16 lu16NumOddTriangles = static_cast<u16>(lu8NumTriangles - 4u * lu16NumQuartets);

        // 0x8283C620 UnpackPolygonSoupVertices(stack, soup). The soup's vertex count is a u8.
        Vector3 laVertices[256];
        UnpackPolygonSoupVertices(laVertices, lPolygonSoup);

        // 0x8283C62C GetPolygon(0); every batch advances the byte cursor by 12 per poly.
        const u8* lpPoly = lPolygonSoup.GetPolygon(0);

        s32                        liNumResults = 0;                // r30
        PolySoupLineNearestResult* lpResult     = lpResultBuffer;   // r31

        Vector3 laV0[4], laV1[4], laV2[4];
        f32     lafT[4];

        // ---- quad pairs (0x8283C660..0x8283CA7C) ---------------------------------------------
        for (u16 lu16Pair = 0; lu16Pair < lu16NumQuadPairs; ++lu16Pair)
        {
            const PolygonSoupPoly* lpA = reinterpret_cast<const PolygonSoupPoly*>(lpPoly);
            const PolygonSoupPoly* lpB = reinterpret_cast<const PolygonSoupPoly*>(lpPoly + 12);
            lpPoly += 24;

            const Vector3& lrA0 = laVertices[lpA->mau8VertexIndex[0]];   // v121
            const Vector3& lrA1 = laVertices[lpA->mau8VertexIndex[1]];   // v125
            const Vector3& lrA2 = laVertices[lpA->mau8VertexIndex[2]];   // v124
            const Vector3& lrA3 = laVertices[lpA->mau8VertexIndex[3]];   // v119
            const Vector3& lrB0 = laVertices[lpB->mau8VertexIndex[0]];   // v118
            const Vector3& lrB1 = laVertices[lpB->mau8VertexIndex[1]];   // v123
            const Vector3& lrB2 = laVertices[lpB->mau8VertexIndex[2]];   // v122
            const Vector3& lrB3 = laVertices[lpB->mau8VertexIndex[3]];   // v116

            laV0[0] = lrA0; laV1[0] = lrA1; laV2[0] = lrA2;
            laV0[1] = lrA3; laV1[1] = lrA2; laV2[1] = lrA1;
            laV0[2] = lrB0; laV1[2] = lrB1; laV2[2] = lrB2;
            laV0[3] = lrB3; laV1[3] = lrB2; laV2[3] = lrB1;

            const u32 lu32Hits = IntersectLinePolySoupTriangleSingleSided4(laV0, laV1, laV2, lLineStart, lLineEnd, lafT);

            for (s32 liLane = 0; liLane < 4; ++liLane)
            {
                if ((lu32Hits & (1u << liLane)) == 0)
                {
                    continue;
                }
                const u32 lu32Tag = (liLane < 2) ? lpA->muSurfaceTag : lpB->muSurfaceTag;   // v120 / v117
                WriteSingleSidedHit(*lpResult, laV0[liLane], laV1[liLane], laV2[liLane], lafT[liLane],
                                    lLineStart, lLineEnd, lu32Tag);
                ++lpResult;
                ++liNumResults;
                if (liNumResults >= liMaxResults)
                {
                    return liNumResults;
                }
            }
        }

        // ---- the odd quad (0x8283CA80..0x8283CCAC) -- lanes 0/1 ------------------------------
        if (lu16NumOddQuads != 0)
        {
            const PolygonSoupPoly* lpA = reinterpret_cast<const PolygonSoupPoly*>(lpPoly);
            lpPoly += 12;

            const Vector3& lrA0 = laVertices[lpA->mau8VertexIndex[0]];   // v123
            const Vector3& lrA1 = laVertices[lpA->mau8VertexIndex[1]];   // v125
            const Vector3& lrA2 = laVertices[lpA->mau8VertexIndex[2]];   // v124
            const Vector3& lrA3 = laVertices[lpA->mau8VertexIndex[3]];   // v122

            laV0[0] = lrA0; laV1[0] = lrA1; laV2[0] = lrA2;
            laV0[1] = lrA3; laV1[1] = lrA2; laV2[1] = lrA1;
            laV0[2] = lrA0; laV1[2] = lrA1; laV2[2] = lrA2;   // duplicates, results unread
            laV0[3] = lrA3; laV1[3] = lrA2; laV2[3] = lrA1;

            const u32 lu32Hits = IntersectLinePolySoupTriangleSingleSided4(laV0, laV1, laV2, lLineStart, lLineEnd, lafT);

            for (s32 liLane = 0; liLane < 2; ++liLane)
            {
                if ((lu32Hits & (1u << liLane)) == 0)
                {
                    continue;
                }
                WriteSingleSidedHit(*lpResult, laV0[liLane], laV1[liLane], laV2[liLane], lafT[liLane],
                                    lLineStart, lLineEnd, lpA->muSurfaceTag);            // v121
                ++lpResult;
                ++liNumResults;
                if (liNumResults >= liMaxResults)
                {
                    return liNumResults;
                }
            }
        }

        // ---- triangle quartets (0x8283CCB0..0x8283D148) -- lane k = poly k -----------------
        for (u16 lu16Quartet = 0; lu16Quartet < lu16NumQuartets; ++lu16Quartet)
        {
            const PolygonSoupPoly* lapPoly[4];
            for (s32 liLane = 0; liLane < 4; ++liLane)
            {
                lapPoly[liLane] = reinterpret_cast<const PolygonSoupPoly*>(lpPoly + 12 * liLane);
                laV0[liLane] = laVertices[lapPoly[liLane]->mau8VertexIndex[0]];
                laV1[liLane] = laVertices[lapPoly[liLane]->mau8VertexIndex[1]];
                laV2[liLane] = laVertices[lapPoly[liLane]->mau8VertexIndex[2]];
            }
            lpPoly += 48;

            const u32 lu32Hits = IntersectLinePolySoupTriangleSingleSided4(laV0, laV1, laV2, lLineStart, lLineEnd, lafT);

            for (s32 liLane = 0; liLane < 4; ++liLane)
            {
                if ((lu32Hits & (1u << liLane)) == 0)
                {
                    continue;
                }
                WriteSingleSidedHit(*lpResult, laV0[liLane], laV1[liLane], laV2[liLane], lafT[liLane],
                                    lLineStart, lLineEnd, lapPoly[liLane]->muSurfaceTag);   // v117|v118, v111, v110, v109
                ++lpResult;
                ++liNumResults;
                if (liNumResults >= liMaxResults)
                {
                    return liNumResults;
                }
            }
        }

        // ---- the odd triangles (0x8283D14C..0x8283D2C4) -- one per call, lane 0 -------------
        for (u16 lu16Odd = 0; lu16Odd < lu16NumOddTriangles; ++lu16Odd)
        {
            const PolygonSoupPoly* lpP = reinterpret_cast<const PolygonSoupPoly*>(lpPoly);
            lpPoly += 12;

            for (s32 liLane = 0; liLane < 4; ++liLane)
            {
                laV0[liLane] = laVertices[lpP->mau8VertexIndex[0]];   // v124
                laV1[liLane] = laVertices[lpP->mau8VertexIndex[1]];   // v125
                laV2[liLane] = laVertices[lpP->mau8VertexIndex[2]];   // v123
            }

            const u32 lu32Hits = IntersectLinePolySoupTriangleSingleSided4(laV0, laV1, laV2, lLineStart, lLineEnd, lafT);

            if ((lu32Hits & 1u) != 0)
            {
                WriteSingleSidedHit(*lpResult, laV0[0], laV1[0], laV2[0], lafT[0],
                                    lLineStart, lLineEnd, lpP->muSurfaceTag);            // v122|v121
                ++lpResult;
                ++liNumResults;
                if (liNumResults >= liMaxResults)
                {
                    return liNumResults;
                }
            }
        }

        return liNumResults;   // 0x8283D2C8 `mr r3, r30`
    }
}

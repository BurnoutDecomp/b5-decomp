// ============================================================================
// GameShared/GameClasses/Geometric/Intersection/CgsLineTests.cpp
//
// CgsGeometric line/box intersection free functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. The X360 bodies are dense hand-vectorised VMX; these
// are semantic per-lane lowerings (the project's established VMX precedent --
// see CgsTriangleBox.cpp / CgsAxisAlignedBox::ContainsPoint).
//
// This batch bodies the two verified functions:
//   TestAxisAlignedBoxAxisAlignedBox @ 0x82812460  (store-for-store)
//   TestLineStartEndAxisAlignedBox   @ 0x82812498  (per-lane VMX lowering; re-read and
//                                                   corrected 2026-09-24, FX-GEOMETRIC)
// and the loose octree's three four-lane line tests, inlined on the X360 into
// LooseOctree::LineTestRecursive @0x828BCF50 (2026-09-25, FX-FOLLOWUPS):
//   TestLineSphere4 (:172 Mask4 / :305 int32_t results), TestLineBoundingBoxAgainstAxisAlignedBox4 (:766)
// ============================================================================

#include "GameShared/GameClasses/Geometric/Intersection/CgsLineTests.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cmath>     // std::signbit (the vmaxfp / vminfp zero ordering); std::fma (TestLineSphere4's vmaddfp)
#include <cstdint>   // uintptr_t (TestLineSphere4's :308 alignment assert)
#include <cstring>   // std::memcpy (the Mask4 lanes)

namespace CgsGeometric
{
    // ------------------------------------------------------------------------
    // TestAxisAlignedBoxAxisAlignedBox @ 0x82812460
    //
    //   li        r11, 0x10
    //   lvx128    v0,  r0, r4    ; v0  = lrBoxB.mMin   (r4+0x00)
    //   lvx128    v13, r0, r3    ; v13 = lrBoxA.mMin   (r3+0x00)
    //   lvx128    v12, r3, r11   ; v12 = lrBoxA.mMax   (r3+0x10)
    //   lvx128    v11, r4, r11   ; v11 = lrBoxB.mMax   (r4+0x10)
    //   vcmpgefp  v0,  v12, v0   ; v0  = (A.max >= B.min) per lane
    //   vcmpgefp  v13, v11, v13  ; v13 = (B.max >= A.min) per lane
    //   vand      v0,  v13, v0   ; v0  = per-axis overlap
    //   vpermwi128 v13, v0, 0x4B ; cross-lane rotate
    //   vpermwi128 v12, v0, 0x87 ; cross-lane rotate
    //   vand      v0,  v0,  v13  ; AND lanes together ...
    //   vand      v0,  v0,  v12  ; ... reducing x,y,z to one boolean
    //   vspltw    v1,  v0,  0    ; broadcast the all-axes-overlap result
    //   blr
    //
    // Standard separating-axis AABB/AABB overlap test: two boxes overlap iff on
    // every axis A.max >= B.min AND B.max >= A.min. Both edges are non-strict
    // (vcmpgefp), so boxes that merely touch (share a face) count as overlapping.
    // The two vpermwi128 rotates + two vand fold the three per-axis lanes into a
    // single all-true predicate. A NaN coordinate makes its VMX compare false,
    // matching the scalar >= here. The w lane is unused.
    // ------------------------------------------------------------------------
    bool TestAxisAlignedBoxAxisAlignedBox(const AxisAlignedBox& lrBoxA, const AxisAlignedBox& lrBoxB)
    {
        return lrBoxA.mMax.x >= lrBoxB.mMin.x && lrBoxB.mMax.x >= lrBoxA.mMin.x    // x axis
            && lrBoxA.mMax.y >= lrBoxB.mMin.y && lrBoxB.mMax.y >= lrBoxA.mMin.y    // y axis
            && lrBoxA.mMax.z >= lrBoxB.mMin.z && lrBoxB.mMax.z >= lrBoxA.mMin.z;   // z axis
    }

    namespace
    {
        // ---- the console's reciprocal ----------------------------------------------------------
        // `vrefp128 v0, v126` then THREE Newton-Raphson steps, each `vnmsubfp128 e = 1 - d*x`
        // followed by `vmaddfp x = x*e + x` (0x828124FC..0x82812538; the same eleven instructions
        // are inlined at 0x82843FE4 in PolygonSoupListSpatialMap::RunQuery(const Line&) and at
        // 0x82812D64 in BaseCollisionGenerator::CollideLineAgainstPolySoupList). The PS3 twin
        // spells it VecRecipEst + one inline step + two CgsNumeric::NewtonRaphsonReciprocalIteratation
        // (DWARF CgsReciprocal.h:42 / :152 NewtonRaphsonReciprocal3).
        //
        // PC LOWERING: the estimate is the exact quotient and the console's three refinement steps
        // then run AS WRITTEN. That keeps the one property a decision here depends on: a ZERO
        // direction lane refines to NaN (e = 1 - 0*inf = NaN), not to the +/-inf a bare 1/d gives.
        // (Finite lanes land within an ulp of the console's.)
        // MOVE-WHEN CgsNumeric/CgsReciprocal.h has a home in the tree (Numeric/** is not this lane's).
        inline f32 NewtonRaphsonReciprocal3(f32 lfValue)
        {
            f32 lfReciprocal = 1.0f / lfValue;                            // vrefp128 (the estimate)
            for (s32 liStep = 0; liStep < 3; ++liStep)
            {
                const f32 lfError = 1.0f - lfValue * lfReciprocal;       // vnmsubfp128 vE, vD, vX
                lfReciprocal      = lfReciprocal * lfError + lfReciprocal; // vmaddfp vX, vX, vX, vE
            }
            return lfReciprocal;
        }

        // `vcmpgefp P, min` AND `vnot(vcmpgtfp P, max)` -- note the asymmetry on a NaN lane: the
        // first compare is false, so a NaN coordinate is NEVER inside, while `!(P > max)` alone
        // would have let it through.
        inline bool IsWithinSlab(f32 lfValue, f32 lfMin, f32 lfMax)
        {
            return (lfValue >= lfMin) && !(lfValue > lfMax);
        }

        // `vcmpgefp one, t` AND `vnot(vcmpgtfp zero, t)` -- t in [0, 1]; a NaN t is rejected by
        // the first compare.
        inline bool IsOnSegment(f32 lfT)
        {
            return (1.0f >= lfT) && !(0.0f > lfT);
        }
    }

    // ------------------------------------------------------------------------
    // TestLineStartEndAxisAlignedBox @ 0x82812498 (RE-READ 2026-09-24, crash parity FX-GEOMETRIC)
    //
    // Segment-vs-AABB via the reciprocal slab method. r3 = the box, v1 = start, v2 = end
    // (`vmr128 v127, v1 ; vmr128 v124, v2 ; mr r23, r3`); returns the hit mask splatted in v1.
    // X360 callers: PolygonSoupTesterJob::LineTestNearestSS @0x829165D8 out of line, and the SAME
    // body inlined per node in PolygonSoupListSpatialMap::RunQuery(const Line&) @0x82843E98 and per
    // leaf in BaseCollisionGenerator::CollideLineAgainstPolySoupList's long arm @0x82812D64 (the
    // DWARF names the inline in RunQuery(const Line&); the PS3 keeps the slab half out of line as
    // TestLineAxisAlignedBox(box, start, end, reciprocal) @0xB13058, CgsLineTests.cpp:431).
    //
    //   0x828124D0  d = end - start                             (vsubfp128 v126, v124, v127)
    //   0x828124FC  r = NewtonRaphsonReciprocal3(d)             (vrefp128 + 3 NR steps, above)
    //   0x82812544  r.x == 0 -> "Line reciprocal X is 0\n"      :441 (0x1B9), non-gating
    //   0x82812600  r.y == 0 -> "Line reciprocal Y is 0\n"      :442 (0x1BA)
    //   0x8281269C  r.z == 0 -> "Line reciprocal Z is 0\n"      :443 (0x1BB)
    //   0x82812714  per lane: tmin = r*(min - start), tmax = r*(max - start) (vmulfp128);
    //               startIn = (start >= min) & !(start > max); endIn likewise for the end
    //   0x8281274C  per face: P = d*t + start (vmaddcfp128, all lanes, one t splat), then
    //               face = onSegment(t) & inside(P) on the OTHER two axes
    //   0x828128BC  v1 = startIn.xyz | endIn.xyz | Xmin | Xmax | Ymin | Ymax | Zmin | Zmax
    //
    // ⛔ CORRECTED 2026-09-24 (two divergences from the asm, both in this body since 2026-07-06):
    //   (1) the END-INSIDE term was missing -- the asm ORs `(end >= min) & !(end > max)` over xyz
    //       (v2/v8 -> v9 @0x82812724/0x82812734/0x82812880) with the start term and the faces;
    //   (2) the reciprocal was a bare 1/d, so a zero direction lane gave +/-inf where the console's
    //       refinement gives NaN. No accept/reject decision moved on the inf path (an inf or NaN t
    //       fails onSegment either way), but the body now computes the console's lanes.
    // The t-range and inside compares are now the console's own (a NaN t / NaN coordinate fails
    // exactly where the asm's compare fails, not one compare later).
    // FireAssert's file/line are dropped per convention; the messages carry their trailing \n.
    // ------------------------------------------------------------------------
    bool TestLineStartEndAxisAlignedBox(const Vector4& lvStart, const Vector4& lvEnd, const AxisAlignedBox& lrBox)
    {
        const f32 lafStart[3] = { lvStart.x, lvStart.y, lvStart.z };
        const f32 lafEnd[3]   = { lvEnd.x, lvEnd.y, lvEnd.z };
        const f32 lafMin[3]   = { lrBox.mMin.x, lrBox.mMin.y, lrBox.mMin.z };
        const f32 lafMax[3]   = { lrBox.mMax.x, lrBox.mMax.y, lrBox.mMax.z };

        f32 lafDirection[3];
        f32 lafReciprocal[3];
        for (s32 liAxis = 0; liAxis < 3; ++liAxis)
        {
            lafDirection[liAxis]  = lafEnd[liAxis] - lafStart[liAxis];
            lafReciprocal[liAxis] = NewtonRaphsonReciprocal3(lafDirection[liAxis]);
        }

        CGS_ASSERT(!(lafReciprocal[0] == 0.0f), "Line reciprocal X is 0\n");   // :441
        CGS_ASSERT(!(lafReciprocal[1] == 0.0f), "Line reciprocal Y is 0\n");   // :442
        CGS_ASSERT(!(lafReciprocal[2] == 0.0f), "Line reciprocal Z is 0\n");   // :443

        // The two endpoints, each inside iff inside on all three axes.
        bool lbStartInside = true;
        bool lbEndInside   = true;
        for (s32 liAxis = 0; liAxis < 3; ++liAxis)
        {
            lbStartInside = lbStartInside && IsWithinSlab(lafStart[liAxis], lafMin[liAxis], lafMax[liAxis]);
            lbEndInside   = lbEndInside   && IsWithinSlab(lafEnd[liAxis],   lafMin[liAxis], lafMax[liAxis]);
        }

        // The six faces: axis a's min plane at tmin[a], its max plane at tmax[a]; the crossing
        // point must lie inside the box on the two axes the face does not fix.
        bool lbFaceHit = false;
        for (s32 liAxis = 0; liAxis < 3; ++liAxis)
        {
            const f32 lafT[2] =
            {
                lafReciprocal[liAxis] * (lafMin[liAxis] - lafStart[liAxis]),   // tmin (vmulfp128 v12)
                lafReciprocal[liAxis] * (lafMax[liAxis] - lafStart[liAxis]),   // tmax (vmulfp128 v11)
            };

            for (s32 liFace = 0; liFace < 2; ++liFace)
            {
                const f32 lfT   = lafT[liFace];
                bool      lbHit = IsOnSegment(lfT);
                for (s32 liOther = 0; liOther < 3; ++liOther)
                {
                    if (liOther == liAxis)
                    {
                        continue;
                    }
                    const f32 lfCrossing = lafDirection[liOther] * lfT + lafStart[liOther];   // vmaddcfp128
                    lbHit = lbHit && IsWithinSlab(lfCrossing, lafMin[liOther], lafMax[liOther]);
                }
                lbFaceHit = lbFaceHit || lbHit;
            }
        }

        return lbStartInside || lbEndInside || lbFaceHit;
    }

    // ============================================================================================
    // THE LOOSE OCTREE'S FOUR-LANE LINE TESTS (2026-09-25, crash parity FX-FOLLOWUPS).
    // Inlined on the X360 into LooseOctree::LineTestRecursive @0x828BCF50; the addresses below are
    // the inlined copies there. Per-lane lowering of the VMX, in the console's operation order.
    // ============================================================================================
    namespace
    {
        // A VMX compare-result lane: all ones where the predicate holds, zero where it does not.
        inline void SetMask4Lane(Vector4& lrMask, s32 liLane, bool lbPassed)
        {
            const u32 luBits = lbPassed ? 0xFFFFFFFFu : 0u;
            std::memcpy(&(&lrMask.x)[liLane], &luBits, sizeof(u32));
        }

        // One lane of TestLineSphere4 (the banner below has the addresses).
        inline bool LineSphereLaneHit(const Sphere& lrSphere, const Vector3& lrStart,
                                      const Vector3& lrDirection, f32 lfLength)
        {
            const Vector4& lrCentre  = lrSphere.mPositionRadius;
            const f32      lfRadius2 = lrCentre.w * lrCentre.w;                          // vmulfp128 v6, v6, v6

            // ROUNDING (ROUNDING_RULE.md rule 3, 2026-09-25 -- REVIEW-J on 335639ce): every `vmaddfp` here rounds
            // ONCE (std::fma); the vmulfp128 / vsubfp steps round separately (rule 4). All eleven fused sites
            // of the :172 copy are below (the :305 copy at 0x828BD3AC.. is the same sequence); until this
            // change each was written a*b + c, rounding the product first.

            // The far end of the segment, e = L * d + s.
            const f32 lfEndX = std::fma(lfLength, lrDirection.x, lrStart.x);             // vmaddfp v1  0x828BD1B0
            const f32 lfEndY = std::fma(lfLength, lrDirection.y, lrStart.y);             // vmaddfp v31 0x828BD1AC
            const f32 lfEndZ = std::fma(lfLength, lrDirection.z, lrStart.z);             // vmaddfp v2  0x828BD1A8

            // The centre relative to the start, and its projection on the unit direction.
            const f32 lfDx = lrCentre.x - lrStart.x;                                     // vsubfp v11
            const f32 lfDy = lrCentre.y - lrStart.y;                                     // vsubfp v12
            const f32 lfDz = lrCentre.z - lrStart.z;                                     // vsubfp v10
            const f32 lfT  = std::fma(lrDirection.z, lfDz,                               // vmaddfp 0x828BD1CC
                             std::fma(lrDirection.x, lfDx,                               // vmaddfp 0x828BD1C4
                                      lrDirection.y * lfDy));                            // vmulfp128 0x828BD1B4

            // The perpendicular offset (C - s) - t * d (vmulfp128 0x828BD1D0..0x828BD1D8, then vsubfp
            // 0x828BD1E4 / 0x828BD1EC / 0x828BD1F0 -- NOT fused), and the three squared distances, each
            // y*y (vmulfp128) then x*x + that, then z*z + that (two vmaddfp).
            const f32 lfTx = lfT * lrDirection.x;
            const f32 lfTy = lfT * lrDirection.y;
            const f32 lfTz = lfT * lrDirection.z;
            const f32 lfPx = lfDx - lfTx;
            const f32 lfPy = lfDy - lfTy;
            const f32 lfPz = lfDz - lfTz;
            const f32 lfPerpendicular2 = std::fma(lfPz, lfPz, std::fma(lfPx, lfPx, lfPy * lfPy));   // 0x828BD214 / 0x828BD200
            const f32 lfStart2         = std::fma(lfDz, lfDz, std::fma(lfDx, lfDx, lfDy * lfDy));   // 0x828BD208 / 0x828BD1F8
            const f32 lfEx = lrCentre.x - lfEndX;                                        // vsubfp 0x828BD1C0
            const f32 lfEy = lrCentre.y - lfEndY;                                        // vsubfp 0x828BD1BC
            const f32 lfEz = lrCentre.z - lfEndZ;                                        // vsubfp 0x828BD1B8
            const f32 lfEnd2           = std::fma(lfEz, lfEz, std::fma(lfEx, lfEx, lfEy * lfEy));   // 0x828BD20C / 0x828BD1FC

            // vcmpgefp128 (t >= 0) AND vnot(vcmpgtfp t > L): the foot of the perpendicular lies on the
            // segment. Each distance term is vnot(vcmpgtfp d2 > R2), so a NaN distance reads as inside.
            const bool lbOnSegment = (lfT >= 0.0f) && !(lfT > lfLength);
            return (lbOnSegment && !(lfPerpendicular2 > lfRadius2))
                || !(lfStart2 > lfRadius2)
                || !(lfEnd2 > lfRadius2);
        }

        // `vmaxfp` / `vminfp` of the two slab parameters of one axis (0x828BD99C / 0x828BD9A4 / 0x828BD9B8 and
        // 0x828BD9A0 / 0x828BD9A8 / 0x828BD9BC) -- the VMX result, not a C select's: a NaN operand gives a NaN
        // (vA's when both are), and +0 orders above -0. A NaN slab parameter therefore makes far AND near NaN,
        // and the axis's `1 >= near` lane (vcmpgefp128 0x828BD9B0) fails, as on the console. (Until 2026-09-25
        // these were `a > b ? a : b` / `a < b ? a : b`, which answer the OTHER operand for a NaN vA.)
        inline f32 SlabFar(f32 lfA, f32 lfB)
        {
            if (lfA != lfA) return lfA;
            if (lfB != lfB) return lfB;
            if (lfA == lfB) return std::signbit(lfA) ? lfB : lfA;   // +0 is the larger zero
            return (lfA > lfB) ? lfA : lfB;
        }
        inline f32 SlabNear(f32 lfA, f32 lfB)
        {
            if (lfA != lfA) return lfA;
            if (lfB != lfB) return lfB;
            if (lfA == lfB) return std::signbit(lfA) ? lfA : lfB;   // -0 is the smaller zero
            return (lfA < lfB) ? lfA : lfB;
        }
    }

    // ------------------------------------------------------------------------
    // TestLineSphere4 -- DWARF CgsLineTests.cpp:172 (`const Mask4`), inlined in LooseOctree::
    // LineTestRecursive's full-batch arm at 0x828BD114..0x828BD238.
    //
    // The four spheres are transposed to structure-of-arrays (vmrghw128 / vmrglw128 pairs), the start
    // and direction splatted per axis, and the length lane rebuilt by vperm128 (unk_82CDA3C0 /
    // unk_82CDA400) + vsldoi 8 into {L.x, L.y, L.z, L.w} -- lane k tests with lane k of the length.
    // Per lane, with C the centre, R the radius, s the start, d the unit direction, L the length:
    //   e     = L * d + s                                        0x828BD1A8..0x828BD1B0  vmaddfp
    //   t     = (d.y * (C.y - s.y) + d.x * (C.x - s.x)) + d.z * (C.z - s.z)   0x828BD1B4..0x828BD1CC
    //   p     = (C - s) - t * d                                  0x828BD1D0..0x828BD1F0
    //   hit   = ((t >= 0) & !(t > L) & !(|p|^2 > R^2))            0x828BD1DC / 0x828BD1E0 / 0x828BD220
    //         | !(|C - s|^2 > R^2)                               0x828BD218 (the start inside)
    //         | !(|C - e|^2 > R^2)                               0x828BD21C (the end inside)
    // i.e. the segment's closest point to the centre lies within R, taken at the foot of the
    // perpendicular when that falls on the segment and otherwise at whichever end point is inside.
    // Every squared length is summed y + x first, then z, as the fused multiply-adds do.
    // ------------------------------------------------------------------------
    const Vector4 TestLineSphere4(const Sphere& lrSphere0, const Sphere& lrSphere1,
                                  const Sphere& lrSphere2, const Sphere& lrSphere3,
                                  Vector3 lLineStart, Vector3 lLineDirection, VecFloat lLineLength)
    {
        const Sphere* const lapSpheres[4] = { &lrSphere0, &lrSphere1, &lrSphere2, &lrSphere3 };

        Vector4 lResult;
        for (s32 liLane = 0; liLane < 4; ++liLane)
        {
            SetMask4Lane(lResult, liLane,
                         LineSphereLaneHit(*lapSpheres[liLane], lLineStart, lLineDirection,
                                           (&lLineLength.x)[liLane]));
        }
        return lResult;
    }

    // ------------------------------------------------------------------------
    // TestLineSphere4 -- DWARF CgsLineTests.cpp:305 (`void`, int32_t* results), inlined in
    // LineTestRecursive's remainder arm at 0x828BD334..0x828BD4B8. The alignment assert comes first
    // (0x828BD334..0x828BD3A8: `srawi 4 ; addze ; slwi 4 ; subf.` is the address modulo 16,
    // "Results must be 16 byte aligned\n", :308 == 0x134); the lane math at 0x828BD3AC..0x828BD4B4 is
    // the :172 form's instruction for instruction (other registers), and `stvx128 v0` at 0x828BD4B8
    // stores the four masks.
    // ------------------------------------------------------------------------
    void TestLineSphere4(const Sphere& lrSphere0, const Sphere& lrSphere1,
                         const Sphere& lrSphere2, const Sphere& lrSphere3,
                         Vector3 lLineStart, Vector3 lLineDirection, VecFloat lLineLength,
                         s32* lpiResults)
    {
        CGS_ASSERT((reinterpret_cast<uintptr_t>(lpiResults) & 15u) == 0,
                   "Results must be 16 byte aligned\n");                                   // :308

        const Vector4 lMask = TestLineSphere4(lrSphere0, lrSphere1, lrSphere2, lrSphere3,
                                              lLineStart, lLineDirection, lLineLength);
        std::memcpy(lpiResults, &lMask, 4 * sizeof(s32));                                 // stvx128 v0
    }

    // ------------------------------------------------------------------------
    // TestLineBoundingBoxAgainstAxisAlignedBox4 -- DWARF CgsLineTests.cpp:766 (`const Mask4`), inlined
    // in LineTestRecursive's child walk at 0x828BD700..0x828BD9E8.
    //
    //   0x828BD700..0x828BD904  per axis, the reciprocal lane splatted and compared (`vcmpeqfp.`) with
    //                           0.0 (flt_82001CC0): "Line reciprocal X is 0\n" :769 (0x301), Y :770
    //                           (0x302), Z :771 (0x303). Non-gating.
    //   0x828BD908..0x828BD998  the four boxes transposed to SoA; per box and axis
    //                           t_max = r * (max - s), t_min = r * (min - s)       (vmulfp128)
    //   0x828BD99C..0x828BD9BC  far = vmaxfp(t_max, t_min), near = vminfp(t_max, t_min)
    //   0x828BD9AC..0x828BD9E8  axis ok = vnot(vcmpgtfp128 0 > far) AND vcmpgefp128 (1 >= near);
    //                           lane = X ok AND Y ok AND Z ok
    // The axes are tested INDEPENDENTLY -- there is no max(near) <= min(far) -- so a lane passes when
    // the segment's bounding box meets the box, whether or not the segment itself does (the same
    // per-axis form as TestLineAgainstNodeBoundingBox @0x828B0FC8). A segment that merely touches a
    // face (far == 0 or near == 1) counts. Only the start (the
    // caller's `lvx128 v127, r0, r20`, params +0x00) and the reciprocal (`lvx128 v12, r20, 0x30`)
    // reach the body; the middle vector is part of the signature and is not read.
    // (vmaxfp / vminfp answer NaN for a NaN operand; SlabFar / SlabNear keep that, so a NaN t fails its axis.)
    // ------------------------------------------------------------------------
    const Vector4 TestLineBoundingBoxAgainstAxisAlignedBox4(const AxisAlignedBox& lrBox0,
                                                            const AxisAlignedBox& lrBox1,
                                                            const AxisAlignedBox& lrBox2,
                                                            const AxisAlignedBox& lrBox3,
                                                            Vector3 lLineStart, Vector3 /*lLineEnd*/,
                                                            Vector3 lLineReciprocal)
    {
        CGS_ASSERT(!(lLineReciprocal.x == 0.0f), "Line reciprocal X is 0\n");   // :769
        CGS_ASSERT(!(lLineReciprocal.y == 0.0f), "Line reciprocal Y is 0\n");   // :770
        CGS_ASSERT(!(lLineReciprocal.z == 0.0f), "Line reciprocal Z is 0\n");   // :771

        const AxisAlignedBox* const lapBoxes[4] = { &lrBox0, &lrBox1, &lrBox2, &lrBox3 };
        const f32 lafStart[3]      = { lLineStart.x, lLineStart.y, lLineStart.z };
        const f32 lafReciprocal[3] = { lLineReciprocal.x, lLineReciprocal.y, lLineReciprocal.z };

        Vector4 lResult;
        for (s32 liLane = 0; liLane < 4; ++liLane)
        {
            const AxisAlignedBox& lrBox = *lapBoxes[liLane];
            const f32 lafMin[3] = { lrBox.mMin.x, lrBox.mMin.y, lrBox.mMin.z };
            const f32 lafMax[3] = { lrBox.mMax.x, lrBox.mMax.y, lrBox.mMax.z };

            bool lbCrosses = true;
            for (s32 liAxis = 0; liAxis < 3; ++liAxis)
            {
                const f32 lfTMax = lafReciprocal[liAxis] * (lafMax[liAxis] - lafStart[liAxis]);
                const f32 lfTMin = lafReciprocal[liAxis] * (lafMin[liAxis] - lafStart[liAxis]);
                const f32 lfFar  = SlabFar(lfTMax, lfTMin);
                const f32 lfNear = SlabNear(lfTMax, lfTMin);
                lbCrosses = lbCrosses && !(0.0f > lfFar) && (1.0f >= lfNear);
            }
            SetMask4Lane(lResult, liLane, lbCrosses);
        }
        return lResult;
    }
}

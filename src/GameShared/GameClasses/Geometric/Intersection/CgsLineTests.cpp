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
// ============================================================================

#include "GameShared/GameClasses/Geometric/Intersection/CgsLineTests.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

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
}

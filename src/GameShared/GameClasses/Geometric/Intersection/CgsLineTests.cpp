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
//   TestLineStartEndAxisAlignedBox   @ 0x82812498  (semantic VMX lowering)
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
        // vrefp128 + Newton-Raphson refinement converges to the exact 1/x
        // (same recip convention as every other VMX reconstruction in this
        // project). Per-lane reciprocal of the segment direction.
        inline f32 LineReciprocal(f32 lfValue)
        {
            return 1.0f / lfValue;
        }
    }

    // ------------------------------------------------------------------------
    // TestLineStartEndAxisAlignedBox @ 0x82812498
    //
    // Segment-vs-AABB intersection via the reciprocal-direction slab method.
    // The X360 body is dense hand-vectorised VMX (SoA over the 3 axes); this is
    // a SEMANTIC reconstruction of the recovered intent -- portable per-lane
    // float slab math -- preserving the observed computation and the three
    // reciprocal-is-zero asserts, not a literal per-VMX-register translation
    // (per this project's established VMX semantic-lowering precedent; see
    // CgsTriangleBox.cpp / CgsAxisAlignedBox::ContainsPoint).
    //
    // Steps proven by the asm:
    //   dir      = lvEnd - lvStart                         (vsubfp128 v126)
    //   invDir   = 1 / dir  per axis  (vrefp128 + Newton-Raphson refine)
    //   assert invDir.x/y/z != 0  ->  'Line reciprocal X/Y/Z is 0\n'
    //   tMin[a]  = (box.min[a] - start[a]) * invDir[a]     (vsubfp128 v12 / vmulfp128)
    //   tMax[a]  = (box.max[a] - start[a]) * invDir[a]     (vsubfp128 v7  / vmulfp128)
    //   for each of the 6 axis planes: hit = start + dir * t   (vmaddcfp128)
    //   accept the plane iff its t in [0,1] AND the hit point lies within the
    //   box's other two axes (vcmpgtfp/vcmpgefp vs box.min/box.max), then OR
    //   all faces together (the big vand/vor reduction tree).
    //
    // FireAssert's file-path + line-number args are dropped per convention; the
    // rodata assert messages are reproduced verbatim (they carry a trailing \n).
    // ------------------------------------------------------------------------
    bool TestLineStartEndAxisAlignedBox(const Vector4& lvStart, const Vector4& lvEnd, const AxisAlignedBox& lrBox)
    {
        const f32 lfDirX = lvEnd.x - lvStart.x;
        const f32 lfDirY = lvEnd.y - lvStart.y;
        const f32 lfDirZ = lvEnd.z - lvStart.z;

        const f32 lfInvX = LineReciprocal(lfDirX);
        const f32 lfInvY = LineReciprocal(lfDirY);
        const f32 lfInvZ = LineReciprocal(lfDirZ);

        // The three reciprocal lanes must be finite: the asm branches to a fired
        // assert when a reciprocal lane equals 0 (a zero direction component).
        CGS_ASSERT(lfInvX != 0.0f, "Line reciprocal X is 0\n");
        CGS_ASSERT(lfInvY != 0.0f, "Line reciprocal Y is 0\n");
        CGS_ASSERT(lfInvZ != 0.0f, "Line reciprocal Z is 0\n");

        // Slab parameters: entry/exit t for each axis. tMin/tMax here are the
        // (unordered) hits of the min-corner and max-corner planes.
        const f32 lfTMinX = (lrBox.mMin.x - lvStart.x) * lfInvX;
        const f32 lfTMinY = (lrBox.mMin.y - lvStart.y) * lfInvY;
        const f32 lfTMinZ = (lrBox.mMin.z - lvStart.z) * lfInvZ;
        const f32 lfTMaxX = (lrBox.mMax.x - lvStart.x) * lfInvX;
        const f32 lfTMaxY = (lrBox.mMax.y - lvStart.y) * lfInvY;
        const f32 lfTMaxZ = (lrBox.mMax.z - lvStart.z) * lfInvZ;

        // Test one axis-aligned face: the crossing point start + dir*t must lie
        // within the box on the other two axes, and t must be on the segment
        // [0,1]. (u0/u1 are the two axes orthogonal to the face's axis.)
        struct FaceTest
        {
            static bool Hits(f32 lfT,
                             f32 lfStartU0, f32 lfDirU0, f32 lfMinU0, f32 lfMaxU0,
                             f32 lfStartU1, f32 lfDirU1, f32 lfMinU1, f32 lfMaxU1)
            {
                if (lfT < 0.0f || lfT > 1.0f)
                {
                    return false;
                }
                const f32 lfHitU0 = lfStartU0 + lfDirU0 * lfT;
                const f32 lfHitU1 = lfStartU1 + lfDirU1 * lfT;
                return lfHitU0 >= lfMinU0 && lfHitU0 <= lfMaxU0
                    && lfHitU1 >= lfMinU1 && lfHitU1 <= lfMaxU1;
            }
        };

        // X-normal faces (crossing point tested against Y and Z ranges).
        const bool lbFaceXMin = FaceTest::Hits(lfTMinX,
            lvStart.y, lfDirY, lrBox.mMin.y, lrBox.mMax.y,
            lvStart.z, lfDirZ, lrBox.mMin.z, lrBox.mMax.z);
        const bool lbFaceXMax = FaceTest::Hits(lfTMaxX,
            lvStart.y, lfDirY, lrBox.mMin.y, lrBox.mMax.y,
            lvStart.z, lfDirZ, lrBox.mMin.z, lrBox.mMax.z);

        // Y-normal faces (tested against X and Z ranges).
        const bool lbFaceYMin = FaceTest::Hits(lfTMinY,
            lvStart.x, lfDirX, lrBox.mMin.x, lrBox.mMax.x,
            lvStart.z, lfDirZ, lrBox.mMin.z, lrBox.mMax.z);
        const bool lbFaceYMax = FaceTest::Hits(lfTMaxY,
            lvStart.x, lfDirX, lrBox.mMin.x, lrBox.mMax.x,
            lvStart.z, lfDirZ, lrBox.mMin.z, lrBox.mMax.z);

        // Z-normal faces (tested against X and Y ranges).
        const bool lbFaceZMin = FaceTest::Hits(lfTMinZ,
            lvStart.x, lfDirX, lrBox.mMin.x, lrBox.mMax.x,
            lvStart.y, lfDirY, lrBox.mMin.y, lrBox.mMax.y);
        const bool lbFaceZMax = FaceTest::Hits(lfTMaxZ,
            lvStart.x, lfDirX, lrBox.mMin.x, lrBox.mMax.x,
            lvStart.y, lfDirY, lrBox.mMin.y, lrBox.mMax.y);

        // A start endpoint already inside the box is also an intersection
        // (the asm's start>=min / start<=max lane compares).
        const bool lbStartInside =
               lvStart.x >= lrBox.mMin.x && lvStart.x <= lrBox.mMax.x
            && lvStart.y >= lrBox.mMin.y && lvStart.y <= lrBox.mMax.y
            && lvStart.z >= lrBox.mMin.z && lvStart.z <= lrBox.mMax.z;

        return lbStartInside
            || lbFaceXMin || lbFaceXMax
            || lbFaceYMin || lbFaceYMax
            || lbFaceZMin || lbFaceZMax;
    }
}

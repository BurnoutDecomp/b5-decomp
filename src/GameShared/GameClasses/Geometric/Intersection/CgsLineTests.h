#ifndef CGS_LINE_TESTS_H
#define CGS_LINE_TESTS_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector4 alias (rw::math::vpu::Vector4)
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"   // Sphere (TestLineSphere4)

// ============================================================================
// GameShared/GameClasses/Geometric/Intersection/CgsLineTests.h
//
// CgsGeometric line/box intersection free functions. This X360 TU is
// d:\p4\b5_main\burnout\main\code\gameshared\gameclasses\geometric\intersection\
// CgsLineTests.cpp -- proven by the FireAssert file-path rodata inside
// TestLineStartEndAxisAlignedBox. Free functions in the CgsGeometric namespace
// (no class shape / no members).
// ============================================================================

namespace CgsGeometric
{
    // TestAxisAlignedBoxAxisAlignedBox @ 0x82812460 -- separating-axis
    // AABB/AABB overlap test (non-strict edges; touching counts as overlap).
    bool TestAxisAlignedBoxAxisAlignedBox(const AxisAlignedBox& lrBoxA,
                                          const AxisAlignedBox& lrBoxB);

    // TestLineStartEndAxisAlignedBox @ 0x82812498 -- segment(start,end)-vs-AABB
    // intersection via the reciprocal-direction slab method (6 faces + either
    // endpoint inside). Asserts each direction reciprocal is non-zero. Inlined on
    // the console into PolygonSoupListSpatialMap::RunQuery(const Line&) and the
    // long arm of BaseCollisionGenerator::CollideLineAgainstPolySoupList.
    bool TestLineStartEndAxisAlignedBox(const Vector4& lvStart,
                                        const Vector4& lvEnd,
                                        const AxisAlignedBox& lrBox);

    // ---- the loose octree's four-lane line tests (2026-09-25, crash parity FX-FOLLOWUPS) ----------------
    // All three are INLINED on the X360 into LooseOctree::LineTestRecursive @0x828BCF50 (the ARTIST image
    // has no out-of-line copy). The PS3 keeps them out of line (DecFIGS CgsGeometric::TestLineSphere4
    // @0xCA6DDC, TestLineBoundingBoxAgainstAxisAlignedBox4 @0xCAE954), which fixes the names and the
    // argument lists; the DWARF places them at CgsLineTests.cpp:172 / :305 / :766. A DWARF `Mask4` is the
    // VMX compare result -- one 32-bit mask per lane, all ones where the lane passed -- carried here in a
    // Vector4, the convention Triangle4::Mask4 already uses (CgsTriangle4.h).

    // TestLineSphere4 (CgsLineTests.cpp:172). Lane k: does the segment start + t * direction, t in
    // [0, length] (direction a unit vector, length lane k of lLineLength), touch sphere k?
    const Vector4 TestLineSphere4(const Sphere& lrSphere0, const Sphere& lrSphere1,
                                  const Sphere& lrSphere2, const Sphere& lrSphere3,
                                  Vector3 lLineStart, Vector3 lLineDirection, VecFloat lLineLength);

    // TestLineSphere4 (CgsLineTests.cpp:305). The same four lanes, stored as four s32 masks into
    // lpiResults, which must be 16-byte aligned (:308).
    void TestLineSphere4(const Sphere& lrSphere0, const Sphere& lrSphere1,
                         const Sphere& lrSphere2, const Sphere& lrSphere3,
                         Vector3 lLineStart, Vector3 lLineDirection, VecFloat lLineLength,
                         s32* lpiResults);

    // TestLineBoundingBoxAgainstAxisAlignedBox4 (CgsLineTests.cpp:766). Lane k: on EVERY axis, does the
    // segment from lLineStart (per-axis reciprocal direction lLineReciprocal, t in [0, 1]) reach box k's
    // slab? The three axes are tested independently -- their t ranges need not overlap -- so this is the
    // segment's bounding box against box k. The middle vector is not read by the inlined body.
    const Vector4 TestLineBoundingBoxAgainstAxisAlignedBox4(const AxisAlignedBox& lrBox0,
                                                            const AxisAlignedBox& lrBox1,
                                                            const AxisAlignedBox& lrBox2,
                                                            const AxisAlignedBox& lrBox3,
                                                            Vector3 lLineStart, Vector3 lLineEnd,
                                                            Vector3 lLineReciprocal);
}

#endif // CGS_LINE_TESTS_H

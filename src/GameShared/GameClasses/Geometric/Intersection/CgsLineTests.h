#ifndef CGS_LINE_TESTS_H
#define CGS_LINE_TESTS_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector4 alias (rw::math::vpu::Vector4)
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h"

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
    // intersection via the reciprocal-direction slab method (6 faces + start
    // endpoint inside). Asserts each direction reciprocal is non-zero.
    bool TestLineStartEndAxisAlignedBox(const Vector4& lvStart,
                                        const Vector4& lvEnd,
                                        const AxisAlignedBox& lrBox);
}

#endif // CGS_LINE_TESTS_H

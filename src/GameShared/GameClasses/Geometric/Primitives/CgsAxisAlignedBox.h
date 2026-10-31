#ifndef CGS_AXIS_ALIGNED_BOX_H
#define CGS_AXIS_ALIGNED_BOX_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3 alias (rw::math::vpu::Vector3)

// ============================================================================
// GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h
//
// CgsGeometric::AxisAlignedBox -- an axis-aligned bounding box stored as two
// 16-byte vectors: the minimum corner at +0x00 and the maximum corner at +0x10.
// OWNING home reconstructed from the X360 ARTIST build.
//
// LAYOUT (proven by the asm store/load targets):
//   Set         @ 0x823A6108   stvx128 v1 -> this+0x00, stvx128 v2 -> this+0x10
//   ContainsPoint @ 0x828AA398 lvx128  this+0x00 (min), lvx128 this+0x10 (max)
//
//   +0x00  Vector3 mMin   (minimum corner; xyz used, w lane spare)
//   +0x10  Vector3 mMax   (maximum corner; xyz used, w lane spare)
//   sizeof == 32
//
// Member names INFERRED from the two-vector min/max shape (the DWARF has no
// entry for this TU); offsets proven by the +0/+0x10 vector stores & loads.
// ============================================================================

namespace CgsGeometric
{
    struct AxisAlignedBox
    {
        Vector3 mMin;   // +0x00  minimum corner
        Vector3 mMax;   // +0x10  maximum corner

        // Set @ 0x823A6108 -- store the min corner (lvMin) at +0 and the max
        // corner (lvMax) at +0x10 (two raw 16-byte stvx128 stores).
        void Set(Vector3 lvMin, Vector3 lvMax);

        // ContainsPoint @ 0x828AA398 -- true iff the point lies strictly inside the
        // box on all three axes (min < p < max per lane). The X360 body is heavy
        // VMX (vcmpgtfp min, vcmpgefp+vnot max, vrlimi128 cross-lane AND); the max
        // edge is strict because the >= compare is negated. Bodied in the .cpp.
        bool ContainsPoint(Vector3 lvPoint) const;
    };
}

#endif // CGS_AXIS_ALIGNED_BOX_H

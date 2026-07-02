#pragma once

#include "types.hpp"
#include "SDKs/EATech/include/rw/math/vpu/vector3.h"
#include "SDKs/EATech/include/rw/math/vpu/matrix44.h"

// rw::collision::AABBox -- the RenderWare collision axis-aligned bounding box:
// two 16-byte corner rows (min at +0x00, max at +0x10), each one VMX register,
// carried as rw::math::vpu::Vector3 (a full 4-lane VectorIntrinsic; the W lane
// travels through the maths like any other lane and is stored back).
//
// OWNING HOME for the single function the X360 binary defines here:
//     rw::collision::AABBox::Transform  @ 0x828B6788
//
// No DWARF hints exist for this TU; the LAYOUT is attested by the asm:
// lvx128 [r4+0x00] / lvx128 [r4+0x10] read the two corner rows, and the result
// is stored as min row then max row (32-byte struct return through r3).

namespace rw
{
namespace collision
{

class AABBox
{
public:
    // @ 0x828B6788 -- return this box transformed by an affine matrix, or an
    // unchanged copy when lpTransform is NULL. The transform path is the
    // classic centre/half-extent form, one VMX lane per axis:
    //     centre    = (max + min) * 0.5
    //     half      = (max - min) * 0.5
    //     newCentre = xAxis*centre.x + yAxis*centre.y + zAxis*centre.z + wAxis
    //     newHalf   = |xAxis|*half.x + |yAxis|*half.y + |zAxis|*half.z
    //     result    = { newCentre - newHalf, newCentre + newHalf }
    // (matrix rows loaded from lpTransform at +0x00/+0x10/+0x20/+0x30, i.e.
    // xAxis/yAxis/zAxis/wAxis of the committed rw::math::vpu::Matrix44Affine).
    AABBox Transform(const math::vpu::Matrix44Affine* lpTransform) const;

    // flt_820F2708 -- the centre/half-extent scale. The .rdata value is not in
    // the export, but the asm dataflow pins it: the result is rebuilt as
    // centre +/- extent from K*(max+min) and K*(max-min), and the NULL-branch
    // returns the box unchanged -- consistency under the identity transform
    // forces K == 0.5 exactly (any other K rescales the box). NOTE: the same
    // address is referenced by AALineClipper::Init, whose committed
    // KF_PAD_EPSILON = 1e-6f was FLAGGED as inferred; that value should be
    // reconciled to 0.5f (see AABBox.cpp).
    static const f32 KF_HALF;

    math::vpu::Vector3 mMin;   // +0x00  lower corner (one VMX row)
    math::vpu::Vector3 mMax;   // +0x10  upper corner (one VMX row)
};

} // namespace collision
} // namespace rw



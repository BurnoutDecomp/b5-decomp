#pragma once

#include "types.hpp"
#include "rw/math/vpu/types.h"   // rw::math::vpu::Vector4 (16-byte SIMD lane)

// ===========================================================================
// rw::collision::Plane -- a plane packed into a single 16-byte VMX lane:
// xyz is the (inward) normal, w the signed offset. The frustum code stores /
// loads a plane as one `vector float` register, so the on-disk layout is
// exactly a 4-lane {x,y,z,w}.
//
// MINIMAL / INCREMENTAL: only the members grounded by the X360 asm are homed
// here -- the packed 4-lane layout and the from-Vector4 constructor that
// CgsGeometric::Frustum::VectorToPlane @ 0x82840DB0 lowers to (it lvx-loads the
// arg vector and stvx-stores it verbatim as the returned Plane). The richer
// accessor surface the DecFIGS DWARF attests (GetNormal / GetDistance and the
// Vector3Plus/VecFloat-returning helpers used by IntersectionOf3Planes) is left
// for the Plane-owning TU to add additively -- their exact sign convention is
// not exercised by VectorToPlane and is not pinned by this TU's asm.
// ===========================================================================

namespace rw
{
namespace collision
{

struct Plane
{
    // Packed plane lane: xyz = normal, w = signed offset. Matches the 16-byte
    // register the frustum asm loads/stores.
    f32 x;
    f32 y;
    f32 z;
    f32 w;

    Plane()
        : x(0.0f), y(0.0f), z(0.0f), w(0.0f)
    {
    }

    // Build a plane straight from a packed 4-lane vector (verbatim lane copy) --
    // this is what VectorToPlane's stvx128 of the (negated) arg vector produces.
    explicit Plane(const rw::math::vpu::Vector4& lrPacked)
        : x(lrPacked.x), y(lrPacked.y), z(lrPacked.z), w(lrPacked.w)
    {
    }
};

} // namespace collision
} // namespace rw

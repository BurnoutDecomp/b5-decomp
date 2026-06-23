#pragma once

#include "types.hpp"

// ===========================================================================
// rw::collision::Frustum -- a 6-plane view frustum used for box culling
// (BrnDirector::Camera::IsLookingAtTarget tests an AABB's 8 corners against it).
//
// OWNING HOME for the single Frustum function the X360 binary defines:
//     rw::collision::Frustum::IsBoxInFrustum  @ 0x82BA7FA0
//
// No DWARF hints exist for this TU. The signature/shape
// is reconstructed from the X360 VMX asm: r3 (a1) walks an array of 6 planes
// (each a 16-byte xyzw register; w is the plane's signed offset), and r4 (a2)
// is an array of the 8 transformed box corners (each xyzw). The dot used is the
// 3-lane dot (vmsum3fp128) of the plane normal against the corner.
// ===========================================================================

namespace rw
{
namespace collision
{

class Frustum
{
public:
    // One 4-lane (xyzw) vector row matching the 16-byte VMX register the asm
    // loads. For a plane: xyz is the (inward) normal, w the offset bound the
    // dot3 is compared against. For a corner: xyz the position, w carried.
    struct Vec4
    {
        f32 x;
        f32 y;
        f32 z;
        f32 w;
    };

    // @ 0x82BA7FA0 -- test the 8 box corners against the 6 frustum planes.
    // Returns 0 when some plane fully separates the box (every corner lies on
    // the plane's culled side, i.e. plane.w >= dot3(plane, corner)); returns 1
    // when no plane separates it (box overlaps / is inside the frustum).
    // X360 __fastcall: r3=lpPlanes (6 entries), r4=lpCorners (8 entries).
    static int IsBoxInFrustum(const Vec4* lpPlanes, const Vec4* lpCorners);
};

} // namespace collision
} // namespace rw

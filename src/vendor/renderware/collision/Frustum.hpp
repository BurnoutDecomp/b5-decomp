#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h" // Vector3, Matrix44Affine
#include "vendor/renderware/collision/Plane.hpp" // rw::collision::Plane (six owned planes)

// ===========================================================================
// rw::collision::Frustum -- a six-plane frustum. DecFIGS
// (SDKs/EATech/include/cmn/rw/collision/frustum.h) declares Plane m_planes[6];
// ARTIST TriangleCollisionDebugComponent::UpdateViewFrustum @0x828C0E20 writes
// those six 16-byte lanes, IsPolyVisible @0x828AAE08 reads them, and OnActivate
// @0x828AAA68 places the next member at +0x70. This is distinct from the
// eight-plane CgsGeometric::Frustum.
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

    Frustum();

    // DecFIGS frustum.h API; each operation addresses one of the six owned lanes.
    void SetPlane(u32 luIndex, const Plane& lrPlane);
    const Plane& GetPlane(u32 luIndex) const;
    Plane GetPlane(u32 luIndex);

    // ARTIST 0x82BA7FA0. This is the member form: r3 is the six-plane Frustum and
    // r4 is the array of eight four-lane box corners.
    int IsBoxInFrustum(const Vector3* lapCorners);

    // Source-compatible helper retained for the existing Director camera caller.
    // It takes an explicit plane array because that caller stores the planes in a
    // CameraRwFrustum snapshot rather than a Frustum object.
    //
    // @ 0x82BA7FA0 -- test the 8 box corners against the 6 frustum planes.
    // Returns 0 when some plane fully separates the box (every corner lies on
    // the plane's culled side, i.e. plane.w >= dot3(plane, corner)); returns 1
    // when no plane separates it (box overlaps / is inside the frustum).
    // X360 __fastcall: r3=lpPlanes (6 entries), r4=lpCorners (8 entries).
    static int IsBoxInFrustum(const Vec4* lpPlanes, const Vec4* lpCorners);

private:
    Plane m_planes[6];
};

} // namespace collision
} // namespace rw

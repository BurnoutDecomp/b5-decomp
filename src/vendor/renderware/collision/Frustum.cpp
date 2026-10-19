#include "vendor/renderware/collision/Frustum.hpp"

// ===========================================================================
// rw::collision::Frustum -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   Frustum::IsBoxInFrustum  @ 0x82BA7FA0
//
// The X360 body is hand-vectorised (lvx128 / vmsum3fp128 / fcmpu). Following
// the project's CgsGeometric::Triangle4 / FeatureEdge precedent it is lowered to
// portable scalar float maths, preserving the loop structure and comparison
// exactly. See Frustum.hpp for the plane/corner shape.
// ===========================================================================

namespace rw
{
namespace collision
{

namespace
{
    // dot3 of the xyz lanes (the asm's vmsum3fp128, lanes 0..2).
    inline f32 Dot3(const Frustum::Vec4& a, const Frustum::Vec4& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
}

// DecFIGS rw/collision/frustum.h: the object owns six Plane values. Plane's
// existing default constructor initializes each lane; ARTIST's Triangle debug
// Construct does not overwrite mViewFrustum.
Frustum::Frustum()
{
}

void Frustum::SetPlane(u32 luIndex, const Plane& lrPlane)
{
    m_planes[luIndex] = lrPlane;
}

const Plane& Frustum::GetPlane(u32 luIndex) const
{
    return m_planes[luIndex];
}

Plane Frustum::GetPlane(u32 luIndex)
{
    return m_planes[luIndex];
}

int Frustum::IsBoxInFrustum(const Vector3* lapCorners)
{
    unsigned int luPlane = 0;
    while (true)
    {
        const Plane& lrPlane = m_planes[luPlane];
        int liAllCornersOnCulledSide = 1;
        for (int liCorner = 0; liCorner < 8; ++liCorner)
        {
            const Vector3& lrCorner = lapCorners[liCorner];
            const f32 lfDot = lrPlane.x * lrCorner.x + lrPlane.y * lrCorner.y
                            + lrPlane.z * lrCorner.z;
            // ARTIST fcmpu/blt rejects only w < dot; unordered stays culled.
            const int liOnCulledSide = !(lrPlane.w < lfDot) ? 1 : 0;
            liAllCornersOnCulledSide &= liOnCulledSide;
        }

        if (liAllCornersOnCulledSide)
            return 0;

        ++luPlane;
        if (luPlane >= 6)
            return 1;
    }
}

// ---------------------------------------------------------------------------
// Frustum::IsBoxInFrustum @ 0x82BA7FA0
//
// Outer loop over the 6 planes (v4 = 0..5, r9 += 0x10 each step). For each
// plane, the inner loop tests the 8 box corners (v7 = 8 down to 0, r10 += 0x10):
//     dot      = dot3(plane, corner)                 (vmsum3fp128 -> back_chain)
//     liInside = (plane.w >= dot) & liInside         (lfs 0xC(r9); fcmpu; and)
// The asm computes the predicate as: r7 = (plane.w < dot) ? 0 : 1 via
// (blt -> li 1 else li 0; cntlzw; extrwi 1,26) i.e. r7 = (plane.w >= dot).
// If after all 8 corners liInside is still 1, that plane separates the box and
// the function returns 0. Otherwise advance to the next plane; after 6 planes
// with no separator, return 1.
// ---------------------------------------------------------------------------
int Frustum::IsBoxInFrustum(const Vec4* lpPlanes, const Vec4* lpCorners)
{
    unsigned int luPlane = 0;
    while (true)
    {
        const Vec4& lPlane = lpPlanes[luPlane];

        int liInside = 1;
        for (int liCorner = 0; liCorner < 8; ++liCorner)
        {
            const f32 lfDot = Dot3(lPlane, lpCorners[liCorner]);
            const int liGe = (lPlane.w >= lfDot) ? 1 : 0;
            liInside = liGe & liInside;
        }

        if (liInside)
        {
            // Plane fully separates the box: it is outside the frustum.
            return 0;
        }

        ++luPlane;
        if (luPlane >= 6)
        {
            // No plane separated the box across all six: inside / overlapping.
            return 1;
        }
    }
}

} // namespace collision
} // namespace rw

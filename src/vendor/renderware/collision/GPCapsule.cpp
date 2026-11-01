#include "vendor/renderware/collision/GPInstance.hpp"

#include <cmath>     // fabs
#include <cstring>   // memcpy (the 0x40 FeatureEdge copy)

// ===========================================================================
// rw::collision::GPCapsule -- the capsule primitive's VolumeMethods callbacks,
// reconstructed from BURNOUT_X360_ARTIST.XEX (dedicated VMX pass wave 2).
//
//   GPCapsule::GetMaximumFeature  @ 0x82BAFA80   (VolumeMethods +0xA4)
//   GPCapsule::GetInterval        @ 0x82BAFB68   (VolumeMethods +0xA8)
//   GPCapsule::GetIntervals       @ 0x82BAFC10   (VolumeMethods +0xAC)
//
// Canonical declarations Feb-2007 rwccore.h:1203-1205 / DWARF volume.h:371-377
// (non-static const members on PS3); reconstructed as static plain-function
// callbacks matching the committed GPInstance::VolumeMethods typedefs per the
// X360 delta note in GPInstance.hpp. The GP capsule core is the axis segment
// [centre - h*axis, centre + h*axis] (centre = mPos, axis =
// mEdgeDirections[0], h = mDimensions.x == the canonical HalfHeight()); the
// radius rides in mFatness and is applied by the callers.
//
// GPCapsule::GetBBox exists in the binary at another address outside this TU
// and is not homed here.
// ===========================================================================

namespace rw
{
namespace collision
{

namespace
{
    // dot3 of the xyz lanes (the asm's vmsum3fp128, lane-broadcast fold).
    inline f32 Dot3(const Vec4& a, const Vec4& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    // The vmsum3fp128 / vspltw broadcast row: every lane carries s.
    inline Vec4 Splat(f32 s)
    {
        Vec4 r;
        r.x = s;
        r.y = s;
        r.z = s;
        r.w = s;
        return r;
    }

    // vsubfp / vaddfp: per-lane a -/+ b.
    inline Vec4 Sub(const Vec4& a, const Vec4& b)
    {
        Vec4 r;
        r.x = a.x - b.x;
        r.y = a.y - b.y;
        r.z = a.z - b.z;
        r.w = a.w - b.w;
        return r;
    }

    inline Vec4 Add(const Vec4& a, const Vec4& b)
    {
        Vec4 r;
        r.x = a.x + b.x;
        r.y = a.y + b.y;
        r.z = a.z + b.z;
        r.w = a.w + b.w;
        return r;
    }

    // vmulfp128 against a splat: per-lane a * s.
    inline Vec4 Scale(const Vec4& a, f32 s)
    {
        Vec4 r;
        r.x = a.x * s;
        r.y = a.y * s;
        r.z = a.z * s;
        r.w = a.w * s;
        return r;
    }

    // vmaddfp against a splat multiplier: per-lane a*s + b (vmaddfp
    // vD, vA, vB, vC computes vD = vA*vC + vB -- multiplier LAST, addend
    // THIRD; operand rule attested across this family).
    inline Vec4 MaddScalar(const Vec4& a, f32 s, const Vec4& b)
    {
        Vec4 r;
        r.x = a.x * s + b.x;
        r.y = a.y * s + b.y;
        r.z = a.z * s + b.z;
        r.w = a.w * s + b.w;
        return r;
    }
}

// ===========================================================================
// rw::collision::GPCapsule::GetMaximumFeature @ 0x82BAFA80
// Reached through mMethods.mGetMaximumFeature by the committed
// PrimitiveIntersect.cpp batch kernels (pass 2) and ComputeContactPoints.
//
// Register image at the branch (0x82BAFAD0):
//   v0  = axis (mEdgeDirections[0]); v12 = splat(h) (mDimensions lane X);
//   v13 = centre (mPos); f13 = a = dot3(axis, dir); f12 = fabs(a).
//
//   fcmpu cr6, f12, flt_82180458(0.05f); bge -> vertex path
//     (unordered/NaN falls through to the edge path, as `>=` does here)
//
// EDGE path: FeatureEdge(centre + h*axis, centre - h*axis) memcpy'd into
// edges[0], numedges = 1. VERTEX path: pt = the supporting end (+ end iff
// a > 0, else the - end), numedges = 0. Common tail: region = 0. abCcw is
// NOT read by this body (the edge's prism-wall planes are built later by the
// consumers, which is where the winding matters).
//
// rodata:
//   flt_82180458 = 0.05f  (vertex/edge threshold; the extract's pseudocode
//                          compares against 0.050000001, the decimal print of
//                          float 0.05)
//   flt_82001CC0 = 0.0f   (end-selection compare)
// ===========================================================================
void GPCapsule::GetMaximumFeature(RwBool /*abCcw*/, math::vpu::Vector3 arDir, Feature& arFeature) const
{
    const __vector4 axis = mEdgeDirections[0].mV;
    const __vector4 halfHeight = __vspltw(mDimensions.mV, 0);
    const f32 projection = __vmsum3fp(axis, arDir.mV).x;
    if (std::fabs(projection) < 0.05f)
    {
        const __vector4 scaled = __vmulfp(axis, halfHeight);
        const math::vpu::Vector3 plus(__vaddfp(mPos.mV, scaled));
        const math::vpu::Vector3 minus(__vsubfp(mPos.mV, scaled));
        const FeatureEdge edge(plus, minus);
        memcpy(&arFeature.edges[0], &edge, sizeof(edge));
        arFeature.numedges = 1;
    }
    else
    {
        if (projection > 0.0f)
            arFeature.pt.mV = __vmaddfp(axis, halfHeight, mPos.mV);
        else
            arFeature.pt.mV = __vsubfp(mPos.mV, __vmulfp(axis, halfHeight));
        arFeature.numedges = 0;
    }
    arFeature.region = 0;
}

// ===========================================================================
// rw::collision::GPCapsule::GetInterval @ 0x82BAFB68
//
//   min = dot3(centre, dir) - |dot3(axis, dir)| * halfHeight
//   max = dot3(centre, dir) + |dot3(axis, dir)| * halfHeight
//
// Every caller-visible store is preserved, including the min/max seeding with
// the raw centre projection before the extent fix-up. The Interval padding
// rows (+0x20..+0x2F) are not written, exactly as on the console.
// ===========================================================================
void GPCapsule::GetInterval(math::vpu::Vector3 arDir, Interval& arInterval) const
{
    const __vector4 projection = __vmsum3fp(mPos.mV, arDir.mV);
    arInterval.min.mV = projection;
    arInterval.max.mV = projection;
    const f32 halfHeight = mDimensions.mV.x;
    const f32 direction = __vmsum3fp(mEdgeDirections[0].mV, arDir.mV).x;
    const f32 extent = std::fabs(direction) * halfHeight;
    const __vector4 value = __vspltw(__lvlx(&extent, 0), 0);
    arInterval.min.mV = __vsubfp(projection, value);
    arInterval.max.mV = __vaddfp(projection, value);
}

// ===========================================================================
// rw::collision::GPCapsule::GetIntervals @ 0x82BAFC10
// Called by the committed FindBestSeparatingDirection through
// mMethods.mGetIntervals. Per direction this is exactly GetInterval above;
// the per-element min/max seeding with the raw centre projection is
// preserved, the Interval padding rows are not written, and the 0x30 output
// stride is sizeof(Interval) (static-asserted in GPInstance.hpp).
// ===========================================================================
void GPCapsule::GetIntervals(const math::vpu::Vector3* lapDirs, u32 auNumDirs, Interval* lapIntervals) const
{
    for (u32 i = 0; i < auNumDirs; ++i)
    {
        const __vector4 projection = __vmsum3fp(mPos.mV, lapDirs[i].mV);
        lapIntervals[i].min.mV = projection;
        lapIntervals[i].max.mV = projection;
        const f32 halfHeight = mDimensions.mV.x;
        const f32 direction = __vmsum3fp(mEdgeDirections[0].mV, lapDirs[i].mV).x;
        const f32 extent = std::fabs(direction) * halfHeight;
        const __vector4 value = __vspltw(__lvlx(&extent, 0), 0);
        lapIntervals[i].min.mV = __vsubfp(projection, value);
        lapIntervals[i].max.mV = __vaddfp(projection, value);
    }
}

// ARTIST's GP capsule bbox slot is the folded empty method at 82AD5078.
void GPCapsule::GetBBox(AABBox&) const {}

} // namespace collision
} // namespace rw

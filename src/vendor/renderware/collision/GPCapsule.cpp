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
void GPCapsule::GetMaximumFeature(const GPInstance* lpThis, RwBool /*abCcw*/,
                                  const Vec4& arDir, Feature& arFeature)
{
    // flt_82180458 -- the vertex-vs-edge axis-projection threshold. FLAGGED:
    // the NAME is the reconstruction's; the VALUE (0.05f) is the extract's.
    const f32 KF_VERTEX_PROJ_THRESHOLD = 0.05f;

    const Vec4& lvAxis   = lpThis->mEdgeDirections[0];   // this+0x40
    const Vec4& lvCentre = lpThis->mPos;                 // this+0x00
    const f32   lfHalfHeight = lpThis->mDimensions.x;    // this+0x70, vspltw 0

    // vmsum3fp128 v13, v0, v1 -- dot3(axis, dir) (spilled; read on the scalar
    // fpu for both compares).
    const f32 lfAxisProj = Dot3(lvAxis, arDir);

    // fcmpu f12(fabs), f0(0.05f); bge -> vertex. Polarity preserved: an
    // unordered (NaN) compare falls into the edge path on both machines.
    if (std::fabs(lfAxisProj) >= KF_VERTEX_PROJ_THRESHOLD)
    {
        // ---- vertex path: the supporting end cap centre -------------------
        Vec4 lvVertex;
        // fcmpu f13, 0.0f; ble -> minus end (unordered/NaN also takes it).
        if (lfAxisProj > 0.0f)
        {
            // vmaddfp v0, v0, v13, v12 == axis * splat(h) + centre.
            lvVertex = MaddScalar(lvAxis, lfHalfHeight, lvCentre);
        }
        else
        {
            // vmulfp128 + vsubfp == centre - axis * splat(h).
            lvVertex = Sub(lvCentre, Scale(lvAxis, lfHalfHeight));
        }

        arFeature.pt       = lvVertex;   // stvx128 v0, r31, 0x220
        arFeature.numedges = 0;          // stw r30(0), 0x230(r31)
    }
    else
    {
        // ---- edge path: the whole axis segment ----------------------------
        const Vec4 lvHalfVec  = Scale(lvAxis, lfHalfHeight);   // vmulfp128
        const Vec4 lvEndMinus = Sub(lvCentre, lvHalfVec);      // v12 -> var_80
        const Vec4 lvEndPlus  = Add(lvCentre, lvHalfVec);      // v0  -> var_70

        // FeatureEdge::FeatureEdge @ 0x82BA85B8 (committed, FeatureEdge.cpp):
        // arP1 = the + end (r4 = &var_70), arP2 = the - end (r5 = &var_80).
        const FeatureEdge lEdge(lvEndPlus, lvEndMinus);

        // memcpy(r31+0x10, &edge, 0x40) -> edges[0]; sizeof(FeatureEdge) is
        // static-asserted == 0x40 in the FeatureEdge home.
        std::memcpy(&arFeature.edges[0], &lEdge, sizeof(FeatureEdge));

        arFeature.numedges = 1;          // stw 1, 0x230(r31)
    }

    // Common tail: stw r30(0), 0(r31).
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
void GPCapsule::GetInterval(const GPInstance* lpThis,
                            const Vec4& arDir, Interval& arInterval)
{
    // vmsum3fp128 v0, v0, v1 -- dot3(mPos, dir), lane-broadcast.
    const f32 lfCentreProj = Dot3(lpThis->mPos, arDir);

    // Seed both rows with the centre projection (stvx128 pair @ 0x82BAFB88).
    arInterval.min = Splat(lfCentreProj);
    arInterval.max = Splat(lfCentreProj);

    // halfHeight = mDimensions.x (lvx this+0x70 + vspltw 0 == the canonical
    // GPCapsule::HalfHeight()); a = dot3(mEdgeDirections[0], dir).
    const f32 lfHalfHeight = lpThis->mDimensions.x;
    const f32 lfAxisProj   = Dot3(lpThis->mEdgeDirections[0], arDir);

    // Scalar-fpu extent, exactly the asm's fabs + fmuls operand order:
    // e = fabs(a) * halfHeight.
    const f32 lfExtent = std::fabs(lfAxisProj) * lfHalfHeight;

    // vsubfp / vaddfp against the splat(e) quad.
    arInterval.min = Splat(lfCentreProj - lfExtent);   // stvx128 v0, r0, r4
    arInterval.max = Splat(lfCentreProj + lfExtent);   // stvx128 v0, r0, r10
}

// ===========================================================================
// rw::collision::GPCapsule::GetIntervals @ 0x82BAFC10
// Called by the committed FindBestSeparatingDirection through
// mMethods.mGetIntervals. Per direction this is exactly GetInterval above;
// the per-element min/max seeding with the raw centre projection is
// preserved, the Interval padding rows are not written, and the 0x30 output
// stride is sizeof(Interval) (static-asserted in GPInstance.hpp).
// ===========================================================================
void GPCapsule::GetIntervals(const GPInstance* lpThis, const Vec4* lapDirs,
                             u32 auNumDirs, Interval* lapIntervals)
{
    // cmplwi/beq: the zero-count call falls straight out.
    for (u32 luDir = 0; luDir < auNumDirs; ++luDir)
    {
        const Vec4& lrDir      = lapDirs[luDir];
        Interval&   lrInterval = lapIntervals[luDir];

        // vmsum3fp128 v0, v13, v0 -- dot3(mPos, dirs[i]), lane-broadcast.
        const f32 lfCentreProj = Dot3(lpThis->mPos, lrDir);

        // Seed both rows with the centre projection (stvx128 pair
        // @ 0x82BAFC54/58).
        lrInterval.min = Splat(lfCentreProj);
        lrInterval.max = Splat(lfCentreProj);

        // a = dot3(mEdgeDirections[0], dir); halfHeight = mDimensions.x.
        const f32 lfAxisProj   = Dot3(lpThis->mEdgeDirections[0], lrDir);
        const f32 lfHalfHeight = lpThis->mDimensions.x;

        // Scalar-fpu extent, exactly the asm's fabs + fmuls operand order.
        const f32 lfExtent = std::fabs(lfAxisProj) * lfHalfHeight;

        // vsubfp / vaddfp against the splat(e) quad.
        lrInterval.min = Splat(lfCentreProj - lfExtent);   // stvx128 -> r11
        lrInterval.max = Splat(lfCentreProj + lfExtent);   // stvx128 -> r8
    }
}

} // namespace collision
} // namespace rw

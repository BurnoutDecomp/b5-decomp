#include "vendor/renderware/collision/GPInstance.hpp"
#include "vendor/renderware/collision/LineSegKernelMath.hpp"

#include <cmath>   // fabs
#include <cstring> // complete FeatureEdge object copies

// ===========================================================================
// rw::collision::GPCylinder -- the cylinder primitive's VolumeMethods
// callbacks, reconstructed from BURNOUT_X360_ARTIST.XEX (dedicated VMX pass
// wave 2).
//
//   GPCylinder::GetMaximumFeature  @ 0x82BAE430   (VolumeMethods +0xA4)
//   GPCylinder::GetInterval        @ 0x82BAD7F8   (VolumeMethods +0xA8)
//   GPCylinder::GetIntervals       @ 0x82BAD938   (VolumeMethods +0xAC)
//
// Native const members consume the direction by value in v1. GetMaximumFeature
// receives this/r3, winding/r4 and Feature&/r5. Real inherited Vector3 rows
// retain mV storage; no scalar callback facade is introduced here.
//
// The local-frame transpose and all product/fused accumulation boundaries are
// retained. VMX estimates receive the original ordered two refinements.
// ===========================================================================

namespace rw
{
namespace collision
{

namespace
{
    inline __vector4 SplatValue(f32 value)
    {
        return __vspltw(__lvlx(&value, 0), 0);
    }

    inline __vector4 FlipSigns(__vector4 value)
    {
        const __vector4 ones = __vspltisw(-1);
        return __vxor(value, __vslw(ones, ones));
    }

    inline __vector4 CrossNative(__vector4 a, __vector4 b)
    {
        const __vector4 ayzx = __vpermwi(a, 0x63);
        const __vector4 byzx = __vpermwi(b, 0x63);
        return __vpermwi(__vnmsubfp(ayzx, b, __vmulfp(a, byzx)), 0x63);
    }

    inline __vector4 ReciprocalRootTwice(__vector4 square)
    {
        const __vector4 one = __vcfsx(__vspltisw(1), 0);
        const __vector4 half = __vcfsx(__vspltisw(1), 1);
        __vector4 estimate = __vrsqrtefp(square);
        const __vector4 squareEstimate = __vmulfp(estimate, estimate);
        const __vector4 halfEstimate = __vmulfp(estimate, half);
        const __vector4 correction = __vnmsubfp(square, squareEstimate, one);
        estimate = __vmaddfp(halfEstimate, correction, estimate);
        const __vector4 squareRefined = __vmulfp(estimate, estimate);
        const __vector4 halfRefined = __vmulfp(estimate, half);
        const __vector4 refinedCorrection = __vnmsubfp(square, squareRefined, one);
        return __vmaddfp(halfRefined, refinedCorrection, estimate);
    }

    inline __vector4 ReciprocalTwice(__vector4 value)
    {
        const __vector4 one = __vcfsx(__vspltisw(1), 0);
        __vector4 estimate = __vrefp(value);
        const __vector4 correction = __vnmsubfp(estimate, value, one);
        estimate = __vmaddfp(estimate, correction, estimate);
        const __vector4 refinedCorrection = __vnmsubfp(estimate, value, one);
        return __vmaddfp(estimate, refinedCorrection, estimate);
    }

    inline __vector4 GuardedLength(__vector4 value)
    {
        const __vector4 zero = __vspltisw(0);
        const __vector4 square = __vmsum3fp(value, value);
        const __vector4 length = __vmulfp(square, ReciprocalRootTwice(square));
        return __vsel(length, zero, __vcmpeqfp(zero, square));
    }

    inline __vector4 Rotate(__vector4 x, __vector4 y, __vector4 z, __vector4 value)
    {
        __vector4 result = __vmulfp(x, __vspltw(value, 0));
        result = __vmaddfp(y, __vspltw(value, 1), result);
        return __vmaddfp(z, __vspltw(value, 2), result);
    }

    inline __vector4 TransformPoint(__vector4 x, __vector4 y, __vector4 z,
                                    __vector4 position, __vector4 value)
    {
        __vector4 result = __vmaddfp(x, __vspltw(value, 0), position);
        result = __vmaddfp(y, __vspltw(value, 1), result);
        return __vmaddfp(z, __vspltw(value, 2), result);
    }
}

// flt_821803A0: |cos| threshold above which the direction counts as
// axis-aligned and the end cap is the maximum feature. Value attested by the
// export's pseudocode compare (`> 0.99980003`).
static const f32 KF_END_FACE_COS_THRESHOLD = 0.99980003f;

// flt_82005574: |cos| threshold below which the direction counts as
// perpendicular to the axis and the side line is the maximum feature. Value
// attested by the export's pseudocode compare (`<= 0.02`).
static const f32 KF_SIDE_EDGE_COS_THRESHOLD = 0.02f;

// flt_821802F8: the octagon diagonal-vertex coefficient, cos(45deg). Value
// attested by the export's pseudocode multiply (`* 0.70710677`).
static const f32 KF_OCTAGON_DIAGONAL = 0.70710677f;

// ===========================================================================
// rw::collision::GPCylinder::GetMaximumFeature @ 0x82BAE430
// Reached through mMethods.mGetMaximumFeature by the committed
// PrimitiveIntersect.cpp batch kernels (pass 2) and ComputeContactPoints.
//
// Structure (asm-attested):
//   0. Fold the direction into the cylinder's LOCAL frame with a vmrghw/
//      vmrglw transpose + a product and two ordered fused column folds.
//      fn[0] is the axis; the zero fourth column still participates.
//   A. |localDir.z| > 0.99980003: END-CAP FACE -- an 8-vertex octagon at
//      z = +/-halfHeight, 8 FeatureEdge records, numedges = 8. The winding
//      flag is toggled instead of the loop normal when the cap faces -Z
//      (v121 stays {0,0,1,0} for the prism-wall crosses; only the STORED
//      ownNormal is sign-flipped).
//   C. |localDir.z| <= 0.02: SIDE EDGE -- the axis-parallel rim line nearest
//      the direction, one FeatureEdge, numedges = 1. Its pn row is NEVER
//      staged: the console memcpy carries 16 bytes of leftover stack into
//      edges[0].pn. The common tail also rotates that copied row.
//   D. otherwise: RIM POINT -- radius * normalised XY of localDir with
//      z = +/-halfHeight, numedges = 0, feature.pt written.
//   T. Tail (all paths): rotate ownNormal / translate+rotate pt into world
//      space, then rotate/translate every edge record's dir/base/pn rows
//      (length untouched). On paths A and C feature.pt (+0x220) was never
//      written -- the console transforms whatever the caller's slot held;
//      preserved.
//
// rodata: flt_82001CC0 = 0.0f / flt_82001C98 = 1.0f (committed
// SeparatingDirection.cpp rodata note); the three thresholds above.
// ===========================================================================
void GPCylinder::GetMaximumFeature(RwBool abCcw, math::vpu::Vector3 arDir,
                                   Feature& arFeature) const
{
    const __vector4 position = mPos.mV;
    const __vector4 axisX = mFaceNormals[2].mV;
    const __vector4 axisY = mFaceNormals[1].mV;
    const __vector4 axisZ = mFaceNormals[0].mV;
    const __vector4 zero = __vspltisw(0);

    // ARTIST82BAE480..4C8: merged-word transpose followed by one product
    // and two ordered fused additions, including the fourth lane.
    const __vector4 xzHigh = __vmrghw(axisX, axisZ);
    const __vector4 yzHigh = __vmrghw(axisY, zero);
    const __vector4 xzLow = __vmrglw(axisX, axisZ);
    const __vector4 yzLow = __vmrglw(axisY, zero);
    const __vector4 columnX = __vmrghw(xzHigh, yzHigh);
    const __vector4 columnY = __vmrglw(xzHigh, yzHigh);
    const __vector4 columnZ = __vmrghw(xzLow, yzLow);
    __vector4 localDirection = __vmulfp(columnX, __vspltw(arDir.mV, 0));
    localDirection = __vmaddfp(columnY, __vspltw(arDir.mV, 1), localDirection);
    localDirection = __vmaddfp(columnZ, __vspltw(arDir.mV, 2), localDirection);
    const f32 lfLocalZ = localDirection.z;
    const f32 lfAbsoluteZ = std::fabs(lfLocalZ);

    // The genuine temporary's pn is deliberately unwritten on the side
    // path. Copy its complete object representation, as the retail memcpy
    // does; no value or preserved destination row is invented for it.
    FeatureEdge edge;
    if (lfAbsoluteZ > KF_END_FACE_COS_THRESHOLD)
    {
        const __vector4 normal = {0.0f, 0.0f, 1.0f, 0.0f};
        arFeature.ownNormal.mV = normal;
        f32 lfHeight = mDimensions.mV.x;
        RwBool lbCcw = abCcw;
        if (lfLocalZ < 0.0f)
        {
            lbCcw = abCcw == 0;
            arFeature.ownNormal.mV = FlipSigns(normal);
            lfHeight = -lfHeight;
        }
        const f32 lfRadius = mDimensions.mV.y;
        const f32 lfDiagonal = lfRadius * KF_OCTAGON_DIAGONAL;
        const __vector4 octagon[8] =
        {
            {0.0f, lfRadius, lfHeight, 0.0f},
            {lfDiagonal, lfDiagonal, lfHeight, 0.0f},
            {lfRadius, 0.0f, lfHeight, 0.0f},
            {lfDiagonal, -lfDiagonal, lfHeight, 0.0f},
            {0.0f, -lfRadius, lfHeight, 0.0f},
            {-lfDiagonal, -lfDiagonal, lfHeight, 0.0f},
            {-lfRadius, 0.0f, lfHeight, 0.0f},
            {-lfDiagonal, lfDiagonal, lfHeight, 0.0f}
        };
        for (u32 lu = 0; lu < 8; ++lu)
        {
            const __vector4 base = lbCcw ? octagon[7 - lu] : octagon[lu];
            const __vector4 next = lbCcw ? octagon[(6u - lu) & 7] : octagon[(lu + 1) & 7];
            const __vector4 delta = __vsubfp(next, base);
            const __vector4 plane = lbCcw ? CrossNative(delta, normal) : CrossNative(normal, delta);
            const __vector4 length = GuardedLength(delta);
            const __vector4 directionInverse = ReciprocalTwice(length);
            const __vector4 planeInverse = ReciprocalTwice(length);
            edge.base.mV = base;
            edge.dir.mV = __vmulfp(directionInverse, delta);
            edge.length.mV = length;
            edge.pn.mV = __vmulfp(planeInverse, plane);
            std::memcpy(&arFeature.edges[lu], &edge, sizeof(edge));
        }
        arFeature.numedges = 8;
    }
    else if (!(lfAbsoluteZ > KF_SIDE_EDGE_COS_THRESHOLD))
    {
        // ARTIST's ble includes unordered, unlike an ordered <= test.
        const __vector4 square = __vmsum3fp(localDirection, localDirection);
        const __vector4 unit = __vmulfp(localDirection, ReciprocalRootTwice(square));
        const __vector4 radius = __vspltw(mDimensions.mV, 1);
        const __vector4 height = __vspltw(mDimensions.mV, 0);
        const __vector4 top = __vrlimi(__vmulfp(unit, radius), height, 2, 0);
        const __vector4 bottom = __vrlimi(top, FlipSigns(height), 2, 0);
        const __vector4 up = __vsubfp(top, bottom);
        const __vector4 down = __vsubfp(bottom, top);
        const __vector4 length = GuardedLength(up);
        const __vector4 downSquare = __vmsum3fp(down, down);
        edge.base.mV = top;
        edge.length.mV = SplatValue(length.x);
        edge.dir.mV = __vmulfp(down, ReciprocalRootTwice(downSquare));
        std::memcpy(&arFeature.edges[0], &edge, sizeof(edge));
        arFeature.ownNormal.mV = localDirection;
        arFeature.numedges = 1;
    }
    else
    {
        const __vector4 radial = __vrlimi(localDirection, zero, 2, 0);
        const __vector4 square = __vmsum3fp(radial, radial);
        const __vector4 unit = __vmulfp(radial, ReciprocalRootTwice(square));
        __vector4 point = __vmulfp(unit, __vspltw(mDimensions.mV, 1));
        point = __vrlimi(point, __vspltw(mDimensions.mV, 0), 2, 0);
        if (lfLocalZ < 0.0f)
            point = __vrlimi(point, FlipSigns(__vspltw(point, 2)), 2, 0);
        arFeature.numedges = 0;
        arFeature.ownNormal.mV = localDirection;
        arFeature.pt.mV = point;
    }

    const __vector4 localNormal = arFeature.ownNormal.mV;
    const __vector4 localPoint = arFeature.pt.mV;
    arFeature.ownNormal.mV = Rotate(axisX, axisY, axisZ, localNormal);
    arFeature.pt.mV = TransformPoint(axisX, axisY, axisZ, position, localPoint);
    for (s32 li = 0; li < arFeature.numedges; ++li)
    {
        FeatureEdge& result = arFeature.edges[li];
        const __vector4 direction = result.dir.mV;
        const __vector4 base = result.base.mV;
        const __vector4 plane = result.pn.mV;
        result.dir.mV = Rotate(axisX, axisY, axisZ, direction);
        result.base.mV = TransformPoint(axisX, axisY, axisZ, position, base);
        result.pn.mV = Rotate(axisX, axisY, axisZ, plane);
    }
}

// ===========================================================================
// rw::collision::GPCylinder::GetInterval @ 0x82BAD7F8
//
//   centreDot = dot3(mPos, dir); axisDot = dot3(mEdgeDirections[0], dir);
//   sinSq = |1.0 - axisDot^2| (fnmsubs + fabs);
//   sin   = (sinSq == 0) ? 0 : sinSq * rsqrt(sinSq)  (guarded sqrt);
//   proj  = halfHeight * |axisDot| + radius * sin;
//   min = centreDot - |proj|; max = |proj| + centreDot (this operand order);
//   interval.min/max = lane-0 broadcasts (padding words NOT touched).
// Hex-Rays's "int result" return is the r3 pass-through artifact -- void.
// ===========================================================================
namespace
{
    // ARTIST 82BAD7F8/82BAD938: scalar fnmsubs precedes two VMX
    // reciprocal-root refinements; scalar fsubs/fadds form the bounds.
    inline void CylinderInterval(const GPCylinder& cylinder, __vector4 direction, Interval& interval)
    {
        const f32 centre = __vmsum3fp(cylinder.mPos.mV, direction).x;
        const f32 axial = __vmsum3fp(cylinder.mEdgeDirections[0].mV, direction).x;
        const f32 square = std::fabs(linemath::ScalarNmsub(axial, axial, 1.0f));
        const f32 absoluteAxial = std::fabs(axial);
        const __vector4 x = {square, square, square, square};
        const __vector4 axialV = {absoluteAxial, absoluteAxial, absoluteAxial, absoluteAxial};
        const __vector4 zero = __vspltisw(0);
        const __vector4 one = __vcfsx(__vspltisw(1), 0);
        const __vector4 half = __vcfsx(__vspltisw(1), 1);
        __vector4 estimate = __vrsqrtefp(x);
        __vector4 correction = __vnmsubfp(x, __vmulfp(estimate, estimate), one);
        estimate = __vmaddfp(__vmulfp(estimate, half), correction, estimate);
        correction = __vnmsubfp(x, __vmulfp(estimate, estimate), one);
        estimate = __vmaddfp(__vmulfp(estimate, half), correction, estimate);
        const __vector4 root = __vsel(__vmulfp(x, estimate), zero, __vcmpeqfp(zero, x));
        const __vector4 radial = __vmulfp(__vspltw(cylinder.mDimensions.mV, 1), root);
        const __vector4 extent = __vmaddfp(__vspltw(cylinder.mDimensions.mV, 0), axialV, radial);
        const f32 absoluteExtent = std::fabs(extent.x);
        const f32 lo = centre - absoluteExtent;
        const f32 hi = absoluteExtent + centre;
        const __vector4 minV = {lo, lo, lo, lo};
        const __vector4 maxV = {hi, hi, hi, hi};
        interval.min.mV = minV;
        interval.max.mV = maxV;
    }
}

void GPCylinder::GetInterval(rw::math::vpu::Vector3 arDir, Interval& arInterval) const
{
    CylinderInterval(*this, arDir.mV, arInterval);
}

void GPCylinder::GetIntervals(const rw::math::vpu::Vector3* lapDirs, u32 auNumDirs, Interval* lapIntervals) const
{
    for (u32 i = 0; i < auNumDirs; ++i)
        CylinderInterval(*this, lapDirs[i].mV, lapIntervals[i]);
}

void GPCylinder::GetBBox(AABBox&) const {} // ARTIST ICF 0x82AD5078

} // namespace collision
} // namespace rw

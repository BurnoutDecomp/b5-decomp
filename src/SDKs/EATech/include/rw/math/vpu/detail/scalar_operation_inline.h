#pragma once

#include "SDKs/EATech/include/rw/math/vpu/vec_float.h"
#include "SDKs/EATech/include/rw/math/vpu/maskscalar.h"
#include "SDKs/EATech/include/rw/math/vpu/vector_intrinsic_operation.h"
#include "SDKs/EATech/include/rw/math/vpu/detail/vec_float_operation_platform_inline.h"
#include "SDKs/EATech/include/rw/math/vpu/detail/scalar_operation_platform_inline.h"

namespace rw { namespace math { namespace vpu {
// DecFIGS scalar_operation_inline.h:112 (rwmath 1.02.00 :71): clear the sign bit of every lane
// with a vspltisw(-1) << itself mask (ARTIST AIAggression::GetAcrossSeparation 0x827711DC..0x8277122C).
template <>
inline VecFloat Abs<VecFloat>(const VecFloat& a)
{
    const VectorIntrinsic mask = __vspltisw(-1);
    const VectorIntrinsic signBitMask = __vslw(mask, mask);
    return VecFloat(__vandc(a.mV, signBitMask));
}
// rwmath 1.02.00 detail/scalar_operation_inline.h:88..:111 (DecFIGS DWARF lists both): one vminfp /
// vmaxfp on the broadcast registers. ARTIST BrnTraffic ParamTransform::UpdateLerpedPosition 0x82712B78
// `vmaxfp v0,v0,v12` (Max(speed, GetVecFloat_Zero())).
template <>
inline VecFloat Min<VecFloat>(const VecFloat& a, const VecFloat& b)
{
    VectorIntrinsic result;
    result = __vminfp(a.mV, b.mV);
    return VecFloat(result);
}
template <>
inline VecFloat Max<VecFloat>(const VecFloat& a, const VecFloat& b)
{
    VectorIntrinsic result;
    result = __vmaxfp(a.mV, b.mV);
    return VecFloat(result);
}
// DecFIGS scalar_operation_inline.h:163.
inline VecFloat Lerp(VecFloat::InParam a, VecFloat::InParam b, VecFloat::InParam t)
{
    return Add(a, Mult(Subtract(b, a), t));
}
// DecFIGS scalar_operation_inline.h:242: Min(max, Max(min, value)) through the float
// specialisations, every operand by reference.
inline float Clamp(const float& value, const float& min, const float& max)
{
    return Min(max, Max(min, value));
}
// DecFIGS scalar_operation_inline.h:303/311. Less-than is the native
// complement of >=, including unordered lanes; greater-than is a bare compare.
inline MaskScalar CompLessThan(VecFloat::InParam a, VecFloat::InParam b)
{
    const VectorIntrinsic greaterEqual = __vcmpgefp(a.mV, b.mV);
    return MaskScalar(__vnor(greaterEqual, greaterEqual));
}
inline MaskScalar CompGreaterThan(VecFloat::InParam a, VecFloat::InParam b)
{
    return MaskScalar(__vcmpgtfp(a.mV, b.mV));
}
// rwmath 1.02.00 detail/scalar_operation_inline.h:129 (declared vpu/scalar.h:62): two Newton steps on the vrefp
// estimate, each with its own one, and no final multiply (ARTIST BrnTraffic::FuzzyEnvelopeSet4::SetEnvelope
// 0x82752598: vcfsx(vspltisw 1, 0) 0x827526CC/0x827526E0, vrefp 0x8275270C/0x82752714, vnmsubfp + vmaddfp twice
// 0x82752724..0x82752740; world CR-32a, DWARF SetEnvelope cpp:60 lists Reciprocal).
inline VecFloat Reciprocal(VecFloat::InParam a)
{
    VectorIntrinsic result;
    {
        VectorIntrinsic secondEstimate;
        {
            VectorIntrinsic estimate, vOne;
            estimate = __vrefp(a.mV);
            vOne = __vcfsx(__vspltisw(1), 0);
            secondEstimate = __vmaddfp(estimate, __vnmsubfp(estimate, a.mV, vOne), estimate);
        }
        VectorIntrinsic vOne;
        vOne = __vcfsx(__vspltisw(1), 0);
        result = __vmaddfp(secondEstimate, __vnmsubfp(secondEstimate, a.mV, vOne), secondEstimate);
    }
    return VecFloat(result);
}
// rwmath 1.02.00 detail/scalar_operation_inline.h:136: one Newton step on the vrefp estimate,
// est + est * (1 - est * a) (ARTIST rw::collision::FeatureEdge::FeatureEdge 0x82BA8630..0x82BA8638:
// vrefp, vnmsubfp against vcfsx(vspltisw 1, 0), vmaddfp).
inline VecFloat ReciprocalFast(VecFloat::InParam a)
{
    VectorIntrinsic result;
    {
        VectorIntrinsic estimate, vOne;
        estimate = __vrefp(a.mV);
        vOne = __vcfsx(__vspltisw(1), 0);
        result = __vmaddfp(estimate, __vnmsubfp(estimate, a.mV, vOne), estimate);
    }
    return VecFloat(result);
}
// rwmath 1.02.00 detail/scalar_operation_inline.h:150: a * the once-refined vrsqrtefp estimate, zero for a
// zero input. Xenon refinement est + (est * 0.5) * (1 - a * est * est) as ARTIST FeatureEdge::FeatureEdge
// 0x82BA85FC..0x82BA8620 (vrsqrtefp; vmulfp est*est, est*half; vnmsubfp; vmaddfp; vmulfp a; vsel against
// the vcmpeqfp(zero, a) mask), the one-step form of Magnitude(Vector2)'s refinement (MainMapComponent::
// SnapToLocation 0x8245ECC4).
inline VecFloat SqrtFast(VecFloat::InParam a)
{
    VectorIntrinsic ret;
    {
        const VectorIntrinsic zero = __vspltisw(0);
        VectorIntrinsic isZeroMask;
        isZeroMask = __vcmpeqfp(zero, a.mV);
        VectorIntrinsic recipSqrt;
        {
            VectorIntrinsic estimate;
            estimate = __vrsqrtefp(a.mV);
            VectorIntrinsic vOne, vHalf;
            vOne = __vcfsx(__vspltisw(1), 0);
            vHalf = __vcfsx(__vspltisw(1), 1);
            recipSqrt = __vmaddfp(__vmulfp(estimate, vHalf), __vnmsubfp(a.mV, __vmulfp(estimate, estimate), vOne), estimate);
        }
        VectorIntrinsic result;
        result = __vmulfp(a.mV, recipSqrt);
        ret = __vsel(result, zero, isZeroMask);
    }
    return VecFloat(ret);
}
// rwmath 1.02.00 detail/scalar_operation_inline.h:287: vsel(falseValue, trueValue, mask) (ARTIST
// FeatureEdge::FeatureEdge 0x82BA8624 vsel v0,v8(zero),v0(sqrt),v9(mask)).
inline VecFloat Select(MaskScalar::InParam mask, VecFloat::InParam trueValue, VecFloat::InParam falseValue)
{
    VectorIntrinsic result;
    result = __vsel(falseValue.mV, trueValue.mV, mask.mV);
    return VecFloat(result);
}
// DecFIGS scalar.h / scalar_operation_inline.h: IsValid is !IsNaN, and the platform IsNaN
// is !_asmIsEqualV4(value, value) (LegacySrc ps3/ppu/scalar_operation_platform_inline.h).
// ARTIST 0x8270DFB8 (Vehicle::SetWheelRot's "RwMath::IsValid( lfValue )").
inline bool IsNaN(VecFloat::InParam value)
{
    return !_asmIsEqualV4(value.mV, value.mV);
}
inline bool IsValid(VecFloat::InParam value)
{
    return !IsNaN(value);
}
}}}

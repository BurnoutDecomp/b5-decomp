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

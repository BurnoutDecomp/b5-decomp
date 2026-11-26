#pragma once

#include "SDKs/EATech/include/rw/math/vpu/vec_float.h"
#include "SDKs/EATech/include/rw/math/vpu/maskscalar.h"
#include "SDKs/EATech/include/rw/math/vpu/vector_intrinsic_operation.h"
#include "SDKs/EATech/include/rw/math/vpu/detail/vec_float_operation_platform_inline.h"
#include "SDKs/EATech/include/rw/math/vpu/detail/scalar_operation_platform_inline.h"

namespace rw { namespace math { namespace vpu {
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

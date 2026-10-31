#pragma once

#include "SDKs/EATech/include/rw/math/vpu/vec_float.h"
#include "SDKs/EATech/include/rw/math/vpu/maskscalar.h"

namespace rw { namespace math { namespace vpu {
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
}}}

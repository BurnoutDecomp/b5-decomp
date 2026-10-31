#pragma once

#include "SDKs/EATech/include/rw/math/vpu/maskscalar.h"

namespace rw { namespace math { namespace vpu {
// DecFIGS maskscalar_operation_inline.h:22/29/36; full mask bits.
inline MaskScalar Not(MaskScalar::InParam value)
{ return MaskScalar(__vnor(value.mV, value.mV)); }
inline MaskScalar And(MaskScalar::InParam a, MaskScalar::InParam b)
{ return MaskScalar(__vand(a.mV, b.mV)); }
inline MaskScalar Or(MaskScalar::InParam a, MaskScalar::InParam b)
{ return MaskScalar(__vor(a.mV, b.mV)); }
}}}

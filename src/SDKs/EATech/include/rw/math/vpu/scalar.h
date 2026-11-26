#pragma once

#include "SDKs/EATech/include/rw/math/vpu/vec_float.h"
#include "SDKs/EATech/include/rw/math/vpu/maskscalar_operation.h"

namespace rw { namespace math { namespace vpu {
// DecFIGS vpu/scalar.h:37/39 (rwmath 1.02.00 scalar.h): the generic Min/Max take their
// operands by reference; float has a platform specialisation
// (detail/scalar_operation_platform_inline.h).
template <class T> inline T Min(const T& a, const T& b) { return (a < b? a : b); }
template <class T> inline T Max(const T& a, const T& b) { return (a > b? a : b); }
}}}

#include "SDKs/EATech/include/rw/math/vpu/detail/scalar_operation_inline.h"

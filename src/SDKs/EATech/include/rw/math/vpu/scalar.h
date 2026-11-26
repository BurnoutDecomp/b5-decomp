#pragma once

#include "SDKs/EATech/include/rw/math/vpu/vec_float.h"
#include "SDKs/EATech/include/rw/math/vpu/maskscalar_operation.h"

namespace rw { namespace math { namespace vpu {
// DecFIGS vpu/scalar.h:37/39 (rwmath 1.02.00 scalar.h): the generic Min/Max take their
// operands by reference; float has a platform specialisation
// (detail/scalar_operation_platform_inline.h).
template <class T> inline T Min(const T& a, const T& b) { return (a < b? a : b); }
template <class T> inline T Max(const T& a, const T& b) { return (a > b? a : b); }
// DecFIGS vpu/scalar.h:36 (rwmath 1.02.00 scalar.h): the generic Abs; VecFloat has the
// sign-mask specialisation in detail/scalar_operation_inline.h.
template <class T> inline T Abs(const T& a) { return a >= static_cast<T>(0.0f) ? a : -a; }
template <> inline VecFloat Abs<VecFloat>(const VecFloat& a);
// DecFIGS scalar_operation_inline.h (rwmath 1.02.00 :88/:105): VecFloat has the vminfp / vmaxfp
// specialisations of Min/Max.
template <> inline VecFloat Min<VecFloat>(const VecFloat& a, const VecFloat& b);
template <> inline VecFloat Max<VecFloat>(const VecFloat& a, const VecFloat& b);
}}}

#include "SDKs/EATech/include/rw/math/vpu/detail/scalar_operation_inline.h"

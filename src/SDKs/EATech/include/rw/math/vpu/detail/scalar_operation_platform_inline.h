#pragma once

// rwmath 1.02.00 rw/math/vpu/detail/<platform>/scalar_operation_platform_inline.h: the float
// specialisations of Min/Max (DecFIGS DWARF :18/:27, `float Min<float>(const float&,
// const float&)`). The PPU form is `test = a - b; fsel test, b, a` (Min) / `fsel test, a, b`
// (Max); the Xenon body is the same select through the XDK __fsel intrinsic.

#include "SDKs/EATech/include/rw/math/vpu/scalar.h"
// LegacySrc rwmath 1.02.00 ps3/ppu/scalar_operation_platform_inline.h:6: the platform scalar header
// brings in the VecFloat operator set.
#include "SDKs/EATech/include/rw/math/vpu/vec_float_operation.h"
#include <ppcintrinsics.h>

namespace rw
{
namespace math
{
namespace vpu
{
    template <>
    inline float Min<float>(const float& a, const float& b)
    {
        const float test = a - b;
        return static_cast<float>(__fsel(test, b, a));
    }

    template <>
    inline float Max<float>(const float& a, const float& b)
    {
        const float test = a - b;
        return static_cast<float>(__fsel(test, a, b));
    }
}
}
}

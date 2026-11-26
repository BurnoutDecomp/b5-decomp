#pragma once

// rwmath 1.02.00 rw/math/vpu/detail/vec_float_operation_platform_inline.h: the platform
// bodies of the VecFloat arithmetic (DecFIGS DWARF: Add(VecFloat, VecFloat) :517,
// Subtract(VecFloat, VecFloat) :662, Mult(VecFloat, VecFloat) :807). Each is one VMX
// operation on the broadcast registers; the Xenon forms are read from ARTIST's inlined
// copies (see the callers named at each body).

#include "SDKs/EATech/include/rw/math/vpu/vec_float.h"

namespace rw
{
namespace math
{
namespace vpu
{
    inline VecFloat Add(VecFloat::InParam a, VecFloat::InParam b)
    {
        return VecFloat(__vaddfp(a.mV, b.mV));
    }

    // ARTIST 0x82771684 (Lerp in AIAggression::GetMaxOvertakeSpeed): vsubfp b, a.
    inline VecFloat Subtract(VecFloat::InParam a, VecFloat::InParam b)
    {
        return VecFloat(__vsubfp(a.mV, b.mV));
    }

    inline VecFloat Mult(VecFloat::InParam a, VecFloat::InParam b)
    {
        return VecFloat(__vmulfp(a.mV, b.mV));
    }
}
}
}

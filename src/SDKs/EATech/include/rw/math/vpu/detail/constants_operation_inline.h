#pragma once

// rwmath 1.02.00 rw/math/vpu/detail/constants_operation_inline.h. DecFIGS DWARF places
// GetVecFloat_Zero at constants_operation_inline.h:110; the SDK body splats a zero word
// (vspltisw 0), as ARTIST's inlined copy shows (BrnTraffic ParamTransform::UpdateLerpedPosition
// 0x82712B58 `vspltisw v12,0` feeding the Max at 0x82712B78).

#include "SDKs/EATech/include/rw/math/vpu/vec_float.h"

namespace rw
{
namespace math
{
namespace vpu
{
    inline VecFloat GetVecFloat_Zero()
    {
        return VecFloat(__vspltisw(0));
    }
}
}
}

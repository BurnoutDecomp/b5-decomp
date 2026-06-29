#pragma once

// Portable PC reconstruction of EARenderWare rwmath (1.02.00)
// rw/math/vpu/detail/vec_float_type_platform_inline.h -- the two VecFloat methods the
// SDK keeps in the platform-specific inline file (so each platform can choose how the
// raw register is wrapped). On the console both are trivial register moves; lowered to
// portable lane copies here. Included by vec_float.h alongside the main inline file.

#include "SDKs/EATech/include/rw/math/vpu/vec_float.h"

namespace rw
{
namespace math
{
namespace vpu
{

inline VecFloat::VecFloat(VectorIntrinsicInParam v)
{
    // mV = v (whole-register copy)
    mV = v;
}

inline const VectorIntrinsic& VecFloat::GetVector() const
{
    return mV;
}

}
}
}

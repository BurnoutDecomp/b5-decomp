#pragma once

// rwmath 1.02.00 rw/math/vpu/constants.h: the constant generators the SDK builds in registers.
// Only the ones ARTIST shows are declared here; each body is in
// detail/constants_operation_inline.h at the SDK's own spelling.

#include "SDKs/EATech/include/rw/math/vpu/vec_float.h"

namespace rw
{
namespace math
{
namespace vpu
{
    inline VecFloat GetVecFloat_Zero();
}
}
}

#include "SDKs/EATech/include/rw/math/vpu/detail/constants_operation_inline.h"

#pragma once

#include "SDKs/EATech/include/rw/math/vpu/vec_float.h"
#include "SDKs/EATech/include/rw/math/vpu/vector_intrinsic_operation.h"

namespace rw
{
namespace math
{
namespace vpu
{
    // LegacySrc rwmath vec_float_operation_inline.h:155 and ARTIST 0x822772C4:
    // select the indexed lane in every word before comparing to the scalar.
    inline bool operator==(VecFloatRefIndex::InParam a, const float& b)
    {
        const VectorIntrinsic& lValue = a.GetVector();
        const VectorIntrinsic lPermutation = __vspltw(__lvsl(NULL, sizeof(float) * a.GetIndex()), 0);
        const VectorIntrinsic lBroadcast = __vperm(lValue, lValue, lPermutation);
        const VecFloat lScalar(b);
        return _asmIsEqualV4(lBroadcast, lScalar.GetVector());
    }
}
}
}

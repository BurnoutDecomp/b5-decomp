#pragma once

// Portable PC reconstruction of EARenderWare rwmath (1.02.00)
// rw/math/vpu/vector_axis.h. Trivial lane-index enum used to select one of the
// four lanes of a VectorIntrinsic. Values grounded against the Feb-2007 source
// (X=0, Y=1, Z=2, W=3); these are also the byte/word lane offsets the console
// AltiVec swizzle tables index by.

namespace rw
{
namespace math
{
namespace vpu
{
    enum VectorAxis
    {
        VectorAxisX = 0,
        VectorAxisY = 1,
        VectorAxisZ = 2,
        VectorAxisW = 3,
    };
}
}
}

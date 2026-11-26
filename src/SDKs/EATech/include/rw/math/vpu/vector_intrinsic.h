#pragma once

#include "types.hpp"
#include <vectorintrinsics.h>

// Native rwmath register storage. The genuine XDK intrinsic has16-byte size and
// alignment and provides x/y/z/w, v[4] and u[4] views; an aligned scalar aggregate
// does not have its parameter/return convention. VectorIntrinsicInParam is a
// vector value, as required at native register boundaries.
namespace rw
{
namespace math
{
namespace vpu
{
    typedef __vector4 VectorIntrinsic;
    typedef const VectorIntrinsic VectorIntrinsicInParam;
    typedef VectorIntrinsic Vector4Intrinsic_32;
    typedef VectorIntrinsicInParam Vector4Intrinsic_32_InParam;

    namespace detail
    {
        union VectorIntrinsicUnion
        {
            uint32_t        scalarUInt[4];
            float           scalarFloat[4];
            VectorIntrinsic vector;
        };

        // DecFIGS vector_intrinsic.h:37: the vperm controls of the runtime-lane swizzle store; entry
        // D * 4 + S copies lane S of the second operand into lane D of the first. ARTIST holds the table
        // at 0x8327F140 (.bss, filled by the rwmath dynamic initialiser 0x82C74120) and every
        // VecFloatRefIndex store indexes it (InterpedParam3::Construct 0x8259CD30).
        extern const VectorIntrinsic gSwizzleStoreConstants[16];
    }
}
}
}

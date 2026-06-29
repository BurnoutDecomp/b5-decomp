#pragma once

// Portable PC reconstruction of EARenderWare rwmath (1.02.00)
// rw/math/vpu/vector_intrinsic.h.
//
// On the X360 the SDK's `VectorIntrinsic` is the hardware `vector float` (a single
// 16-byte, 16-aligned VMX128 register holding four 32-bit lanes). Every operation in
// the vpu math layer is one or more AltiVec/VMX intrinsics on that register. None of
// those intrinsics exist on PC, so per the project's vendor-SIMD rule we model the
// register as a portable 4-lane struct with the *exact same memory layout* (16 bytes,
// 16-aligned, four contiguous 32-bit lanes) and lower each intrinsic to the equivalent
// scalar lane math elsewhere. Game structs that embed a VectorIntrinsic therefore match
// the console layout and compile on x64.
//
// The lanes are exposed as both float and uint32_t (the AltiVec ops freely reinterpret
// the register between the integer and float domains -- e.g. vspltisw builds an integer
// splat then vcfsx converts it to float). The union view `VectorIntrinsicUnion` is the
// SDK's named way to read/write individual lanes by index.

#include <cstdint>

namespace rw
{
namespace math
{
namespace vpu
{
    struct alignas(16) VectorIntrinsic
    {
        // Four 32-bit lanes. The console register is lane-ordered [X,Y,Z,W]
        // (big-endian element 0 == VectorAxisX); we keep that lane ordering so the
        // VectorAxis indices select the same logical component.
        float mafLane[4];
    };

    // The SDK passes the register by const-reference on PC-style ABIs (on the console it
    // is a register pass-by-value). The vpu method signatures are spelled in terms of
    // these typedefs, so we mirror them.
    typedef const VectorIntrinsic& VectorIntrinsicInParam;

    typedef VectorIntrinsic        Vector4Intrinsic_32;
    typedef VectorIntrinsicInParam Vector4Intrinsic_32_InParam;

    namespace detail
    {
        // Lane accessor union the SDK exposes for scalar read/write of one lane.
        // Same 16 bytes as the register; reading scalarFloat[i] / scalarUInt[i]
        // pulls lane i, writing it sets lane i.
        union VectorIntrinsicUnion
        {
            uint32_t        scalarUInt[4];
            float           scalarFloat[4];
            VectorIntrinsic vector;
        };
    }
}
}
}

#pragma once
#include <xtl.h>
#include <xboxmath.h>
#include "types.hpp"
namespace BrnParticleMath
{
    // Native scalar contraction emits fmadds; retain scalar denormal behavior.
    __forceinline f32 ScalarMultiplyAdd(f32 a, f32 b, f32 c) { return a * b + c; }
    // ARTIST vmaddfp/vmaddcfp128 sites use the real VMX operation and rounding.
    __forceinline f32 VpuMultiplyAdd(f32 a, f32 b, f32 c)
    {
        return __vmaddfp(XMVectorReplicate(a), XMVectorReplicate(b), XMVectorReplicate(c)).x;
    }
    __forceinline f32 VpuNegativeMultiplySubtract(f32 a, f32 b, f32 c)
    {
        return __vnmsubfp(XMVectorReplicate(a), XMVectorReplicate(b), XMVectorReplicate(c)).x;
    }
}

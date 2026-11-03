#pragma once

// BrnTraffic fuzzy-logic combinators over VecFloat (per-lane membership values in [0,1]).
// Declaration shape from the DecFIGS DWARF (SharedClasses/Traffic/BrnTrafficFuzzyLogic.h).
//
// In the ARTIST build these are folded inline at every call site (the ProcessParamRules
// asm emits the vminfp / vmaxfp / vsubfp directly with no `bl`), so they were inline
// helpers in the shared header. Standard fuzzy semantics, confirmed against the SIMD:
//   AND(a,b) = min(a,b)   (vminfp)
//   OR (a,b) = max(a,b)   (vmaxfp)
//   NOT(a)   = 1 - a      (vsubfp of the broadcast 1.0 produced by vspltisw/vcfsx)

#include "BrnCommonTypes.h"                  // VecFloat
#include "rw/math/vpu/vector4_operation.h"   // Min / Max / operator- / GetVector4_One

namespace BrnTraffic
{
    inline VecFloat FuzzyAND(VecFloat lLhs, VecFloat lRhs)
    {
        return VecFloat(__vminfp(lLhs.GetVector(), lRhs.GetVector()));
    }

    inline VecFloat FuzzyOR(VecFloat lLhs, VecFloat lRhs)
    {
        return VecFloat(__vmaxfp(lLhs.GetVector(), lRhs.GetVector()));
    }

    inline VecFloat FuzzyNOT(VecFloat lValue)
    {
        return VecFloat(__vsubfp(VecFloat(1.0f).GetVector(), lValue.GetVector()));
    }
}

#pragma once

#include <xtl.h>
#include <xboxmath.h>

// ARTIST 0x821F0C08 uses this scalar SDK routine, distinct from XMVectorSinCos.
namespace XboxMath
{
    inline void XMScalarSinCos(float* lpfSin, float* lpfCos, float lfValue)
    {
        ::XMScalarSinCos(lpfSin, lpfCos, lfValue);
    }
}

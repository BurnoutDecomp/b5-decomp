#pragma once

#include <xtl.h>
#include <xboxmath.h>

// ARTIST CameraRig::Construct 0x8220B0E8 inlines the native paired VMX routine.
namespace XboxMath
{
    inline void XMVectorSinCos(float* lpfSin, float* lpfCos, float lfValue)
    {
        XMVECTOR lvSin, lvCos;
        ::XMVectorSinCos(&lvSin, &lvCos, ::XMVectorReplicate(lfValue));
        *lpfSin = lvSin.x;
        *lpfCos = lvCos.x;
    }
}

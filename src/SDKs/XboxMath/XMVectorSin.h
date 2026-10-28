#pragma once

#include <xtl.h>
#include <xboxmath.h>

// ARTIST 0x821F05E0. Use the native SDK VMX implementation; callers consume lane X.
namespace XboxMath
{
    inline float XMVectorSin(float lfValue)
    {
        return ::XMVectorSin(::XMVectorReplicate(lfValue)).x;
    }
}

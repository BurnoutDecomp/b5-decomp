#pragma once

#include <xtl.h>
#include <xboxmath.h>

// ARTIST 0x821F06B0. Use the native SDK VMX implementation; callers consume lane X.
namespace XboxMath
{
    inline float XMVectorCos(float lfValue)
    {
        return ::XMVectorCos(::XMVectorReplicate(lfValue)).x;
    }
}

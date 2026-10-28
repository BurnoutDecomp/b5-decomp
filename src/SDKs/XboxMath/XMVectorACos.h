#pragma once

#include <xtl.h>
#include <xboxmath.h>

// ARTIST 0x821F0980. Use the native SDK VMX implementation; callers consume lane X.
namespace XboxMath
{
    inline float XMVectorACos(float lfValue)
    {
        return ::XMVectorACos(::XMVectorReplicate(lfValue)).x;
    }
}

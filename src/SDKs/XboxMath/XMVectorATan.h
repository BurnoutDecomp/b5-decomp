#pragma once

#include <xtl.h>
#include <xboxmath.h>

// ARTIST 0x821F0A70. Use the native SDK VMX implementation; callers consume lane X.
namespace XboxMath
{
    inline float XMVectorATan(float lfValue)
    {
        return ::XMVectorATan(::XMVectorReplicate(lfValue)).x;
    }
}

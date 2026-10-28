#pragma once

#include <xtl.h>
#include <xboxmath.h>

// ARTIST 0x821F0788. Use the native SDK VMX implementation; callers consume lane X.
namespace XboxMath
{
    inline float XMVectorTan(float lfValue)
    {
        return ::XMVectorTan(::XMVectorReplicate(lfValue)).x;
    }
}

#pragma once
#include "types.hpp"
#include <ppcintrinsics.h>

namespace BrnDirector { namespace Camera { namespace Utils {
// ARTIST82251A7C..82251AB0; declaration BrnSpeedResponder.h:43.
struct SpeedResponder
{
    void Construct() { mfSpeedRatio = 0.0f; }
    void Update(f32 lfSpeed, f32 lfMaxSpeed)
    {
        f32 lfRatio = lfSpeed / lfMaxSpeed;
        lfRatio = static_cast<f32>(__fsel(-lfRatio, 0.0f, lfRatio));
        mfSpeedRatio = static_cast<f32>(__fsel(1.0f - lfRatio, lfRatio, 1.0f));
    }
    f32 GetSpeedRatio() { return mfSpeedRatio; }
private:
    f32 mfSpeedRatio;
};
}}}

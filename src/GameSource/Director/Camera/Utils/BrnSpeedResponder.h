#pragma once
#include "types.hpp"
#include <cmath>
namespace BrnDirector { namespace Camera { namespace Utils {
// ARTIST 82251A7C..82251AB0, declaration shape BrnSpeedResponder.h:43.
struct SpeedResponder
{
    void Construct() { mfSpeedRatio = 0.0f; }
    void Update(f32 speed, f32 maxSpeed) { mfSpeedRatio = std::fmin(1.0f, std::fmax(0.0f, speed / maxSpeed)); }
    f32 GetSpeedRatio() { return mfSpeedRatio; }
private:
    f32 mfSpeedRatio;
};
}}}

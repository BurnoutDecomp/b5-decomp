#pragma once

// Native Xbox 360 RenderWare rwmath scalar (FPU) trigonometry, EARenderWare rwmath 1.02.00
// include/rw/math/fpu/trig.h (references/LegacySrc/BrnEntityModuleUnity/EARenderWare/stable/
// external/rwmath/1.02.00/...). DWARF lists `rw::math::fpu::DegToRad<float>(float angle_degrees)`
// (trig.h:21) in BrnWorldAIUnity.cpp, where BrnAIDriver.cpp's angle tunables are initialised with
// it. Only the angle conversions are carried here; Sin/Cos/Tan live in scalar_operation.h.

#include "rw/math/fpu/constants.h"

namespace rw
{
namespace math
{
namespace fpu
{
    template <class T> inline T DegToRad(T angle_degrees)
    {
        return angle_degrees * DEGREES_TO_RADIANS;
    }

    template <class T> inline T RadToDeg(T angle_radians)
    {
        return angle_radians * RADIANS_TO_DEGREES;
    }
}
}
}

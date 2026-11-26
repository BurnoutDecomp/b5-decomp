#pragma once

// Native Xbox 360 RenderWare rwmath scalar (FPU) constants, EARenderWare rwmath 1.02.00
// include/rw/math/fpu/constants.h (references/LegacySrc/BrnEntityModuleUnity/EARenderWare/stable/
// external/rwmath/1.02.00/...; DWARF SDKs/EATech/include/rw/math/fpu/constants.h:56/59). Only the
// angle conversion factors are carried so far (rw::math::fpu::DegToRad / RadToDeg, trig.h).

namespace rw
{
namespace math
{
namespace fpu
{
    const float DEGREES_TO_RADIANS = 0.01745329252f;   // constants.h:56
    const float RADIANS_TO_DEGREES = 57.29577951f;     // constants.h:59
}
}
}

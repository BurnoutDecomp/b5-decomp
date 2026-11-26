#pragma once
// Native Xbox 360 RenderWare rwmath scalar (FPU) constants, EARenderWare rwmath 1.02.00
// include/rw/math/fpu/constants.h (references/LegacySrc/BrnEntityModuleUnity/EARenderWare/stable/
// external/rwmath/1.02.00/...; DWARF SDKs/EATech/include/rw/math/fpu/constants.h:43/56/59). Only
// the constants a native caller needs so far are carried.
//
// Linkage: ARTIST holds each of these once, in the first object's .rdata and in this header's
// declaration order (0x82001738 MIN_FLOAT, 0x8200173C MAX_FLOAT, 0x82001740 SMALL_FLOAT,
// 0x82001744 DEGREES_TO_RADIANS, 0x82001748 RADIANS_TO_DEGREES, 0x8200174C PI, TWO_PI, HALF_PI,
// SQRT_2, ZERO_FLOAT, the two slerp angles, the slerp dot tolerance, MINIMUM_RECIPROCAL, EPSILON
// 0x82001770), and later translation units load that one copy (the camera behaviours read
// DEGREES_TO_RADIANS from 0x82001744) while still folding the value at compile time (BrnAIDriver.cpp's
// DegToRad tunables are folded .data, 0x82F3038C..; BrnWorld::KF_INVALID_DISTANCE = MAX_FLOAT is
// folded .data 0x82F302F4), and no unity TU's own .rdata block holds one (BrnWorldAIUnity's
// 0x820C3DDC.. has neither DEGREES_TO_RADIANS nor MAX_FLOAT). That is an external-linkage constant
// with a visible initializer, one COMDAT copy kept by the linker: the XDK's __declspec(selectany).
// A plain namespace-scope const would put a private copy into every referencing TU's .rdata block
// instead, moving that TU's literal-pool offsets.
namespace rw
{
namespace math
{
namespace fpu
{
    extern __declspec(selectany) const float MAX_FLOAT          = 3.402823466e+38f;   // constants.h:43
    extern __declspec(selectany) const float DEGREES_TO_RADIANS = 0.01745329252f;     // constants.h:56
    extern __declspec(selectany) const float RADIANS_TO_DEGREES = 57.29577951f;       // constants.h:59
}
}
}

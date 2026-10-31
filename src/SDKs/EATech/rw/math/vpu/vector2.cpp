#include "rw/math/vpu/types.h"

namespace rw { namespace math { namespace vpu {

// ARTIST821F0DB0: f1/f2, raw XY stores and zero ZW, then one whole-row copy.
Vector2::Vector2(float lfX, float lfY)
{
    __vector4 lValue;
    lValue.x = lfX;
    lValue.y = lfY;
    lValue.u[2] = 0;
    lValue.u[3] = 0;
    mV = lValue;
}

} } }

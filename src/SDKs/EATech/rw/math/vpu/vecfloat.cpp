#include "SDKs/EATech/include/rw/math/vpu/vec_float.h"

namespace rw { namespace math { namespace vpu {

// ARTIST821F0DA0: referenced scalar in r4; load its raw bits without an FPU
// conversion, broadcast word0 and store the native register at this in r3.
VecFloat::VecFloat(const float& lfValue)
{
    mV = __vspltw(__lvlx(&lfValue, 0), 0);
}

} } }

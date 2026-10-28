#include "RwMathVectorTemplates.h"

namespace rw { namespace math { namespace fpu {
template <>
Vector2Template<float>::Vector2Template(float lfX, float lfY)
    : mX(lfX),
      mY(lfY)
{
}
} } }

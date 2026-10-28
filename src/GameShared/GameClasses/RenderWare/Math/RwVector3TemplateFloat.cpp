#include "RwMathVectorTemplates.h"

namespace rw { namespace math { namespace fpu {
template <>
Vector3Template<float>::Vector3Template(float lfX, float lfY, float lfZ)
    : mX(lfX),
      mY(lfY),
      mZ(lfZ)
{
}
} } }

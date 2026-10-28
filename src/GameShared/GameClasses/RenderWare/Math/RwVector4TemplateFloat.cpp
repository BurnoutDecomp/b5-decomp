#include "RwMathVectorTemplates.h"

namespace rw { namespace math { namespace fpu {
template <>
Vector4Template<float>::Vector4Template(float lfX, float lfY, float lfZ, float lfW)
    : mX(lfX),
      mY(lfY),
      mZ(lfZ),
      mW(lfW)
{
}
} } }

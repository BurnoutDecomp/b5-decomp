#include "RwMathVectorTemplates.h"

namespace rw::math::fpu
{
template <>
Vector4Template<double>::Vector4Template(double lfX, double lfY, double lfZ, double lfW)
    : mX(lfX),
      mY(lfY),
      mZ(lfZ),
      mW(lfW)
{
}
}

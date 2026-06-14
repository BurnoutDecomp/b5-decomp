#include "RwMathVectorTemplates.h"

namespace rw::math::fpu
{
template <>
Vector3Template<double>::Vector3Template(double lfX, double lfY, double lfZ)
    : mX(lfX),
      mY(lfY),
      mZ(lfZ)
{
}
}

#include "RwMathVectorTemplates.h"

namespace rw::math::fpu
{
template <>
Vector2Template<double>::Vector2Template(double lfX, double lfY)
    : mX(lfX),
      mY(lfY)
{
}
}

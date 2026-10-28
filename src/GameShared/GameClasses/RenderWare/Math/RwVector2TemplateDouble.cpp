#include "RwMathVectorTemplates.h"

namespace rw { namespace math { namespace fpu {
template <>
Vector2Template<double>::Vector2Template(double lfX, double lfY)
    : mX(lfX),
      mY(lfY)
{
}
} } }

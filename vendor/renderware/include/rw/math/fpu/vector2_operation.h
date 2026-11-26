#pragma once

// RenderWare rwmath scalar (FPU) Vector2Template operations (EARenderWare rwmath 1.02.00,
// include/rw/math/fpu/vector2_operation.h; DecFIGS DWARF dwarfdump/SDKs/EATech/include/rw/math/fpu/
// vector2_operation.h). Only the forms the game's call sites use are reconstructed; they are
// templates, so nothing is emitted unless a caller instantiates them. The type itself lives in
// GameShared/GameClasses/RenderWare/Math/RwMathVectorTemplates.h (mX/mY behind X()/Y()).
//
// Evidence for the inlined shape: DecFIGS BrnAI::AStar::EuclideanDistance (PS3 0xA260C0) builds
// `lPosition0 - lPosition1` in a stack temporary and calls Magnitude<float>(Vector2Template<float>
// const&); ARTIST 0x8276E0D8 inlines both (fsubs x, fsubs y, fmuls y*y, fmadds x*x+.., fsqrts).

#include "rw/math/fpu/scalar_operation.h"
#include "GameShared/GameClasses/RenderWare/Math/RwMathVectorTemplates.h"

namespace rw
{
namespace math
{
namespace fpu
{
    // vector2_operation.h:136 (DWARF :139) -- lane-wise difference.
    template <class T>
    inline Vector2Template<T> operator-(const Vector2Template<T>& a, const Vector2Template<T>& b)
    {
        Vector2Template<T> r(a.X() - b.X(), a.Y() - b.Y());
        return r;
    }

    // vector2_operation.h:176 -- Sqrt(x*x + y*y).
    template <class T>
    inline T Magnitude(const Vector2Template<T>& vec)
    {
        return Sqrt(vec.X() * vec.X() + vec.Y() * vec.Y());
    }
}
}
}

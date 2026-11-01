#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/RenderWare/Math/RwMathVectorTemplates.h"

namespace BrnEffects
{
namespace Curves
{

// ARTIST823FD314/318 passes the packed records at +18/+24 to Evaluate.
// DecFIGS Curves.h:135/138 names these FPU Vector3/Vector2 members.
struct SmoothStep
{
    typedef rw::math::fpu::Vector3Template<f32> Vector3;
    typedef rw::math::fpu::Vector2Template<f32> Vector2;

    Vector3 mCurveParams;
    Vector2 mScaleFactors;

    // DecFIGS Curves.h:51; the table initializer copies all five scalar lanes.
    SmoothStep(const Vector3& lvParams, const Vector2& lvScale)
        : mCurveParams(lvParams), mScaleFactors(lvScale) {}

    static f32 Evaluate(const Vector2& lvParams, const Vector2& lvScale, f32 lfInput);
    // ARTIST8227D2F0: r3/r4 point to the FPU records; f1 is the input.
    static f32 Evaluate(const Vector3& lvParams, const Vector2& lvScale, f32 lfInput);
    f32 Evaluate(f32 lfInput) const;
};

CGS_STATIC_ASSERT((sizeof(SmoothStep) == 20), "ARTIST packed distance curve");
CGS_STATIC_ASSERT((__alignof(SmoothStep) == 4), "ARTIST scalar curve alignment");

}
}

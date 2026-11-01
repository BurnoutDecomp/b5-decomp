#include "GameSource/Effects/Curves.h"

namespace BrnEffects
{
namespace Curves
{

// ARTIST8227D2F0. Scalar operations retain the native branch/FMA order.
f32 SmoothStep::Evaluate(const Vector3& lvParams, const Vector2& lvScale, f32 lfInput)
{
    if (lfInput < lvParams.X())
        return lvScale.X();
    if (lfInput > lvParams.Z())
        return lvScale.Y();

    const f32 lfSpan = lvScale.Y() - lvScale.X();
    if (lfInput > lvParams.Y())
    {
        const f32 lfT = (lfInput - lvParams.Y()) / (lvParams.Z() - lvParams.Y());
        return (lfT * 0.5f + 0.5f) * lfSpan + lvScale.X();
    }
    const f32 lfT = (lfInput - lvParams.X()) / (lvParams.Y() - lvParams.X());
    return (lfT * 0.5f) * lfSpan + lvScale.X();
}

// Inlined at ARTIST823FD314..330.
f32 SmoothStep::Evaluate(f32 lfInput) const
{
    return Evaluate(mCurveParams, mScaleFactors, lfInput);
}

}
}

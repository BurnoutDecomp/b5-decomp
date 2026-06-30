// Embed/ODR + template-instantiation gate for the vpu Vector3 / Vector4 free-function
// operation inline bodies (vector3_operation_inline.h, vector4_operation_inline.h).
// Self-contained from a foreign TU: includes only the public headers and forces every
// bodied free function (including the templated VecFloatRef<INDEX> overloads) to instantiate,
// so the compile gate actually covers the bodies -- a header alone is a vacuous `cl /c` pass.
//
// Only the FAITHFULLY-RECONSTRUCTED (bodied) functions are exercised here. The
// DECLARATION-ONLY + FLAGGED entries (operator==/!=, IsZero, IsSimilar, IsValid, the
// Comp*/Select Mask families, CosAngleBetweenNormalizedVectors / AngleBetweenVectors,
// SLerp) are intentionally NOT called: they have no body in this cluster, so calling them
// would be an unresolved-symbol link error, not a gate of a real body.

#include "SDKs/EATech/include/rw/math/vpu/vector3.h"
#include "SDKs/EATech/include/rw/math/vpu/vector4.h"

using namespace rw::math::vpu;

float Vector3_OperationEmbedCheckEntry(VectorIntrinsic& reg)
{
    VecFloat s(2.0f);
    VecFloatRef<VectorAxisX> rx(reg);
    VecFloatRefIndex ri(reg, VectorAxisZ);

    Vector3 a(1.0f, 2.0f, 3.0f);
    Vector3 b(4.0f, 5.0f, 6.0f);
    Vector3 c(7.0f, 8.0f, 9.0f);
    Vector3 t;

    // Named arithmetic
    Vector3 r = Add(a, b);
    r = Subtract(a, b);
    r = Mult(a, b);
    r = MultAdd(a, b, c);
    r = Mult(a, s);
    r = Mult(a, 3.0f);
    r = Mult(a, rx);
    r = Mult(a, ri);
    r = Divide(a, s);
    r = Divide(a, 3.0f);
    r = Divide(a, rx);
    r = Divide(a, ri);

    // Dot / Cross
    VecFloat d = Dot(a, b);
    r = Cross(a, b);

    // Magnitude family
    d = Magnitude(a);
    d = MagnitudeSquared(a);
    d = MagnitudeRecip(a);

    // Normalize family
    r = NormalizeEstimate(a);
    r = NormalizeFast(a);
    r = Normalize(a);
    VecFloat mag;
    r = Normalize(a, mag);
    d = NormalizeReturnMagnitude(a, t);

    // Lerp / NLerp
    r = Lerp(a, b, s);
    r = NLerpEstimate(a, b, s);
    r = NLerpFast(a, b, s);
    r = NLerp(a, b, s);

    // Negate / Min / Max / Clamp / Abs
    r = Negate(a);
    r = Min(a, b);
    r = Max(a, b);
    r = Clamp(a, b, c);
    r = Abs(a);
    bool nan = IsAnyNaN(a);

    // operators: VecFloat
    t = a; t *= s; t /= s; t += s; t -= s;
    r = a * s; r = s * a; r = a / s; r = a + s; r = a - s; r = s + a; r = s - a;
    // operators: float
    t = a; t *= 2.0f; t /= 2.0f;
    r = a * 2.0f; r = 2.0f * a; r = a / 2.0f; r = a + 2.0f; r = a - 2.0f; r = 2.0f + a; r = 2.0f - a;
    // operators: VecFloatRef<INDEX>
    t = a; t *= rx; t /= rx; t += rx; t -= rx;
    r = a * rx; r = rx * a; r = a / rx; r = a + rx; r = a - rx; r = rx + a; r = rx - a;
    // operators: VecFloatRefIndex
    t = a; t *= ri; t /= ri; t += ri; t -= ri;
    r = a * ri; r = ri * a; r = a / ri; r = a + ri; r = a - ri; r = ri + a; r = ri - a;
    // vector-vector
    r = -a;
    t = a; t += b; t -= b; t *= b; t /= b;
    r = a * b; r = a / b; r = a - b; r = a + b;

    // void out-param variants
    Negate(a, t);
    Mult(a, s, t); Mult(a, rx, t); Mult(a, ri, t);
    Divide(a, s, t); Divide(a, rx, t); Divide(a, ri, t);
    Add(a, b, t); Subtract(a, b, t); Cross(a, b, t); Mult(a, b, t);
    Normalize(a, t);
    Lerp(a, b, s, t); Lerp(a, b, rx, t); Lerp(a, b, ri, t);
    NLerp(a, b, s, t); NLerp(a, b, rx, t); NLerp(a, b, ri, t);
    VecFloat dd; Dot(a, b, dd);

    return r.X().GetFloat() + d.GetFloat() + mag.GetFloat() + dd.GetFloat()
         + (nan ? 1.0f : 0.0f) + t.Y().GetFloat();
}

float Vector4_OperationEmbedCheckEntry(VectorIntrinsic& reg)
{
    VecFloat s(2.0f);
    VecFloatRef<VectorAxisX> rx(reg);
    VecFloatRefIndex ri(reg, VectorAxisW);

    Vector4 a(1.0f, 2.0f, 3.0f, 4.0f);
    Vector4 b(5.0f, 6.0f, 7.0f, 8.0f);
    Vector4 c(9.0f, 10.0f, 11.0f, 12.0f);
    Vector4 t;

    // Named arithmetic
    Vector4 r = Add(a, b);
    r = Subtract(a, b);
    r = Mult(a, b);
    r = MultAdd(a, b, c);
    r = Mult(a, s);
    r = Mult(a, 3.0f);
    r = Mult(a, rx);
    r = Mult(a, ri);
    r = Divide(a, s);
    r = Divide(a, 3.0f);
    r = Divide(a, rx);
    r = Divide(a, ri);
    r = DivideByW(a);

    // Dot / full-4 component mult/divide operators
    VecFloat d = Dot(a, b);
    r = a * b;     // component-wise Vector4*Vector4
    r = a / b;     // component-wise Vector4/Vector4

    // Magnitude family (full-4)
    d = Magnitude(a);
    d = MagnitudeSquared(a);
    d = MagnitudeRecip(a);

    // Normalize family (full-4)
    r = NormalizeEstimate(a);
    r = NormalizeFast(a);
    r = Normalize(a);
    VecFloat mag;
    r = Normalize(a, mag);
    d = NormalizeReturnMagnitude(a, t);

    // Lerp / NLerp
    r = Lerp(a, b, s);
    r = NLerpEstimate(a, b, s);
    r = NLerpFast(a, b, s);
    r = NLerp(a, b, s);

    // SetXYZ / GetXYZ
    SetXYZ(t, s, s, s);
    SetXYZ(t, rx, rx, rx);
    SetXYZ(t, ri, ri, ri);
    const Vector3& xyz = GetXYZ(a);

    // *XYZ family
    d = MagnitudeXYZ(a);
    d = MagnitudeSquaredXYZ(a);
    d = MagnitudeRecipXYZ(a);
    d = DotXYZ(a, b);
    r = CrossXYZ(a, b);

    // Negate / Min / Max / Clamp / Abs
    r = Negate(a);
    r = Min(a, b);
    r = Max(a, b);
    r = Clamp(a, b, c);
    r = Abs(a);
    bool nan = IsAnyNaN(a);

    // operators: VecFloat
    t = a; t *= s; t /= s; t += s; t -= s;
    r = a * s; r = s * a; r = a / s; r = a + s; r = a - s; r = s + a; r = s - a;
    // operators: float
    t = a; t *= 2.0f; t /= 2.0f;
    r = a * 2.0f; r = 2.0f * a; r = a / 2.0f; r = a + 2.0f; r = a - 2.0f; r = 2.0f + a; r = 2.0f - a;
    // operators: VecFloatRef<INDEX>
    t = a; t *= rx; t /= rx; t += rx; t -= rx;
    r = a * rx; r = rx * a; r = a / rx; r = a + rx; r = a - rx; r = rx + a; r = rx - a;
    // operators: VecFloatRefIndex
    t = a; t *= ri; t /= ri; t += ri; t -= ri;
    r = a * ri; r = ri * a; r = a / ri; r = a + ri; r = a - ri; r = ri + a; r = ri - a;
    // vector-vector
    r = -a;
    t = a; t += b; t -= b; t *= b; t /= b;
    r = a - b; r = a + b;

    // void out-param variants
    Negate(a, t);
    Mult(a, s, t); Mult(a, rx, t); Mult(a, ri, t);
    Divide(a, s, t);
    Add(a, b, t); Subtract(a, b, t); Mult(a, b, t);
    DotXYZ(a, b, mag); CrossXYZ(a, b, t);
    Normalize(a, t);
    Lerp(a, b, s, t); NLerp(a, b, s, t);
    VecFloat dd; Dot(a, b, dd);

    return r.X().GetFloat() + d.GetFloat() + mag.GetFloat() + dd.GetFloat()
         + xyz.X().GetFloat() + (nan ? 1.0f : 0.0f) + t.W().GetFloat();
}

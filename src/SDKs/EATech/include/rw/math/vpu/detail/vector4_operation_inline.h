#pragma once

// VPU Vector4 operations on the single SDK class definition.
// Recovered VMX primitives retain all four lanes; unrecovered scalar algorithms
// and declaration-only helpers below remain separate native-recovery work.

#include <cmath>

#include "SDKs/EATech/include/rw/math/vpu/vector4.h"

namespace rw
{
namespace math
{
namespace vpu
{

// =====================================================================================
//  Equality / validity / similarity -- DECLARATION-ONLY + FLAGGED
// =====================================================================================

bool operator == (Vector4::InParam a, Vector4::InParam b);                 // FLAGGED: _asmIsEqualV4
bool operator != (Vector4::InParam a, Vector4::InParam b);                 // FLAGGED: _asmIsEqualV4
bool IsZero(Vector4::InParam v, VecFloat::InParam tolerance);              // FLAGGED: _asmIsZeroV4
bool IsSimilar(Vector4::InParam a, Vector4::InParam b, VecFloat::InParam tolerance); // FLAGGED: _asmIsSimilarV4
bool IsValid(Vector4::InParam v);                                         // FLAGGED: needs IsValid(VecFloat)

// IsAnyNaN -- FAITHFUL. vcmpeqfp_p(v,v): a NaN lane fails self-comparison.
inline bool IsAnyNaN(Vector4::InParam v)
{
    return !( v.mV.v[0] == v.mV.v[0]
           && v.mV.v[1] == v.mV.v[1]
           && v.mV.v[2] == v.mV.v[2]
           && v.mV.v[3] == v.mV.v[3] );
}

// =====================================================================================
//  Negate / Min / Max / Clamp / Abs
// =====================================================================================

inline Vector4 Negate(Vector4::InParam v)
{
    const VectorIntrinsic allOnes = __vspltisw(-1);
    const VectorIntrinsic sign = __vslw(allOnes, allOnes);
    return Vector4(__vxor(v.mV, sign));
}

inline Vector4 Min(Vector4::InParam v0, Vector4::InParam v1)
{
    return Vector4(__vminfp(v0.mV, v1.mV));
}

inline Vector4 Max(Vector4::InParam v0, Vector4::InParam v1)
{
    return Vector4(__vmaxfp(v0.mV, v1.mV));
}

inline Vector4 Clamp(Vector4::InParam v0, Vector4::InParam v1, Vector4::InParam v2)
{
    // r = vmaxfp(v0,v1); r = vminfp(r,v2).
    return Min(Max(v0, v1), v2);
}

inline Vector4 Abs(Vector4::InParam v)
{
    const VectorIntrinsic allOnes = __vspltisw(-1);
    return Vector4(__vandc(v.mV, __vslw(allOnes, allOnes)));
}

// =====================================================================================
//  Dot (full-4) + scalar Mult / Divide
// =====================================================================================

inline VecFloat Dot(Vector4::InParam a, Vector4::InParam b)
{
    // xx_yy_zz_ww = vmaddfp(b,a,0); two vsldoi rotations + vaddfp sum all four lanes.
    const float lfDot = a.mV.v[0] * b.mV.v[0]
                      + a.mV.v[1] * b.mV.v[1]
                      + a.mV.v[2] * b.mV.v[2]
                      + a.mV.v[3] * b.mV.v[3];
    return VecFloat(lfDot);
}

inline Vector4 Mult(Vector4::InParam v, VecFloat::InParam s)
{
    const float a = s.mV.v[VectorAxisX];
    Vector4 r;
    for (int k = 0; k < 4; ++k) r.mV.v[k] = v.mV.v[k] * a;
    return r;
}

inline Vector4 operator * (Vector4::InParam v, Vector4::InParam s)
{
    // vmaddfp(v,s,0): per-lane multiply (component-wise).
    Vector4 r;
    for (int k = 0; k < 4; ++k) r.mV.v[k] = v.mV.v[k] * s.mV.v[k];
    return r;
}

inline Vector4 operator / (Vector4::InParam v, Vector4::InParam s)
{
    // reciprocal of s (vrefp + Newton) then vmaddfp(reciprocal, v, 0): per-lane divide.
    Vector4 r;
    for (int k = 0; k < 4; ++k) r.mV.v[k] = v.mV.v[k] / s.mV.v[k];
    return r;
}

inline Vector4 Mult(Vector4::InParam v, float s)
{
    Vector4 r;
    for (int k = 0; k < 4; ++k) r.mV.v[k] = v.mV.v[k] * s;
    return r;
}

template<VectorAxis INDEX>
inline Vector4 Mult(Vector4::InParam v, VecFloatRef<INDEX> s)
{
    const float a = s.mV.v[INDEX];
    Vector4 r;
    for (int k = 0; k < 4; ++k) r.mV.v[k] = v.mV.v[k] * a;
    return r;
}

inline Vector4 Mult(Vector4::InParam v, VecFloatRefIndex::InParam s)
{
    const float a = s.mV.v[s.mIndex];
    Vector4 r;
    for (int k = 0; k < 4; ++k) r.mV.v[k] = v.mV.v[k] * a;
    return r;
}

inline Vector4 Divide(Vector4::InParam v, VecFloat::InParam s)
{
    const float recip = 1.0f / s.mV.v[VectorAxisX];
    Vector4 r;
    for (int k = 0; k < 4; ++k) r.mV.v[k] = v.mV.v[k] * recip;
    return r;
}

inline Vector4 Divide(Vector4::InParam v, float s)
{
    const float recip = 1.0f / s;
    Vector4 r;
    for (int k = 0; k < 4; ++k) r.mV.v[k] = v.mV.v[k] * recip;
    return r;
}

template<VectorAxis INDEX>
inline Vector4 Divide(Vector4::InParam v, VecFloatRef<INDEX> s)
{
    const float recip = 1.0f / s.mV.v[INDEX];
    Vector4 r;
    for (int k = 0; k < 4; ++k) r.mV.v[k] = v.mV.v[k] * recip;
    return r;
}

inline Vector4 Divide(Vector4::InParam v, VecFloatRefIndex::InParam s)
{
    const float recip = 1.0f / s.mV.v[s.mIndex];
    Vector4 r;
    for (int k = 0; k < 4; ++k) r.mV.v[k] = v.mV.v[k] * recip;
    return r;
}

// =====================================================================================
//  Add / Subtract / Mult / MultAdd / DivideByW
// =====================================================================================

inline Vector4 Add(Vector4::InParam a, Vector4::InParam b)
{
    return Vector4(__vaddfp(a.mV, b.mV));
}

inline Vector4 Subtract(Vector4::InParam a, Vector4::InParam b)
{
    return Vector4(__vsubfp(a.mV, b.mV));
}

inline Vector4 Mult(Vector4::InParam a, Vector4::InParam b)
{
    return Vector4(__vmulfp(a.mV, b.mV));
}

inline Vector4 MultAdd(Vector4::InParam a, Vector4::InParam b, Vector4::InParam c)
{
    return Vector4(__vmaddfp(a.mV, b.mV, c.mV));
}

inline Vector4 DivideByW(Vector4::InParam v)
{
    // splattedAxis = vspltw(v, W); reciprocal = 1/W (vrefp + Newton); vmaddfp(reciprocal,v,0).
    const float recip = 1.0f / v.mV.v[VectorAxisW];
    Vector4 r;
    for (int k = 0; k < 4; ++k) r.mV.v[k] = v.mV.v[k] * recip;
    return r;
}

// =====================================================================================
//  Magnitude family (full-4)
// =====================================================================================

inline VecFloat MagnitudeSquared(Vector4::InParam v)
{
    const float lfDot = v.mV.v[0] * v.mV.v[0]
                      + v.mV.v[1] * v.mV.v[1]
                      + v.mV.v[2] * v.mV.v[2]
                      + v.mV.v[3] * v.mV.v[3];
    return VecFloat(lfDot);
}

inline VecFloat MagnitudeRecip(Vector4::InParam v)
{
    const float lfDot = v.mV.v[0] * v.mV.v[0]
                      + v.mV.v[1] * v.mV.v[1]
                      + v.mV.v[2] * v.mV.v[2]
                      + v.mV.v[3] * v.mV.v[3];
    return VecFloat(1.0f / sqrtf(lfDot));
}

inline VecFloat Magnitude(Vector4::InParam v)
{
    // dot * (1/sqrt(dot)) == sqrt(dot); vsel guards a zero dot to zero.
    const float lfDot = v.mV.v[0] * v.mV.v[0]
                      + v.mV.v[1] * v.mV.v[1]
                      + v.mV.v[2] * v.mV.v[2]
                      + v.mV.v[3] * v.mV.v[3];
    return VecFloat(lfDot == 0.0f ? 0.0f : lfDot * (1.0f / sqrtf(lfDot)));
}

// =====================================================================================
//  Normalize family (full-4)
// =====================================================================================

inline Vector4 NormalizeEstimate(Vector4::InParam v)
{
    const float lfDot = v.mV.v[0] * v.mV.v[0]
                      + v.mV.v[1] * v.mV.v[1]
                      + v.mV.v[2] * v.mV.v[2]
                      + v.mV.v[3] * v.mV.v[3];
    const float lfScale = 1.0f / sqrtf(lfDot);
    Vector4 r;
    for (int k = 0; k < 4; ++k) r.mV.v[k] = v.mV.v[k] * lfScale;
    return r;
}

inline Vector4 NormalizeFast(Vector4::InParam v)
{
    const float lfDot = v.mV.v[0] * v.mV.v[0]
                      + v.mV.v[1] * v.mV.v[1]
                      + v.mV.v[2] * v.mV.v[2]
                      + v.mV.v[3] * v.mV.v[3];
    const float lfScale = 1.0f / sqrtf(lfDot);
    Vector4 r;
    for (int k = 0; k < 4; ++k) r.mV.v[k] = v.mV.v[k] * lfScale;
    return r;
}

inline Vector4 Normalize(Vector4::InParam v)
{
    const float lfDot = v.mV.v[0] * v.mV.v[0]
                      + v.mV.v[1] * v.mV.v[1]
                      + v.mV.v[2] * v.mV.v[2]
                      + v.mV.v[3] * v.mV.v[3];
    const float lfScale = 1.0f / sqrtf(lfDot);
    Vector4 r;
    for (int k = 0; k < 4; ++k) r.mV.v[k] = v.mV.v[k] * lfScale;
    return r;
}

inline Vector4 Normalize(Vector4::InParam v, VecFloat& magnitude)
{
    const float lfDot = v.mV.v[0] * v.mV.v[0]
                      + v.mV.v[1] * v.mV.v[1]
                      + v.mV.v[2] * v.mV.v[2]
                      + v.mV.v[3] * v.mV.v[3];
    const float lfScale = 1.0f / sqrtf(lfDot);
    Vector4 r;
    for (int k = 0; k < 4; ++k) r.mV.v[k] = v.mV.v[k] * lfScale;
    magnitude = VecFloat(lfDot == 0.0f ? 0.0f : lfDot * lfScale);
    return r;
}

inline VecFloat NormalizeReturnMagnitude(Vector4::InParam v, Vector4& result)
{
    const float lfDot = v.mV.v[0] * v.mV.v[0]
                      + v.mV.v[1] * v.mV.v[1]
                      + v.mV.v[2] * v.mV.v[2]
                      + v.mV.v[3] * v.mV.v[3];
    const float lfScale = 1.0f / sqrtf(lfDot);
    for (int k = 0; k < 4; ++k) result.mV.v[k] = v.mV.v[k] * lfScale;
    return VecFloat(lfDot == 0.0f ? 0.0f : lfDot * lfScale);
}

// =====================================================================================
//  Lerp / NLerp (full-4)
// =====================================================================================

inline Vector4 Lerp(Vector4::InParam v0, Vector4::InParam v1, VecFloat::InParam t)
{
    // temp = vsubfp(v1,v0); ret = vmaddfp(temp, t, v0).
    const float lfT = t.mV.v[VectorAxisX];
    Vector4 r;
    for (int k = 0; k < 4; ++k) r.mV.v[k] = (v1.mV.v[k] - v0.mV.v[k]) * lfT + v0.mV.v[k];
    return r;
}

inline Vector4 NLerpEstimate(Vector4::InParam v0, Vector4::InParam v1, VecFloat::InParam t)
{
    return NormalizeEstimate(Lerp(v0, v1, t));
}

inline Vector4 NLerpFast(Vector4::InParam v0, Vector4::InParam v1, VecFloat::InParam t)
{
    return NormalizeFast(Lerp(v0, v1, t));
}

inline Vector4 NLerp(Vector4::InParam v0, Vector4::InParam v1, VecFloat::InParam t)
{
    return Normalize(Lerp(v0, v1, t));
}

// =====================================================================================
//  SetXYZ / GetXYZ
// =====================================================================================

inline void SetXYZ(Vector4& v, VecFloat::InParam x, VecFloat::InParam y, VecFloat::InParam z)
{
    // vperm assemble of x.X / y.Y / z.Z into lanes 0/1/2 (vsldoi keeps the existing W lane).
    v.mV.v[0] = x.mV.v[VectorAxisX];
    v.mV.v[1] = y.mV.v[VectorAxisY];
    v.mV.v[2] = z.mV.v[VectorAxisZ];
}

template<VectorAxis INDEX0, VectorAxis INDEX1, VectorAxis INDEX2>
inline void SetXYZ(Vector4& v, VecFloatRef<INDEX0> x, VecFloatRef<INDEX1> y, VecFloatRef<INDEX2> z)
{
    v.mV.v[0] = x.mV.v[INDEX0];
    v.mV.v[1] = y.mV.v[INDEX1];
    v.mV.v[2] = z.mV.v[INDEX2];
}

inline void SetXYZ(Vector4& v, VecFloatRefIndex::InParam x, VecFloatRefIndex::InParam y, VecFloatRefIndex::InParam z)
{
    // Three single-lane swizzle-stores into lanes X/Y/Z; W preserved.
    v.mV.v[VectorAxisX] = x.mV.v[x.mIndex];
    v.mV.v[VectorAxisY] = y.mV.v[y.mIndex];
    v.mV.v[VectorAxisZ] = z.mV.v[z.mIndex];
}

inline const Vector3& GetXYZ(const Vector4& v)
{
    // The Vector3 and Vector4 register layouts coincide on lanes [X,Y,Z]; the SDK reads the
    // Vector4 register through a Vector3 reference (lane W ignored by Vector3 readers).
    return *reinterpret_cast<const Vector3*>(&v);
}

// =====================================================================================
//  *XYZ ops -- treat the Vector4 as a Vector3 (lanes X,Y,Z)
// =====================================================================================

inline VecFloat MagnitudeSquaredXYZ(Vector4::InParam v)
{
    const float lfDot = v.mV.v[0] * v.mV.v[0]
                      + v.mV.v[1] * v.mV.v[1]
                      + v.mV.v[2] * v.mV.v[2];
    return VecFloat(lfDot);
}

inline VecFloat MagnitudeRecipXYZ(Vector4::InParam v)
{
    const float lfDot = v.mV.v[0] * v.mV.v[0]
                      + v.mV.v[1] * v.mV.v[1]
                      + v.mV.v[2] * v.mV.v[2];
    return VecFloat(1.0f / sqrtf(lfDot));
}

inline VecFloat MagnitudeXYZ(Vector4::InParam v)
{
    const float lfDot = v.mV.v[0] * v.mV.v[0]
                      + v.mV.v[1] * v.mV.v[1]
                      + v.mV.v[2] * v.mV.v[2];
    return VecFloat(lfDot == 0.0f ? 0.0f : lfDot * (1.0f / sqrtf(lfDot)));
}

inline VecFloat DotXYZ(Vector4::InParam v0, Vector4::InParam v1)
{
    const float lfDot = v0.mV.v[0] * v1.mV.v[0]
                      + v0.mV.v[1] * v1.mV.v[1]
                      + v0.mV.v[2] * v1.mV.v[2];
    return VecFloat(lfDot);
}

inline Vector4 CrossXYZ(Vector4::InParam v0, Vector4::InParam v1)
{
    // gCrossProductPermuteConstant ([Y,Z,X]) cross over lanes X,Y,Z -- the canonical
    // y*z-z*y / z*x-x*z / x*y-y*x. The W lane is left as the permuted residue; callers of
    // CrossXYZ consume only lanes X,Y,Z. We write the three meaningful lanes and leave W
    // untouched (uninitialised, matching the console's don't-care W).
    const float ax = v0.mV.v[0], ay = v0.mV.v[1], az = v0.mV.v[2];
    const float bx = v1.mV.v[0], by = v1.mV.v[1], bz = v1.mV.v[2];
    Vector4 r;
    r.mV.v[0] = ay * bz - az * by;
    r.mV.v[1] = az * bx - ax * bz;
    r.mV.v[2] = ax * by - ay * bx;
    r.mV.v[3] = 0.0f;
    return r;
}

// =====================================================================================
//  Comparison masks / Select -- OMITTED + FLAGGED
//
//  CompLessThan / CompGreaterThan / CompLessEqual / CompGreaterEqual / CompEqual /
//  CompNotEqual return a Mask4, and the two Select overloads consume a Mask4 / MaskScalar.
//  Those mask types are NOT committed in this cluster, so even a forward declaration here
//  would not parse. Omitted rather than fabricated; bodies (vcmpgefp/vcmpgtfp/vcmpeqfp +
//  vnor mask, vsel select) can be added once the mask types are homed.
// =====================================================================================

// =====================================================================================
//  Compound assignment + operators (VecFloat / float / VecFloatRef / VecFloatRefIndex)
// =====================================================================================

// ---- v op= VecFloat ---------------------------------------------------------------

inline Vector4& operator *= (Vector4& v, VecFloat::InParam a)
{
    const float s = a.mV.v[VectorAxisX];
    for (int k = 0; k < 4; ++k) v.mV.v[k] *= s;
    return v;
}

inline Vector4& operator /= (Vector4& v, VecFloat::InParam a)
{
    const float recip = 1.0f / a.mV.v[VectorAxisX];
    for (int k = 0; k < 4; ++k) v.mV.v[k] *= recip;
    return v;
}

inline Vector4& operator += (Vector4& v, VecFloat::InParam a)
{
    // vaddfp(v, a): a is a broadcast register, so every lane adds the same scalar.
    for (int k = 0; k < 4; ++k) v.mV.v[k] += a.mV.v[k];
    return v;
}

inline Vector4& operator -= (Vector4& v, VecFloat::InParam a)
{
    for (int k = 0; k < 4; ++k) v.mV.v[k] -= a.mV.v[k];
    return v;
}

inline Vector4 operator * (Vector4::InParam v, VecFloat::InParam s) { return Mult(v, s); }
inline Vector4 operator * (VecFloat::InParam s, Vector4::InParam v) { return Mult(v, s); }
inline Vector4 operator / (Vector4::InParam v, VecFloat::InParam s) { return Divide(v, s); }

inline Vector4 operator + (Vector4::InParam v, VecFloat::InParam s)
{
    // vaddfp(v, s): broadcast register, every lane adds s.
    Vector4 r;
    for (int k = 0; k < 4; ++k) r.mV.v[k] = v.mV.v[k] + s.mV.v[k];
    return r;
}

inline Vector4 operator - (Vector4::InParam v, VecFloat::InParam s)
{
    Vector4 r;
    for (int k = 0; k < 4; ++k) r.mV.v[k] = v.mV.v[k] - s.mV.v[k];
    return r;
}

inline Vector4 operator + (VecFloat::InParam s, Vector4::InParam v)
{
    Vector4 r;
    for (int k = 0; k < 4; ++k) r.mV.v[k] = v.mV.v[k] + s.mV.v[k];
    return r;
}

inline Vector4 operator - (VecFloat::InParam s, Vector4::InParam v)
{
    // vsubfp(s, v): scalar register minus each lane.
    Vector4 r;
    for (int k = 0; k < 4; ++k) r.mV.v[k] = s.mV.v[k] - v.mV.v[k];
    return r;
}

// ---- v op= float ------------------------------------------------------------------

inline Vector4& operator *= (Vector4& v, float a)
{
    for (int k = 0; k < 4; ++k) v.mV.v[k] *= a;
    return v;
}

inline Vector4& operator /= (Vector4& v, float a)
{
    const float recip = 1.0f / a;
    for (int k = 0; k < 4; ++k) v.mV.v[k] *= recip;
    return v;
}

inline Vector4 operator * (Vector4::InParam v, float s) { return Mult(v, s); }
inline Vector4 operator * (float s, Vector4::InParam v) { return Mult(v, s); }
inline Vector4 operator / (Vector4::InParam v, float s) { return Divide(v, s); }

inline Vector4 operator + (Vector4::InParam v, float s)
{
    // broadcast(s) then vaddfp.
    Vector4 r;
    for (int k = 0; k < 4; ++k) r.mV.v[k] = v.mV.v[k] + s;
    return r;
}

inline Vector4 operator - (Vector4::InParam v, float s)
{
    Vector4 r;
    for (int k = 0; k < 4; ++k) r.mV.v[k] = v.mV.v[k] - s;
    return r;
}

inline Vector4 operator + (float s, Vector4::InParam v) { return v + s; }

inline Vector4 operator - (float s, Vector4::InParam v)
{
    Vector4 r;
    for (int k = 0; k < 4; ++k) r.mV.v[k] = s - v.mV.v[k];
    return r;
}

// ---- v op= VecFloatRef<INDEX> -----------------------------------------------------

template<VectorAxis INDEX>
inline Vector4& operator *= (Vector4& v, VecFloatRef<INDEX> a)
{
    const float s = a.mV.v[INDEX];
    for (int k = 0; k < 4; ++k) v.mV.v[k] *= s;
    return v;
}

template<VectorAxis INDEX>
inline Vector4& operator /= (Vector4& v, VecFloatRef<INDEX> a)
{
    const float recip = 1.0f / a.mV.v[INDEX];
    for (int k = 0; k < 4; ++k) v.mV.v[k] *= recip;
    return v;
}

template<VectorAxis INDEX>
inline Vector4& operator += (Vector4& v, VecFloatRef<INDEX> a)
{
    const float s = a.mV.v[INDEX];
    for (int k = 0; k < 4; ++k) v.mV.v[k] += s;
    return v;
}

template<VectorAxis INDEX>
inline Vector4& operator -= (Vector4& v, VecFloatRef<INDEX> a)
{
    const float s = a.mV.v[INDEX];
    for (int k = 0; k < 4; ++k) v.mV.v[k] -= s;
    return v;
}

template<VectorAxis INDEX>
inline Vector4 operator * (Vector4::InParam v, VecFloatRef<INDEX> s) { return Mult(v, s); }
template<VectorAxis INDEX>
inline Vector4 operator * (VecFloatRef<INDEX> s, Vector4::InParam v) { return Mult(v, s); }
template<VectorAxis INDEX>
inline Vector4 operator / (Vector4::InParam v, VecFloatRef<INDEX> s) { return Divide(v, s); }

template<VectorAxis INDEX>
inline Vector4 operator + (Vector4::InParam v, VecFloatRef<INDEX> s)
{
    const float a = s.mV.v[INDEX];
    Vector4 r;
    for (int k = 0; k < 4; ++k) r.mV.v[k] = v.mV.v[k] + a;
    return r;
}

template<VectorAxis INDEX>
inline Vector4 operator - (Vector4::InParam v, VecFloatRef<INDEX> s)
{
    const float a = s.mV.v[INDEX];
    Vector4 r;
    for (int k = 0; k < 4; ++k) r.mV.v[k] = v.mV.v[k] - a;
    return r;
}

template<VectorAxis INDEX>
inline Vector4 operator + (VecFloatRef<INDEX> s, Vector4::InParam v) { return v + s; }

template<VectorAxis INDEX>
inline Vector4 operator - (VecFloatRef<INDEX> s, Vector4::InParam v)
{
    const float a = s.mV.v[INDEX];
    Vector4 r;
    for (int k = 0; k < 4; ++k) r.mV.v[k] = a - v.mV.v[k];
    return r;
}

// ---- v op= VecFloatRefIndex -------------------------------------------------------

inline Vector4& operator *= (Vector4& v, VecFloatRefIndex::InParam a)
{
    const float s = a.mV.v[a.mIndex];
    for (int k = 0; k < 4; ++k) v.mV.v[k] *= s;
    return v;
}

inline Vector4& operator /= (Vector4& v, VecFloatRefIndex::InParam a)
{
    const float recip = 1.0f / a.mV.v[a.mIndex];
    for (int k = 0; k < 4; ++k) v.mV.v[k] *= recip;
    return v;
}

inline Vector4& operator += (Vector4& v, VecFloatRefIndex::InParam a)
{
    const float s = a.mV.v[a.mIndex];
    for (int k = 0; k < 4; ++k) v.mV.v[k] += s;
    return v;
}

inline Vector4& operator -= (Vector4& v, VecFloatRefIndex::InParam a)
{
    const float s = a.mV.v[a.mIndex];
    for (int k = 0; k < 4; ++k) v.mV.v[k] -= s;
    return v;
}

inline Vector4 operator * (Vector4::InParam v, VecFloatRefIndex::InParam s) { return Mult(v, s); }
inline Vector4 operator * (VecFloatRefIndex::InParam s, Vector4::InParam v) { return Mult(v, s); }
inline Vector4 operator / (Vector4::InParam v, VecFloatRefIndex::InParam s) { return Divide(v, s); }

inline Vector4 operator + (Vector4::InParam v, VecFloatRefIndex::InParam s)
{
    const float a = s.mV.v[s.mIndex];
    Vector4 r;
    for (int k = 0; k < 4; ++k) r.mV.v[k] = v.mV.v[k] + a;
    return r;
}

inline Vector4 operator - (Vector4::InParam v, VecFloatRefIndex::InParam s)
{
    const float a = s.mV.v[s.mIndex];
    Vector4 r;
    for (int k = 0; k < 4; ++k) r.mV.v[k] = v.mV.v[k] - a;
    return r;
}

inline Vector4 operator + (VecFloatRefIndex::InParam s, Vector4::InParam v) { return v + s; }

inline Vector4 operator - (VecFloatRefIndex::InParam s, Vector4::InParam v)
{
    const float a = s.mV.v[s.mIndex];
    Vector4 r;
    for (int k = 0; k < 4; ++k) r.mV.v[k] = a - v.mV.v[k];
    return r;
}

// =====================================================================================
//  Vector-Vector operators
// =====================================================================================

inline Vector4 operator - (Vector4::InParam v) { return Negate(v); }

inline Vector4& operator += (Vector4& v, Vector4::InParam b)
{
    for (int k = 0; k < 4; ++k) v.mV.v[k] += b.mV.v[k];
    return v;
}

inline Vector4& operator -= (Vector4& v, Vector4::InParam b)
{
    for (int k = 0; k < 4; ++k) v.mV.v[k] -= b.mV.v[k];
    return v;
}

inline Vector4& operator *= (Vector4& v, Vector4::InParam b)
{
    for (int k = 0; k < 4; ++k) v.mV.v[k] *= b.mV.v[k];
    return v;
}

inline Vector4& operator /= (Vector4& v, Vector4::InParam b)
{
    for (int k = 0; k < 4; ++k) v.mV.v[k] /= b.mV.v[k];
    return v;
}

inline Vector4 operator - (Vector4::InParam a, Vector4::InParam b) { return Subtract(a, b); }
inline Vector4 operator + (Vector4::InParam a, Vector4::InParam b) { return Add(a, b); }

// =====================================================================================
//  void out-param variants -- thin wrappers over the value-returning forms
// =====================================================================================

inline void Negate(Vector4::InParam v, Vector4& result) { result = Negate(v); }
inline void Mult(Vector4::InParam v, VecFloat::InParam s, Vector4& result) { result = Mult(v, s); }
template<VectorAxis INDEX>
inline void Mult(Vector4::InParam v, VecFloatRef<INDEX> s, Vector4& result) { result = Mult(v, s); }
inline void Mult(Vector4::InParam v, VecFloatRefIndex::InParam s, Vector4& result) { result = Mult(v, s); }
inline void Divide(Vector4::InParam v, VecFloat::InParam s, Vector4& result) { result = Divide(v, s); }
inline void Add(Vector4::InParam a, Vector4::InParam b, Vector4& result) { result = Add(a, b); }
inline void Subtract(Vector4::InParam a, Vector4::InParam b, Vector4& result) { result = Subtract(a, b); }
inline void Mult(Vector4::InParam a, Vector4::InParam b, Vector4& result) { result = Mult(a, b); }
inline void DotXYZ(Vector4::InParam v0, Vector4::InParam v1, VecFloat& result) { result = DotXYZ(v0, v1); }
inline void CrossXYZ(Vector4::InParam v0, Vector4::InParam v1, Vector4& result) { result = CrossXYZ(v0, v1); }
inline void Normalize(Vector4::InParam v, Vector4& result) { result = Normalize(v); }
inline void Lerp(Vector4::InParam v0, Vector4::InParam v1, VecFloat::InParam t, Vector4& result) { result = Lerp(v0, v1, t); }
inline void NLerp(Vector4::InParam v0, Vector4::InParam v1, VecFloat::InParam t, Vector4& result) { result = NLerp(v0, v1, t); }
inline void Dot(Vector4::InParam a, Vector4::InParam b, VecFloat& r) { r = Dot(a, b); }

// SLerp -- DECLARATION-ONLY + FLAGGED (SinCos-driven, not committed in this cluster).
void SLerp(Vector4::InParam v0, Vector4::InParam v1, VecFloat::InParam t, Vector4& result); // FLAGGED: SinCos

}
}
}

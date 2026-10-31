#pragma once

// VPU Vector3 operations on the single SDK class definition.
// Recovered VMX bodies retain all four stored lanes and native operation order.
// Scalar/ref overloads and declaration-only helpers that have not been recovered
// remain existing debt; this header is not an all-operation parity claim.

#include <cmath>

#include "SDKs/EATech/include/rw/math/vpu/vector3.h"

namespace rw
{
namespace math
{
namespace vpu
{

namespace detail
{
    // Native Xenon reciprocal-root operation order: ARTIST82600064..94 and
    // 822B1D7C..AC. This is private arithmetic, not an alternate value ABI.
    __forceinline VectorIntrinsic Vector3NativeReciprocalRoot(VectorIntrinsic squared, int refinements)
    {
        const VectorIntrinsic one = __vcfsx(__vspltisw(1), 0);
        const VectorIntrinsic half = __vcfsx(__vspltisw(1), 1);
        VectorIntrinsic estimate = __vrsqrtefp(squared);
        for (int i = 0; i < refinements; ++i)
        {
            const VectorIntrinsic error = __vnmsubfp(squared, __vmulfp(estimate, estimate), one);
            estimate = __vmaddfp(__vmulfp(estimate, half), error, estimate);
        }
        return estimate;
    }

    // ARTIST826C3D00..4C: retain the two fused reciprocal refinements.
    __forceinline VectorIntrinsic Vector3NativeReciprocal(VectorIntrinsic divisor)
    {
        const VectorIntrinsic one = __vcfsx(__vspltisw(1), 0);
        VectorIntrinsic estimate = __vrefp(divisor);
        for (int i = 0; i < 2; ++i)
            estimate = __vmaddfp(estimate, __vnmsubfp(estimate, divisor, one), estimate);
        return estimate;
    }
}


// =====================================================================================
//  Equality / validity / similarity -- DECLARATION-ONLY + FLAGGED
//  (undecoded _asmIsEqualV3/_asmIsZeroV3/_asmIsSimilarV3 ; IsValid needs IsValid(VecFloat))
// =====================================================================================

inline bool operator == (Vector3::InParam a, Vector3::InParam b)
{
    return a.mV.v[0] == b.mV.v[0]
        && a.mV.v[1] == b.mV.v[1]
        && a.mV.v[2] == b.mV.v[2];
}
inline bool operator != (Vector3::InParam a, Vector3::InParam b) { return !(a == b); }
bool IsZero(Vector3::InParam v, VecFloat::InParam tolerance);              // FLAGGED: _asmIsZeroV3
bool IsSimilar(Vector3::InParam a, Vector3::InParam b, VecFloat::InParam epsilon); // FLAGGED: _asmIsSimilarV3
inline bool IsValid(Vector3::InParam v)
{
    return v.mV.v[0] == v.mV.v[0]
        && v.mV.v[1] == v.mV.v[1]
        && v.mV.v[2] == v.mV.v[2];
}

// IsAnyNaN -- FAITHFUL. The console vcmpeqfp_p predicate tests whether v==v in every lane;
// a NaN lane fails self-comparison. Lowered to the same per-lane self-compare over [X,Y,Z].
inline bool IsAnyNaN(Vector3::InParam v)
{
    // equal = vcmpeqfp_p(v,v) (all-lanes-equal predicate); return !equal.
    return !( v.mV.v[0] == v.mV.v[0]
           && v.mV.v[1] == v.mV.v[1]
           && v.mV.v[2] == v.mV.v[2] );
}

// =====================================================================================
//  Add / Subtract / Mult / Divide / MultAdd -- core named arithmetic
// =====================================================================================

inline Vector3 Add(Vector3::InParam a, Vector3::InParam b)
{
    return Vector3(__vaddfp(a.mV, b.mV));
}

inline Vector3 Subtract(Vector3::InParam a, Vector3::InParam b)
{
    return Vector3(__vsubfp(a.mV, b.mV));
}

inline Vector3 Mult(Vector3::InParam a, Vector3::InParam b)
{
    return Vector3(__vmulfp(a.mV, b.mV));
}

inline Vector3 MultAdd(Vector3::InParam a, Vector3::InParam b, Vector3::InParam c)
{
    return Vector3(__vmaddfp(a.mV, b.mV, c.mV));
}

inline Vector3 Mult(Vector3::InParam v, VecFloat::InParam s)
{
    // vmaddfp(v, s, 0): s is a broadcast, so every lane multiplies by s.X.
    const float lfScale = s.mV.v[VectorAxisX];
    Vector3 r;
    r.mV.v[0] = v.mV.v[0] * lfScale;
    r.mV.v[1] = v.mV.v[1] * lfScale;
    r.mV.v[2] = v.mV.v[2] * lfScale;
    return r;
}

inline Vector3 Mult(Vector3::InParam v, float s)
{
    const VectorIntrinsic scalar = __vspltw(__lvlx(&s, 0), 0);
    return Vector3(__vmulfp(v.mV, scalar));
}

template<VectorAxis INDEX>
inline Vector3 Mult(Vector3::InParam v, VecFloatRef<INDEX> s)
{
    // splattedAxis = vspltw(s.mV, INDEX); vmaddfp(v, splattedAxis, 0).
    const float lfScale = s.mV.v[INDEX];
    Vector3 r;
    r.mV.v[0] = v.mV.v[0] * lfScale;
    r.mV.v[1] = v.mV.v[1] * lfScale;
    r.mV.v[2] = v.mV.v[2] * lfScale;
    return r;
}

inline Vector3 Mult(Vector3::InParam v, VecFloatRefIndex::InParam s)
{
    // permute the referenced lane across, then vmaddfp(v, sVI, 0).
    const float lfScale = s.mV.v[s.mIndex];
    Vector3 r;
    r.mV.v[0] = v.mV.v[0] * lfScale;
    r.mV.v[1] = v.mV.v[1] * lfScale;
    r.mV.v[2] = v.mV.v[2] * lfScale;
    return r;
}

inline Vector3 Divide(Vector3::InParam v, VecFloat::InParam s)
{
    return Vector3(__vmulfp(detail::Vector3NativeReciprocal(s.mV), v.mV));
}

inline Vector3 Divide(Vector3::InParam v, float s)
{
    // The named scalar overload computes a scalar reciprocal before broadcasting.
    const float lfRecip = 1.0f / s;
    return Vector3(__vmulfp(__vspltw(__lvlx(&lfRecip, 0), 0), v.mV));
}

template<VectorAxis INDEX>
inline Vector3 Divide(Vector3::InParam v, VecFloatRef<INDEX> s)
{
    // splattedAxis = vspltw(s.mV, INDEX); reciprocal = 1/splattedAxis; vmaddfp(v, recip, 0).
    const float lfRecip = 1.0f / s.mV.v[INDEX];
    Vector3 r;
    r.mV.v[0] = v.mV.v[0] * lfRecip;
    r.mV.v[1] = v.mV.v[1] * lfRecip;
    r.mV.v[2] = v.mV.v[2] * lfRecip;
    return r;
}

inline Vector3 Divide(Vector3::InParam v, VecFloatRefIndex::InParam s)
{
    const float lfRecip = 1.0f / s.mV.v[s.mIndex];
    Vector3 r;
    r.mV.v[0] = v.mV.v[0] * lfRecip;
    r.mV.v[1] = v.mV.v[1] * lfRecip;
    r.mV.v[2] = v.mV.v[2] * lfRecip;
    return r;
}

// =====================================================================================
//  Dot / Cross
// =====================================================================================

inline VecFloat Dot(Vector3::InParam a, Vector3::InParam b)
{
    return VecFloat(__vmsum3fp(a.mV, b.mV));
}

inline Vector3 Cross(Vector3::InParam a, Vector3::InParam b)
{
    // ARTIST8254081C..54: rounded first product, fused subtraction, then YZX.
    const VectorIntrinsic ayzx = __vpermwi(a.mV, 0x63);
    const VectorIntrinsic byzx = __vpermwi(b.mV, 0x63);
    const VectorIntrinsic product = __vmulfp(a.mV, byzx);
    return Vector3(__vpermwi(__vnmsubfp(ayzx, b.mV, product), 0x63));
}

// =====================================================================================
//  Magnitude family
// =====================================================================================

inline VecFloat MagnitudeSquared(Vector3::InParam v)
{
    return Dot(v, v);
}

inline VecFloat MagnitudeRecip(Vector3::InParam v)
{
    // dotproduct = |v|^2 ; recipSqrt = 1/sqrt(dotproduct) (vrsqrtefp + Newton).
    const float lfDot = v.mV.v[0] * v.mV.v[0]
                      + v.mV.v[1] * v.mV.v[1]
                      + v.mV.v[2] * v.mV.v[2];
    return VecFloat(1.0f / sqrtf(lfDot));
}

inline VecFloat Magnitude(Vector3::InParam v)
{
    const VectorIntrinsic squared = __vmsum3fp(v.mV, v.mV);
    const VectorIntrinsic reciprocal = detail::Vector3NativeReciprocalRoot(squared, 2);
    const VectorIntrinsic zero = __vspltisw(0);
    return VecFloat(__vsel(__vmulfp(squared, reciprocal), zero, __vcmpeqfp(squared, zero)));
}

// =====================================================================================
//  Normalize family
// =====================================================================================

inline Vector3 NormalizeEstimate(Vector3::InParam v)
{
    const VectorIntrinsic squared = __vmsum3fp(v.mV, v.mV);
    const VectorIntrinsic reciprocal = detail::Vector3NativeReciprocalRoot(squared, 0);
    return Vector3(__vmulfp(v.mV, reciprocal));
}

inline Vector3 NormalizeFast(Vector3::InParam v)
{
    const VectorIntrinsic squared = __vmsum3fp(v.mV, v.mV);
    const VectorIntrinsic reciprocal = detail::Vector3NativeReciprocalRoot(squared, 1);
    return Vector3(__vmulfp(v.mV, reciprocal));
}

inline Vector3 Normalize(Vector3::InParam v)
{
    const VectorIntrinsic squared = __vmsum3fp(v.mV, v.mV);
    const VectorIntrinsic reciprocal = detail::Vector3NativeReciprocalRoot(squared, 2);
    return Vector3(__vmulfp(v.mV, reciprocal));
}

inline Vector3 Normalize(Vector3::InParam v, VecFloat& magnitude)
{
    const VectorIntrinsic squared = __vmsum3fp(v.mV, v.mV);
    const VectorIntrinsic reciprocal = detail::Vector3NativeReciprocalRoot(squared, 2);
    const VectorIntrinsic zero = __vspltisw(0);
    magnitude = VecFloat(__vsel(__vmulfp(squared, reciprocal), zero, __vcmpeqfp(squared, zero)));
    return Vector3(__vmulfp(v.mV, reciprocal));
}

inline VecFloat NormalizeReturnMagnitude(Vector3::InParam v, Vector3& result)
{
    VecFloat magnitude;
    result = Normalize(v, magnitude);
    return magnitude;
}

// CosAngleBetweenNormalizedVectors / AngleBetweenVectors -- DECLARATION-ONLY + FLAGGED.
// CosAngleBetweenNormalizedVectors is GT clamp(Dot(v0,v1),-1,+1) (vmaxfp(-1)+vminfp(1), NO ACos) --
// fully groundable but needs a VecFloat Min/Max (vminfp/vmaxfp) not yet committed in vec_float; body
// it when that lands. AngleBetweenVectors reduces to ACos(that cos) (+ a TWO_PI wind-axis branch);
// ACos / TWO_PI are not committed, so the trig step cannot be grounded here.
VecFloat CosAngleBetweenNormalizedVectors(Vector3::InParam v0, Vector3::InParam v1);   // FLAGGED: needs VecFloat Min/Max
VecFloat AngleBetweenVectors(Vector3::InParam v0, Vector3::InParam v1);                // FLAGGED: ACos
VecFloat AngleBetweenVectors(Vector3::InParam v0, Vector3::InParam v1, Vector3::InParam windAxis); // FLAGGED: ACos/TWO_PI

// =====================================================================================
//  Lerp / NLerp
// =====================================================================================

inline Vector3 Lerp(Vector3::InParam v0, Vector3::InParam v1, VecFloat::InParam t)
{
    // temp = vsubfp(v1,v0); ret = vmaddfp(temp, t, v0) == v0 + (v1-v0)*t.
    const float lfT = t.mV.v[VectorAxisX];
    Vector3 r;
    r.mV.v[0] = (v1.mV.v[0] - v0.mV.v[0]) * lfT + v0.mV.v[0];
    r.mV.v[1] = (v1.mV.v[1] - v0.mV.v[1]) * lfT + v0.mV.v[1];
    r.mV.v[2] = (v1.mV.v[2] - v0.mV.v[2]) * lfT + v0.mV.v[2];
    return r;
}

inline Vector3 NLerpEstimate(Vector3::InParam v0, Vector3::InParam v1, VecFloat::InParam t)
{
    // lerp = Lerp(v0,v1,t); then normalize-estimate (raw vrsqrtefp).
    return NormalizeEstimate(Lerp(v0, v1, t));
}

inline Vector3 NLerpFast(Vector3::InParam v0, Vector3::InParam v1, VecFloat::InParam t)
{
    // lerp = Lerp(v0,v1,t); then normalize-fast (one Newton step).
    return NormalizeFast(Lerp(v0, v1, t));
}

inline Vector3 NLerp(Vector3::InParam v0, Vector3::InParam v1, VecFloat::InParam t)
{
    // lerp = Lerp(v0,v1,t); then normalize (two Newton steps).
    return Normalize(Lerp(v0, v1, t));
}

// =====================================================================================
//  Negate / Min / Max / Clamp / Abs
// =====================================================================================

inline Vector3 Negate(Vector3::InParam v)
{
    const VectorIntrinsic allOnes = __vspltisw(-1);
    const VectorIntrinsic sign = __vslw(allOnes, allOnes);
    return Vector3(__vxor(v.mV, sign));
}

inline Vector3 Min(Vector3::InParam v0, Vector3::InParam v1)
{
    return Vector3(__vminfp(v0.mV, v1.mV));
}

inline Vector3 Max(Vector3::InParam v0, Vector3::InParam v1)
{
    return Vector3(__vmaxfp(v0.mV, v1.mV));
}

inline Vector3 Clamp(Vector3::InParam v0, Vector3::InParam v1, Vector3::InParam v2)
{
    // r = vmaxfp(v0,v1); r = vminfp(r,v2): clamp v0 into [v1,v2] per lane.
    return Min(Max(v0, v1), v2);
}

inline Vector3 Abs(Vector3::InParam v)
{
    const VectorIntrinsic allOnes = __vspltisw(-1);
    return Vector3(__vandc(v.mV, __vslw(allOnes, allOnes)));
}

// =====================================================================================
//  Comparison masks / Select -- OMITTED + FLAGGED
//
//  CompLessThan / CompGreaterThan / CompLessEqual / CompGreaterEqual / CompEqual /
//  CompNotEqual return a Mask3, and the two Select overloads consume a Mask3 / MaskScalar.
//  Those mask types are NOT committed in this cluster, so even a forward declaration here
//  would not parse. They are intentionally omitted rather than fabricated; a reviewer that
//  homes the Mask3 / MaskScalar types can add the (faithful vcmpgefp/vcmpgtfp/vcmpeqfp +
//  vnor mask, vsel select) bodies then. Console forms recorded in the Feb-2007 source.
// =====================================================================================

// =====================================================================================
//  IsZero / IsSimilar already declared above (FLAGGED).
//  Compound assignment + operators (scalar / VecFloat / VecFloatRef / VecFloatRefIndex)
// =====================================================================================

// ---- v op= VecFloat ---------------------------------------------------------------

inline Vector3& operator *= (Vector3& v, VecFloat::InParam a)
{
    const float s = a.mV.v[VectorAxisX];
    v.mV.v[0] *= s; v.mV.v[1] *= s; v.mV.v[2] *= s;
    return v;
}

inline Vector3& operator /= (Vector3& v, VecFloat::InParam a)
{
    const float recip = 1.0f / a.mV.v[VectorAxisX];
    v.mV.v[0] *= recip; v.mV.v[1] *= recip; v.mV.v[2] *= recip;
    return v;
}

inline Vector3& operator += (Vector3& v, VecFloat::InParam a)
{
    // vaddfp(v, a): a is a broadcast so the same scalar is added to every lane.
    const float s = a.mV.v[VectorAxisX];
    v.mV.v[0] += s; v.mV.v[1] += s; v.mV.v[2] += s;
    return v;
}

inline Vector3& operator -= (Vector3& v, VecFloat::InParam a)
{
    const float s = a.mV.v[VectorAxisX];
    v.mV.v[0] -= s; v.mV.v[1] -= s; v.mV.v[2] -= s;
    return v;
}

inline Vector3 operator * (Vector3::InParam v, VecFloat::InParam s) { return Mult(v, s); }
inline Vector3 operator * (VecFloat::InParam s, Vector3::InParam v) { return Mult(v, s); }
inline Vector3 operator / (Vector3::InParam v, VecFloat::InParam s) { return Divide(v, s); }

inline Vector3 operator + (Vector3::InParam v, VecFloat::InParam s)
{
    const float a = s.mV.v[VectorAxisX];
    Vector3 r;
    r.mV.v[0] = v.mV.v[0] + a;
    r.mV.v[1] = v.mV.v[1] + a;
    r.mV.v[2] = v.mV.v[2] + a;
    return r;
}

inline Vector3 operator - (Vector3::InParam v, VecFloat::InParam s)
{
    const float a = s.mV.v[VectorAxisX];
    Vector3 r;
    r.mV.v[0] = v.mV.v[0] - a;
    r.mV.v[1] = v.mV.v[1] - a;
    r.mV.v[2] = v.mV.v[2] - a;
    return r;
}

inline Vector3 operator + (VecFloat::InParam s, Vector3::InParam v) { return v + s; }

inline Vector3 operator - (VecFloat::InParam s, Vector3::InParam v)
{
    // vsubfp(s, v): scalar minus each lane.
    const float a = s.mV.v[VectorAxisX];
    Vector3 r;
    r.mV.v[0] = a - v.mV.v[0];
    r.mV.v[1] = a - v.mV.v[1];
    r.mV.v[2] = a - v.mV.v[2];
    return r;
}

// ---- v op= float ------------------------------------------------------------------

inline Vector3& operator *= (Vector3& v, float s)
{
    v.mV.v[0] *= s; v.mV.v[1] *= s; v.mV.v[2] *= s;
    return v;
}

inline Vector3& operator /= (Vector3& v, float s)
{
    const float recip = 1.0f / s;
    v.mV.v[0] *= recip; v.mV.v[1] *= recip; v.mV.v[2] *= recip;
    return v;
}

inline Vector3 operator * (Vector3::InParam v, float s) { return Mult(v, s); }
inline Vector3 operator * (float s, Vector3::InParam v) { return Mult(v, s); }
inline Vector3 operator / (Vector3::InParam v, float s) { return Divide(v, VecFloat(s)); }

inline Vector3 operator + (Vector3::InParam v, float s)
{
    Vector3 r;
    r.mV.v[0] = v.mV.v[0] + s;
    r.mV.v[1] = v.mV.v[1] + s;
    r.mV.v[2] = v.mV.v[2] + s;
    return r;
}

inline Vector3 operator - (Vector3::InParam v, float s)
{
    Vector3 r;
    r.mV.v[0] = v.mV.v[0] - s;
    r.mV.v[1] = v.mV.v[1] - s;
    r.mV.v[2] = v.mV.v[2] - s;
    return r;
}

inline Vector3 operator + (float s, Vector3::InParam v) { return v + s; }

inline Vector3 operator - (float s, Vector3::InParam v)
{
    Vector3 r;
    r.mV.v[0] = s - v.mV.v[0];
    r.mV.v[1] = s - v.mV.v[1];
    r.mV.v[2] = s - v.mV.v[2];
    return r;
}

// ---- v op= VecFloatRef<INDEX> -----------------------------------------------------

template<VectorAxis INDEX>
inline Vector3& operator *= (Vector3& v, VecFloatRef<INDEX> a)
{
    const float s = a.mV.v[INDEX];
    v.mV.v[0] *= s; v.mV.v[1] *= s; v.mV.v[2] *= s;
    return v;
}

template<VectorAxis INDEX>
inline Vector3& operator /= (Vector3& v, VecFloatRef<INDEX> a)
{
    const float recip = 1.0f / a.mV.v[INDEX];
    v.mV.v[0] *= recip; v.mV.v[1] *= recip; v.mV.v[2] *= recip;
    return v;
}

template<VectorAxis INDEX>
inline Vector3& operator += (Vector3& v, VecFloatRef<INDEX> a)
{
    const float s = a.mV.v[INDEX];
    v.mV.v[0] += s; v.mV.v[1] += s; v.mV.v[2] += s;
    return v;
}

template<VectorAxis INDEX>
inline Vector3& operator -= (Vector3& v, VecFloatRef<INDEX> a)
{
    const float s = a.mV.v[INDEX];
    v.mV.v[0] -= s; v.mV.v[1] -= s; v.mV.v[2] -= s;
    return v;
}

template<VectorAxis INDEX>
inline Vector3 operator * (Vector3::InParam v, VecFloatRef<INDEX> s) { return Mult(v, s); }
template<VectorAxis INDEX>
inline Vector3 operator * (VecFloatRef<INDEX> s, Vector3::InParam v) { return Mult(v, s); }
template<VectorAxis INDEX>
inline Vector3 operator / (Vector3::InParam v, VecFloatRef<INDEX> s) { return Divide(v, s); }

template<VectorAxis INDEX>
inline Vector3 operator + (Vector3::InParam v, VecFloatRef<INDEX> s)
{
    const float a = s.mV.v[INDEX];
    Vector3 r;
    r.mV.v[0] = v.mV.v[0] + a;
    r.mV.v[1] = v.mV.v[1] + a;
    r.mV.v[2] = v.mV.v[2] + a;
    return r;
}

template<VectorAxis INDEX>
inline Vector3 operator - (Vector3::InParam v, VecFloatRef<INDEX> s)
{
    const float a = s.mV.v[INDEX];
    Vector3 r;
    r.mV.v[0] = v.mV.v[0] - a;
    r.mV.v[1] = v.mV.v[1] - a;
    r.mV.v[2] = v.mV.v[2] - a;
    return r;
}

template<VectorAxis INDEX>
inline Vector3 operator + (VecFloatRef<INDEX> s, Vector3::InParam v) { return v + s; }

template<VectorAxis INDEX>
inline Vector3 operator - (VecFloatRef<INDEX> s, Vector3::InParam v)
{
    const float a = s.mV.v[INDEX];
    Vector3 r;
    r.mV.v[0] = a - v.mV.v[0];
    r.mV.v[1] = a - v.mV.v[1];
    r.mV.v[2] = a - v.mV.v[2];
    return r;
}

// ---- v op= VecFloatRefIndex -------------------------------------------------------

inline Vector3& operator *= (Vector3& v, VecFloatRefIndex::InParam a)
{
    const float s = a.mV.v[a.mIndex];
    v.mV.v[0] *= s; v.mV.v[1] *= s; v.mV.v[2] *= s;
    return v;
}

inline Vector3& operator /= (Vector3& v, VecFloatRefIndex::InParam a)
{
    const float recip = 1.0f / a.mV.v[a.mIndex];
    v.mV.v[0] *= recip; v.mV.v[1] *= recip; v.mV.v[2] *= recip;
    return v;
}

inline Vector3& operator += (Vector3& v, VecFloatRefIndex::InParam a)
{
    const float s = a.mV.v[a.mIndex];
    v.mV.v[0] += s; v.mV.v[1] += s; v.mV.v[2] += s;
    return v;
}

inline Vector3& operator -= (Vector3& v, VecFloatRefIndex::InParam a)
{
    const float s = a.mV.v[a.mIndex];
    v.mV.v[0] -= s; v.mV.v[1] -= s; v.mV.v[2] -= s;
    return v;
}

inline Vector3 operator * (Vector3::InParam v, VecFloatRefIndex::InParam s) { return Mult(v, s); }
inline Vector3 operator * (VecFloatRefIndex::InParam s, Vector3::InParam v) { return Mult(v, s); }
inline Vector3 operator / (Vector3::InParam v, VecFloatRefIndex::InParam s) { return Divide(v, s); }

inline Vector3 operator + (Vector3::InParam v, VecFloatRefIndex::InParam s)
{
    const float a = s.mV.v[s.mIndex];
    Vector3 r;
    r.mV.v[0] = v.mV.v[0] + a;
    r.mV.v[1] = v.mV.v[1] + a;
    r.mV.v[2] = v.mV.v[2] + a;
    return r;
}

inline Vector3 operator - (Vector3::InParam v, VecFloatRefIndex::InParam s)
{
    const float a = s.mV.v[s.mIndex];
    Vector3 r;
    r.mV.v[0] = v.mV.v[0] - a;
    r.mV.v[1] = v.mV.v[1] - a;
    r.mV.v[2] = v.mV.v[2] - a;
    return r;
}

inline Vector3 operator + (VecFloatRefIndex::InParam s, Vector3::InParam v) { return v + s; }

inline Vector3 operator - (VecFloatRefIndex::InParam s, Vector3::InParam v)
{
    const float a = s.mV.v[s.mIndex];
    Vector3 r;
    r.mV.v[0] = a - v.mV.v[0];
    r.mV.v[1] = a - v.mV.v[1];
    r.mV.v[2] = a - v.mV.v[2];
    return r;
}

// =====================================================================================
//  Vector-Vector operators
// =====================================================================================

inline Vector3 operator - (Vector3::InParam v) { return Negate(v); }

inline Vector3& operator += (Vector3& v, Vector3::InParam b)
{
    v.mV.v[0] += b.mV.v[0];
    v.mV.v[1] += b.mV.v[1];
    v.mV.v[2] += b.mV.v[2];
    return v;
}

inline Vector3& operator -= (Vector3& v, Vector3::InParam b)
{
    v.mV.v[0] -= b.mV.v[0];
    v.mV.v[1] -= b.mV.v[1];
    v.mV.v[2] -= b.mV.v[2];
    return v;
}

inline Vector3& operator *= (Vector3& v, Vector3::InParam b)
{
    v.mV.v[0] *= b.mV.v[0];
    v.mV.v[1] *= b.mV.v[1];
    v.mV.v[2] *= b.mV.v[2];
    return v;
}

inline Vector3& operator /= (Vector3& v, Vector3::InParam b)
{
    v.mV.v[0] /= b.mV.v[0];
    v.mV.v[1] /= b.mV.v[1];
    v.mV.v[2] /= b.mV.v[2];
    return v;
}

inline Vector3 operator * (Vector3::InParam a, Vector3::InParam b) { return Mult(a, b); }
inline Vector3 operator / (Vector3::InParam a, Vector3::InParam b)
{
    Vector3 r;
    r.mV.v[0] = a.mV.v[0] / b.mV.v[0];
    r.mV.v[1] = a.mV.v[1] / b.mV.v[1];
    r.mV.v[2] = a.mV.v[2] / b.mV.v[2];
    return r;
}
inline Vector3 operator - (Vector3::InParam a, Vector3::InParam b) { return Subtract(a, b); }
inline Vector3 operator + (Vector3::InParam a, Vector3::InParam b) { return Add(a, b); }

// =====================================================================================
//  void out-param variants -- thin wrappers over the value-returning forms
// =====================================================================================

inline void Negate(Vector3::InParam vec, Vector3& result)             { result = Negate(vec); }
inline void Mult(Vector3::InParam vec, VecFloat::InParam scalar, Vector3& result) { result = Mult(vec, scalar); }
template<VectorAxis INDEX>
inline void Mult(Vector3::InParam vec, VecFloatRef<INDEX> scalar, Vector3& result) { result = Mult(vec, scalar); }
inline void Mult(Vector3::InParam vec, VecFloatRefIndex::InParam scalar, Vector3& result) { result = Mult(vec, scalar); }
inline void Divide(Vector3::InParam v, VecFloat::InParam s, Vector3& result) { result = Divide(v, s); }
template<VectorAxis INDEX>
inline void Divide(Vector3::InParam v, VecFloatRef<INDEX> s, Vector3& result) { result = Divide(v, s); }
inline void Divide(Vector3::InParam v, VecFloatRefIndex::InParam s, Vector3& result) { result = Divide(v, s); }
inline void Add(Vector3::InParam a, Vector3::InParam b, Vector3& result) { result = Add(a, b); }
inline void Subtract(Vector3::InParam a, Vector3::InParam b, Vector3& result) { result = Subtract(a, b); }
inline void Cross(Vector3::InParam a, Vector3::InParam b, Vector3& result) { result = Cross(a, b); }
inline void Mult(Vector3::InParam a, Vector3::InParam b, Vector3& result) { result = Mult(a, b); }
inline void Normalize(Vector3::InParam v, Vector3& result) { result = Normalize(v); }
inline void Lerp(Vector3::InParam v0, Vector3::InParam v1, VecFloat::InParam t, Vector3& result) { result = Lerp(v0, v1, t); }
template<VectorAxis INDEX>
inline void Lerp(Vector3::InParam v0, Vector3::InParam v1, VecFloatRef<INDEX> t, Vector3& result) { result = Lerp(v0, v1, VecFloat(t)); }
inline void Lerp(Vector3::InParam v0, Vector3::InParam v1, VecFloatRefIndex::InParam t, Vector3& result) { result = Lerp(v0, v1, VecFloat(t)); }
inline void NLerp(Vector3::InParam v0, Vector3::InParam v1, VecFloat::InParam t, Vector3& result) { result = NLerp(v0, v1, t); }
template<VectorAxis INDEX>
inline void NLerp(Vector3::InParam v0, Vector3::InParam v1, VecFloatRef<INDEX> t, Vector3& result) { result = NLerp(v0, v1, VecFloat(t)); }
inline void NLerp(Vector3::InParam v0, Vector3::InParam v1, VecFloatRefIndex::InParam t, Vector3& result) { result = NLerp(v0, v1, VecFloat(t)); }
inline void Dot(Vector3::InParam a, Vector3::InParam b, VecFloat& r) { r = Dot(a, b); }

// SLerp (all overloads) -- DECLARATION-ONLY + FLAGGED: the value-returning SLerp(v0,v1,t)
// these forward to is a SinCos-driven great-circle interpolation; SinCos/ACos are not
// committed in this cluster.
void SLerp(Vector3::InParam v0, Vector3::InParam v1, VecFloat::InParam t, Vector3& result);          // FLAGGED: SinCos
template<VectorAxis INDEX>
void SLerp(Vector3::InParam v0, Vector3::InParam v1, VecFloatRef<INDEX> t, Vector3& result);          // FLAGGED: SinCos
void SLerp(Vector3::InParam v0, Vector3::InParam v1, VecFloatRefIndex::InParam t, Vector3& result);   // FLAGGED: SinCos

}
}
}

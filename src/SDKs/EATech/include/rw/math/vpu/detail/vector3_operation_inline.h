#pragma once

// Portable PC reconstruction of EARenderWare rwmath (1.02.00)
// rw/math/vpu/detail/vector3_operation_inline.h -- the free-function vector algebra
// (operator +,-,*,/ ; Add/Subtract/Mult/Divide/MultAdd ; Dot/Cross ; Magnitude family ;
// Normalize family ; Lerp/NLerp ; Min/Max/Clamp/Abs/Negate ; component scaling) declared
// via vector3.h. Camera behaviours, collision and CgsGraphics::Camera call these.
//
// On the X360 every body here is one or more AltiVec/VMX intrinsics on the `vector float`
// register (`VectorIntrinsic mV`). Per the project's vendor-SIMD rule -- and exactly as the
// committed VecFloat lane wrappers (vec_float_type_inline.h), the Vector3/Vector4 type
// inlines, and the wave-40 matrix44_operation_platform_inline.h do -- each is lowered to the
// equivalent portable per-lane float math (members by name, no raw offset hacks). The console
// intrinsic each body wraps is named in a comment. The Feb-2007 partial source is the
// definitive ground truth for these always-inline bodies.
//
// A Vector3 occupies lanes [X,Y,Z]; lane W is unused. The console ops compute on all four
// lanes and the reductions (Dot/Magnitude) explicitly sum only X+Y+Z, so the lowered bodies
// touch lanes 0..2 and leave lane 3 alone (matching the register-level no-op on W).
//
// FAITHFULLY RECONSTRUCTED: all of the arithmetic / Dot / Cross / Magnitude / Normalize /
//   Lerp/NLerp / Min/Max/Clamp/Abs/Negate surface, every scalar / VecFloat / VecFloatRef<>
//   / VecFloatRefIndex overload, the void-out-param variants and the compound-assignment
//   operators. Each maps onto a committed vec_float SIMD op (vaddfp/vsubfp/vmaddfp/
//   vnmsubfp/vrefp/vrsqrtefp/vminfp/vmaxfp/vsel) lowered to its exact scalar form.
//
//   * vaddfp/vsubfp(a,b)      per-lane a +/- b
//   * vmaddfp(a,b,c)          per-lane a*b + c   (fused multiply-add; addend 0 => multiply)
//   * vnmsubfp(a,b,c)         per-lane c - a*b
//   * vrefp + Newton refine   per-lane reciprocal 1/b
//   * vrsqrtefp + Newton      per-lane reciprocal-sqrt 1/sqrt(b)
//   * vminfp/vmaxfp(a,b)      per-lane min/max
//   * vsel(f,t,mask)          per-lane select (used here only for the divide-by-zero guard
//                             on Magnitude / Normalize: a zero-length input returns zero)
//   The console builds 1/x and 1/sqrt(x) from an estimate plus Newton-Raphson refinement;
//   that is a hardware-accurate way to evaluate the exact reciprocal/rsqrt, so the portable
//   form is the closed-form scalar it converges to (same convention as the committed
//   VecFloatRef::operator/= and matrix44 reciprocal lowering).
//
// LEFT DECLARATION-ONLY + FLAGGED (genuinely multi-stage hand-VMX or dependent on types/
// helpers NOT committed in this cluster -- a scalar paraphrase would be a fabrication):
//   * operator == / != / IsZero / IsSimilar      -> _asmIsEqualV3 / _asmIsZeroV3 /
//                                                    _asmIsSimilarV3 : undecoded asm helpers.
//   * IsValid                                     -> needs IsValid(VecFloat) (not committed).
//   * Comp{LessThan,GreaterThan,LessEqual,GreaterEqual,Equal,NotEqual} / Select
//                                                 -> return/consume Mask3 / MaskScalar, types
//                                                    not committed in this cluster.
//   * CosAngleBetweenNormalizedVectors / AngleBetweenVectors / SLerp
//                                                 -> depend on ACos / SinCos / TWO_PI which
//                                                    are not committed in this cluster.

#include <cmath>

#include "SDKs/EATech/include/rw/math/vpu/vector3.h"

namespace rw
{
namespace math
{
namespace vpu
{

// =====================================================================================
//  Equality / validity / similarity -- DECLARATION-ONLY + FLAGGED
//  (undecoded _asmIsEqualV3/_asmIsZeroV3/_asmIsSimilarV3 ; IsValid needs IsValid(VecFloat))
// =====================================================================================

bool operator == (Vector3::InParam a, Vector3::InParam b);                 // FLAGGED: _asmIsEqualV3
bool operator != (Vector3::InParam a, Vector3::InParam b);                 // FLAGGED: _asmIsEqualV3
bool IsZero(Vector3::InParam v, VecFloat::InParam tolerance);              // FLAGGED: _asmIsZeroV3
bool IsSimilar(Vector3::InParam a, Vector3::InParam b, VecFloat::InParam epsilon); // FLAGGED: _asmIsSimilarV3
bool IsValid(Vector3::InParam v);                                         // FLAGGED: needs IsValid(VecFloat)

// IsAnyNaN -- FAITHFUL. The console vcmpeqfp_p predicate tests whether v==v in every lane;
// a NaN lane fails self-comparison. Lowered to the same per-lane self-compare over [X,Y,Z].
inline bool IsAnyNaN(Vector3::InParam v)
{
    // equal = vcmpeqfp_p(v,v) (all-lanes-equal predicate); return !equal.
    return !( v.mV.mafLane[0] == v.mV.mafLane[0]
           && v.mV.mafLane[1] == v.mV.mafLane[1]
           && v.mV.mafLane[2] == v.mV.mafLane[2] );
}

// =====================================================================================
//  Add / Subtract / Mult / Divide / MultAdd -- core named arithmetic
// =====================================================================================

inline Vector3 Add(Vector3::InParam a, Vector3::InParam b)
{
    // vaddfp(a,b)
    Vector3 r;
    r.mV.mafLane[0] = a.mV.mafLane[0] + b.mV.mafLane[0];
    r.mV.mafLane[1] = a.mV.mafLane[1] + b.mV.mafLane[1];
    r.mV.mafLane[2] = a.mV.mafLane[2] + b.mV.mafLane[2];
    return r;
}

inline Vector3 Subtract(Vector3::InParam a, Vector3::InParam b)
{
    // vsubfp(a,b)
    Vector3 r;
    r.mV.mafLane[0] = a.mV.mafLane[0] - b.mV.mafLane[0];
    r.mV.mafLane[1] = a.mV.mafLane[1] - b.mV.mafLane[1];
    r.mV.mafLane[2] = a.mV.mafLane[2] - b.mV.mafLane[2];
    return r;
}

inline Vector3 Mult(Vector3::InParam a, Vector3::InParam b)
{
    // vmaddfp(a,b,0): per-lane multiply.
    Vector3 r;
    r.mV.mafLane[0] = a.mV.mafLane[0] * b.mV.mafLane[0];
    r.mV.mafLane[1] = a.mV.mafLane[1] * b.mV.mafLane[1];
    r.mV.mafLane[2] = a.mV.mafLane[2] * b.mV.mafLane[2];
    return r;
}

inline Vector3 MultAdd(Vector3::InParam a, Vector3::InParam b, Vector3::InParam c)
{
    // vmaddfp(a,b,c): per-lane a*b + c.
    Vector3 r;
    r.mV.mafLane[0] = a.mV.mafLane[0] * b.mV.mafLane[0] + c.mV.mafLane[0];
    r.mV.mafLane[1] = a.mV.mafLane[1] * b.mV.mafLane[1] + c.mV.mafLane[1];
    r.mV.mafLane[2] = a.mV.mafLane[2] * b.mV.mafLane[2] + c.mV.mafLane[2];
    return r;
}

inline Vector3 Mult(Vector3::InParam v, VecFloat::InParam s)
{
    // vmaddfp(v, s, 0): s is a broadcast, so every lane multiplies by s.X.
    const float lfScale = s.mV.mafLane[VectorAxisX];
    Vector3 r;
    r.mV.mafLane[0] = v.mV.mafLane[0] * lfScale;
    r.mV.mafLane[1] = v.mV.mafLane[1] * lfScale;
    r.mV.mafLane[2] = v.mV.mafLane[2] * lfScale;
    return r;
}

inline Vector3 Mult(Vector3::InParam v, float s)
{
    // broadcast(s) then vmaddfp(v, broadcast, 0).
    Vector3 r;
    r.mV.mafLane[0] = v.mV.mafLane[0] * s;
    r.mV.mafLane[1] = v.mV.mafLane[1] * s;
    r.mV.mafLane[2] = v.mV.mafLane[2] * s;
    return r;
}

template<VectorAxis INDEX>
inline Vector3 Mult(Vector3::InParam v, VecFloatRef<INDEX> s)
{
    // splattedAxis = vspltw(s.mV, INDEX); vmaddfp(v, splattedAxis, 0).
    const float lfScale = s.mV.mafLane[INDEX];
    Vector3 r;
    r.mV.mafLane[0] = v.mV.mafLane[0] * lfScale;
    r.mV.mafLane[1] = v.mV.mafLane[1] * lfScale;
    r.mV.mafLane[2] = v.mV.mafLane[2] * lfScale;
    return r;
}

inline Vector3 Mult(Vector3::InParam v, VecFloatRefIndex::InParam s)
{
    // permute the referenced lane across, then vmaddfp(v, sVI, 0).
    const float lfScale = s.mV.mafLane[s.mIndex];
    Vector3 r;
    r.mV.mafLane[0] = v.mV.mafLane[0] * lfScale;
    r.mV.mafLane[1] = v.mV.mafLane[1] * lfScale;
    r.mV.mafLane[2] = v.mV.mafLane[2] * lfScale;
    return r;
}

inline Vector3 Divide(Vector3::InParam v, VecFloat::InParam s)
{
    // reciprocal = 1/s (vrefp + Newton); vmaddfp(v, reciprocal, 0).
    const float lfRecip = 1.0f / s.mV.mafLane[VectorAxisX];
    Vector3 r;
    r.mV.mafLane[0] = v.mV.mafLane[0] * lfRecip;
    r.mV.mafLane[1] = v.mV.mafLane[1] * lfRecip;
    r.mV.mafLane[2] = v.mV.mafLane[2] * lfRecip;
    return r;
}

inline Vector3 Divide(Vector3::InParam v, float s)
{
    // floatRecip = 1/s; broadcast; vmaddfp(v, reciprocal, 0).
    const float lfRecip = 1.0f / s;
    Vector3 r;
    r.mV.mafLane[0] = v.mV.mafLane[0] * lfRecip;
    r.mV.mafLane[1] = v.mV.mafLane[1] * lfRecip;
    r.mV.mafLane[2] = v.mV.mafLane[2] * lfRecip;
    return r;
}

template<VectorAxis INDEX>
inline Vector3 Divide(Vector3::InParam v, VecFloatRef<INDEX> s)
{
    // splattedAxis = vspltw(s.mV, INDEX); reciprocal = 1/splattedAxis; vmaddfp(v, recip, 0).
    const float lfRecip = 1.0f / s.mV.mafLane[INDEX];
    Vector3 r;
    r.mV.mafLane[0] = v.mV.mafLane[0] * lfRecip;
    r.mV.mafLane[1] = v.mV.mafLane[1] * lfRecip;
    r.mV.mafLane[2] = v.mV.mafLane[2] * lfRecip;
    return r;
}

inline Vector3 Divide(Vector3::InParam v, VecFloatRefIndex::InParam s)
{
    const float lfRecip = 1.0f / s.mV.mafLane[s.mIndex];
    Vector3 r;
    r.mV.mafLane[0] = v.mV.mafLane[0] * lfRecip;
    r.mV.mafLane[1] = v.mV.mafLane[1] * lfRecip;
    r.mV.mafLane[2] = v.mV.mafLane[2] * lfRecip;
    return r;
}

// =====================================================================================
//  Dot / Cross
// =====================================================================================

inline VecFloat Dot(Vector3::InParam a, Vector3::InParam b)
{
    // xx_yy_zz = vmaddfp(a,b,0); then vsldoi rotates and vaddfp sums X+Y+Z; vspltw lane0.
    const float lfDot = a.mV.mafLane[0] * b.mV.mafLane[0]
                      + a.mV.mafLane[1] * b.mV.mafLane[1]
                      + a.mV.mafLane[2] * b.mV.mafLane[2];
    return VecFloat(lfDot);
}

inline Vector3 Cross(Vector3::InParam a, Vector3::InParam b)
{
    // gCrossProductPermuteConstant rotates lanes to [Y,Z,X]. The console builds
    //   a*perm(b) - perm(a)*b   then permutes the result back, which is exactly the
    //   y*z-z*y / z*x-x*z / x*y-y*x cross product.
    const float ax = a.mV.mafLane[0], ay = a.mV.mafLane[1], az = a.mV.mafLane[2];
    const float bx = b.mV.mafLane[0], by = b.mV.mafLane[1], bz = b.mV.mafLane[2];
    Vector3 r;
    r.mV.mafLane[0] = ay * bz - az * by;
    r.mV.mafLane[1] = az * bx - ax * bz;
    r.mV.mafLane[2] = ax * by - ay * bx;
    return r;
}

// =====================================================================================
//  Magnitude family
// =====================================================================================

inline VecFloat MagnitudeSquared(Vector3::InParam v)
{
    // dot(v,v) reduced over X+Y+Z, splatted.
    const float lfDot = v.mV.mafLane[0] * v.mV.mafLane[0]
                      + v.mV.mafLane[1] * v.mV.mafLane[1]
                      + v.mV.mafLane[2] * v.mV.mafLane[2];
    return VecFloat(lfDot);
}

inline VecFloat MagnitudeRecip(Vector3::InParam v)
{
    // dotproduct = |v|^2 ; recipSqrt = 1/sqrt(dotproduct) (vrsqrtefp + Newton).
    const float lfDot = v.mV.mafLane[0] * v.mV.mafLane[0]
                      + v.mV.mafLane[1] * v.mV.mafLane[1]
                      + v.mV.mafLane[2] * v.mV.mafLane[2];
    return VecFloat(1.0f / sqrtf(lfDot));
}

inline VecFloat Magnitude(Vector3::InParam v)
{
    // dotproduct = |v|^2 ; result = dotproduct * (1/sqrt(dotproduct)) == sqrt(dotproduct);
    // vsel guards: a zero dot yields zero (avoids 0 * inf from the reciprocal estimate).
    const float lfDot = v.mV.mafLane[0] * v.mV.mafLane[0]
                      + v.mV.mafLane[1] * v.mV.mafLane[1]
                      + v.mV.mafLane[2] * v.mV.mafLane[2];
    return VecFloat(lfDot == 0.0f ? 0.0f : lfDot * (1.0f / sqrtf(lfDot)));
}

// =====================================================================================
//  Normalize family
// =====================================================================================

inline Vector3 NormalizeEstimate(Vector3::InParam v)
{
    // temp = vrsqrtefp(dot) (raw estimate, no Newton refine); vmaddfp(v, temp, 0).
    const float lfDot = v.mV.mafLane[0] * v.mV.mafLane[0]
                      + v.mV.mafLane[1] * v.mV.mafLane[1]
                      + v.mV.mafLane[2] * v.mV.mafLane[2];
    const float lfScale = 1.0f / sqrtf(lfDot);
    Vector3 r;
    r.mV.mafLane[0] = v.mV.mafLane[0] * lfScale;
    r.mV.mafLane[1] = v.mV.mafLane[1] * lfScale;
    r.mV.mafLane[2] = v.mV.mafLane[2] * lfScale;
    return r;
}

inline Vector3 NormalizeFast(Vector3::InParam v)
{
    // temp = 1/sqrt(dot) (estimate + one Newton step); vmaddfp(v, temp, 0).
    const float lfDot = v.mV.mafLane[0] * v.mV.mafLane[0]
                      + v.mV.mafLane[1] * v.mV.mafLane[1]
                      + v.mV.mafLane[2] * v.mV.mafLane[2];
    const float lfScale = 1.0f / sqrtf(lfDot);
    Vector3 r;
    r.mV.mafLane[0] = v.mV.mafLane[0] * lfScale;
    r.mV.mafLane[1] = v.mV.mafLane[1] * lfScale;
    r.mV.mafLane[2] = v.mV.mafLane[2] * lfScale;
    return r;
}

inline Vector3 Normalize(Vector3::InParam v)
{
    // temp = 1/sqrt(dot) (estimate + two Newton steps); vmaddfp(v, temp, 0).
    const float lfDot = v.mV.mafLane[0] * v.mV.mafLane[0]
                      + v.mV.mafLane[1] * v.mV.mafLane[1]
                      + v.mV.mafLane[2] * v.mV.mafLane[2];
    const float lfScale = 1.0f / sqrtf(lfDot);
    Vector3 r;
    r.mV.mafLane[0] = v.mV.mafLane[0] * lfScale;
    r.mV.mafLane[1] = v.mV.mafLane[1] * lfScale;
    r.mV.mafLane[2] = v.mV.mafLane[2] * lfScale;
    return r;
}

inline Vector3 Normalize(Vector3::InParam v, VecFloat& magnitude)
{
    // one_over_rqrt = 1/sqrt(dot); normalized = v * one_over_rqrt;
    // result_mag = dot * one_over_rqrt == |v| ; vsel guards a zero-length input to mag 0.
    const float lfDot = v.mV.mafLane[0] * v.mV.mafLane[0]
                      + v.mV.mafLane[1] * v.mV.mafLane[1]
                      + v.mV.mafLane[2] * v.mV.mafLane[2];
    const float lfScale = 1.0f / sqrtf(lfDot);
    Vector3 r;
    r.mV.mafLane[0] = v.mV.mafLane[0] * lfScale;
    r.mV.mafLane[1] = v.mV.mafLane[1] * lfScale;
    r.mV.mafLane[2] = v.mV.mafLane[2] * lfScale;
    magnitude = VecFloat(lfDot == 0.0f ? 0.0f : lfDot * lfScale);
    return r;
}

inline VecFloat NormalizeReturnMagnitude(Vector3::InParam v, Vector3& result)
{
    // Same as Normalize(v, mag) but returns the magnitude and writes the unit vector out.
    const float lfDot = v.mV.mafLane[0] * v.mV.mafLane[0]
                      + v.mV.mafLane[1] * v.mV.mafLane[1]
                      + v.mV.mafLane[2] * v.mV.mafLane[2];
    const float lfScale = 1.0f / sqrtf(lfDot);
    result.mV.mafLane[0] = v.mV.mafLane[0] * lfScale;
    result.mV.mafLane[1] = v.mV.mafLane[1] * lfScale;
    result.mV.mafLane[2] = v.mV.mafLane[2] * lfScale;
    return VecFloat(lfDot == 0.0f ? 0.0f : lfDot * lfScale);
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
    const float lfT = t.mV.mafLane[VectorAxisX];
    Vector3 r;
    r.mV.mafLane[0] = (v1.mV.mafLane[0] - v0.mV.mafLane[0]) * lfT + v0.mV.mafLane[0];
    r.mV.mafLane[1] = (v1.mV.mafLane[1] - v0.mV.mafLane[1]) * lfT + v0.mV.mafLane[1];
    r.mV.mafLane[2] = (v1.mV.mafLane[2] - v0.mV.mafLane[2]) * lfT + v0.mV.mafLane[2];
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
    // vxor(v, signMask): flip the sign bit of every lane.
    Vector3 r;
    r.mV.mafLane[0] = -v.mV.mafLane[0];
    r.mV.mafLane[1] = -v.mV.mafLane[1];
    r.mV.mafLane[2] = -v.mV.mafLane[2];
    return r;
}

inline Vector3 Min(Vector3::InParam v0, Vector3::InParam v1)
{
    // vminfp(v0,v1)
    Vector3 r;
    r.mV.mafLane[0] = v0.mV.mafLane[0] < v1.mV.mafLane[0] ? v0.mV.mafLane[0] : v1.mV.mafLane[0];
    r.mV.mafLane[1] = v0.mV.mafLane[1] < v1.mV.mafLane[1] ? v0.mV.mafLane[1] : v1.mV.mafLane[1];
    r.mV.mafLane[2] = v0.mV.mafLane[2] < v1.mV.mafLane[2] ? v0.mV.mafLane[2] : v1.mV.mafLane[2];
    return r;
}

inline Vector3 Max(Vector3::InParam v0, Vector3::InParam v1)
{
    // vmaxfp(v0,v1)
    Vector3 r;
    r.mV.mafLane[0] = v0.mV.mafLane[0] > v1.mV.mafLane[0] ? v0.mV.mafLane[0] : v1.mV.mafLane[0];
    r.mV.mafLane[1] = v0.mV.mafLane[1] > v1.mV.mafLane[1] ? v0.mV.mafLane[1] : v1.mV.mafLane[1];
    r.mV.mafLane[2] = v0.mV.mafLane[2] > v1.mV.mafLane[2] ? v0.mV.mafLane[2] : v1.mV.mafLane[2];
    return r;
}

inline Vector3 Clamp(Vector3::InParam v0, Vector3::InParam v1, Vector3::InParam v2)
{
    // r = vmaxfp(v0,v1); r = vminfp(r,v2): clamp v0 into [v1,v2] per lane.
    return Min(Max(v0, v1), v2);
}

inline Vector3 Abs(Vector3::InParam v)
{
    // vandc(v, signBitMask): clear the sign bit of every lane.
    Vector3 r;
    r.mV.mafLane[0] = v.mV.mafLane[0] < 0.0f ? -v.mV.mafLane[0] : v.mV.mafLane[0];
    r.mV.mafLane[1] = v.mV.mafLane[1] < 0.0f ? -v.mV.mafLane[1] : v.mV.mafLane[1];
    r.mV.mafLane[2] = v.mV.mafLane[2] < 0.0f ? -v.mV.mafLane[2] : v.mV.mafLane[2];
    return r;
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
    const float s = a.mV.mafLane[VectorAxisX];
    v.mV.mafLane[0] *= s; v.mV.mafLane[1] *= s; v.mV.mafLane[2] *= s;
    return v;
}

inline Vector3& operator /= (Vector3& v, VecFloat::InParam a)
{
    const float recip = 1.0f / a.mV.mafLane[VectorAxisX];
    v.mV.mafLane[0] *= recip; v.mV.mafLane[1] *= recip; v.mV.mafLane[2] *= recip;
    return v;
}

inline Vector3& operator += (Vector3& v, VecFloat::InParam a)
{
    // vaddfp(v, a): a is a broadcast so the same scalar is added to every lane.
    const float s = a.mV.mafLane[VectorAxisX];
    v.mV.mafLane[0] += s; v.mV.mafLane[1] += s; v.mV.mafLane[2] += s;
    return v;
}

inline Vector3& operator -= (Vector3& v, VecFloat::InParam a)
{
    const float s = a.mV.mafLane[VectorAxisX];
    v.mV.mafLane[0] -= s; v.mV.mafLane[1] -= s; v.mV.mafLane[2] -= s;
    return v;
}

inline Vector3 operator * (Vector3::InParam v, VecFloat::InParam s) { return Mult(v, s); }
inline Vector3 operator * (VecFloat::InParam s, Vector3::InParam v) { return Mult(v, s); }
inline Vector3 operator / (Vector3::InParam v, VecFloat::InParam s) { return Divide(v, s); }

inline Vector3 operator + (Vector3::InParam v, VecFloat::InParam s)
{
    const float a = s.mV.mafLane[VectorAxisX];
    Vector3 r;
    r.mV.mafLane[0] = v.mV.mafLane[0] + a;
    r.mV.mafLane[1] = v.mV.mafLane[1] + a;
    r.mV.mafLane[2] = v.mV.mafLane[2] + a;
    return r;
}

inline Vector3 operator - (Vector3::InParam v, VecFloat::InParam s)
{
    const float a = s.mV.mafLane[VectorAxisX];
    Vector3 r;
    r.mV.mafLane[0] = v.mV.mafLane[0] - a;
    r.mV.mafLane[1] = v.mV.mafLane[1] - a;
    r.mV.mafLane[2] = v.mV.mafLane[2] - a;
    return r;
}

inline Vector3 operator + (VecFloat::InParam s, Vector3::InParam v) { return v + s; }

inline Vector3 operator - (VecFloat::InParam s, Vector3::InParam v)
{
    // vsubfp(s, v): scalar minus each lane.
    const float a = s.mV.mafLane[VectorAxisX];
    Vector3 r;
    r.mV.mafLane[0] = a - v.mV.mafLane[0];
    r.mV.mafLane[1] = a - v.mV.mafLane[1];
    r.mV.mafLane[2] = a - v.mV.mafLane[2];
    return r;
}

// ---- v op= float ------------------------------------------------------------------

inline Vector3& operator *= (Vector3& v, float s)
{
    v.mV.mafLane[0] *= s; v.mV.mafLane[1] *= s; v.mV.mafLane[2] *= s;
    return v;
}

inline Vector3& operator /= (Vector3& v, float s)
{
    const float recip = 1.0f / s;
    v.mV.mafLane[0] *= recip; v.mV.mafLane[1] *= recip; v.mV.mafLane[2] *= recip;
    return v;
}

inline Vector3 operator * (Vector3::InParam v, float s) { return Mult(v, s); }
inline Vector3 operator * (float s, Vector3::InParam v) { return Mult(v, s); }
inline Vector3 operator / (Vector3::InParam v, float s) { return Divide(v, s); }

inline Vector3 operator + (Vector3::InParam v, float s)
{
    Vector3 r;
    r.mV.mafLane[0] = v.mV.mafLane[0] + s;
    r.mV.mafLane[1] = v.mV.mafLane[1] + s;
    r.mV.mafLane[2] = v.mV.mafLane[2] + s;
    return r;
}

inline Vector3 operator - (Vector3::InParam v, float s)
{
    Vector3 r;
    r.mV.mafLane[0] = v.mV.mafLane[0] - s;
    r.mV.mafLane[1] = v.mV.mafLane[1] - s;
    r.mV.mafLane[2] = v.mV.mafLane[2] - s;
    return r;
}

inline Vector3 operator + (float s, Vector3::InParam v) { return v + s; }

inline Vector3 operator - (float s, Vector3::InParam v)
{
    Vector3 r;
    r.mV.mafLane[0] = s - v.mV.mafLane[0];
    r.mV.mafLane[1] = s - v.mV.mafLane[1];
    r.mV.mafLane[2] = s - v.mV.mafLane[2];
    return r;
}

// ---- v op= VecFloatRef<INDEX> -----------------------------------------------------

template<VectorAxis INDEX>
inline Vector3& operator *= (Vector3& v, VecFloatRef<INDEX> a)
{
    const float s = a.mV.mafLane[INDEX];
    v.mV.mafLane[0] *= s; v.mV.mafLane[1] *= s; v.mV.mafLane[2] *= s;
    return v;
}

template<VectorAxis INDEX>
inline Vector3& operator /= (Vector3& v, VecFloatRef<INDEX> a)
{
    const float recip = 1.0f / a.mV.mafLane[INDEX];
    v.mV.mafLane[0] *= recip; v.mV.mafLane[1] *= recip; v.mV.mafLane[2] *= recip;
    return v;
}

template<VectorAxis INDEX>
inline Vector3& operator += (Vector3& v, VecFloatRef<INDEX> a)
{
    const float s = a.mV.mafLane[INDEX];
    v.mV.mafLane[0] += s; v.mV.mafLane[1] += s; v.mV.mafLane[2] += s;
    return v;
}

template<VectorAxis INDEX>
inline Vector3& operator -= (Vector3& v, VecFloatRef<INDEX> a)
{
    const float s = a.mV.mafLane[INDEX];
    v.mV.mafLane[0] -= s; v.mV.mafLane[1] -= s; v.mV.mafLane[2] -= s;
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
    const float a = s.mV.mafLane[INDEX];
    Vector3 r;
    r.mV.mafLane[0] = v.mV.mafLane[0] + a;
    r.mV.mafLane[1] = v.mV.mafLane[1] + a;
    r.mV.mafLane[2] = v.mV.mafLane[2] + a;
    return r;
}

template<VectorAxis INDEX>
inline Vector3 operator - (Vector3::InParam v, VecFloatRef<INDEX> s)
{
    const float a = s.mV.mafLane[INDEX];
    Vector3 r;
    r.mV.mafLane[0] = v.mV.mafLane[0] - a;
    r.mV.mafLane[1] = v.mV.mafLane[1] - a;
    r.mV.mafLane[2] = v.mV.mafLane[2] - a;
    return r;
}

template<VectorAxis INDEX>
inline Vector3 operator + (VecFloatRef<INDEX> s, Vector3::InParam v) { return v + s; }

template<VectorAxis INDEX>
inline Vector3 operator - (VecFloatRef<INDEX> s, Vector3::InParam v)
{
    const float a = s.mV.mafLane[INDEX];
    Vector3 r;
    r.mV.mafLane[0] = a - v.mV.mafLane[0];
    r.mV.mafLane[1] = a - v.mV.mafLane[1];
    r.mV.mafLane[2] = a - v.mV.mafLane[2];
    return r;
}

// ---- v op= VecFloatRefIndex -------------------------------------------------------

inline Vector3& operator *= (Vector3& v, VecFloatRefIndex::InParam a)
{
    const float s = a.mV.mafLane[a.mIndex];
    v.mV.mafLane[0] *= s; v.mV.mafLane[1] *= s; v.mV.mafLane[2] *= s;
    return v;
}

inline Vector3& operator /= (Vector3& v, VecFloatRefIndex::InParam a)
{
    const float recip = 1.0f / a.mV.mafLane[a.mIndex];
    v.mV.mafLane[0] *= recip; v.mV.mafLane[1] *= recip; v.mV.mafLane[2] *= recip;
    return v;
}

inline Vector3& operator += (Vector3& v, VecFloatRefIndex::InParam a)
{
    const float s = a.mV.mafLane[a.mIndex];
    v.mV.mafLane[0] += s; v.mV.mafLane[1] += s; v.mV.mafLane[2] += s;
    return v;
}

inline Vector3& operator -= (Vector3& v, VecFloatRefIndex::InParam a)
{
    const float s = a.mV.mafLane[a.mIndex];
    v.mV.mafLane[0] -= s; v.mV.mafLane[1] -= s; v.mV.mafLane[2] -= s;
    return v;
}

inline Vector3 operator * (Vector3::InParam v, VecFloatRefIndex::InParam s) { return Mult(v, s); }
inline Vector3 operator * (VecFloatRefIndex::InParam s, Vector3::InParam v) { return Mult(v, s); }
inline Vector3 operator / (Vector3::InParam v, VecFloatRefIndex::InParam s) { return Divide(v, s); }

inline Vector3 operator + (Vector3::InParam v, VecFloatRefIndex::InParam s)
{
    const float a = s.mV.mafLane[s.mIndex];
    Vector3 r;
    r.mV.mafLane[0] = v.mV.mafLane[0] + a;
    r.mV.mafLane[1] = v.mV.mafLane[1] + a;
    r.mV.mafLane[2] = v.mV.mafLane[2] + a;
    return r;
}

inline Vector3 operator - (Vector3::InParam v, VecFloatRefIndex::InParam s)
{
    const float a = s.mV.mafLane[s.mIndex];
    Vector3 r;
    r.mV.mafLane[0] = v.mV.mafLane[0] - a;
    r.mV.mafLane[1] = v.mV.mafLane[1] - a;
    r.mV.mafLane[2] = v.mV.mafLane[2] - a;
    return r;
}

inline Vector3 operator + (VecFloatRefIndex::InParam s, Vector3::InParam v) { return v + s; }

inline Vector3 operator - (VecFloatRefIndex::InParam s, Vector3::InParam v)
{
    const float a = s.mV.mafLane[s.mIndex];
    Vector3 r;
    r.mV.mafLane[0] = a - v.mV.mafLane[0];
    r.mV.mafLane[1] = a - v.mV.mafLane[1];
    r.mV.mafLane[2] = a - v.mV.mafLane[2];
    return r;
}

// =====================================================================================
//  Vector-Vector operators
// =====================================================================================

inline Vector3 operator - (Vector3::InParam v) { return Negate(v); }

inline Vector3& operator += (Vector3& v, Vector3::InParam b)
{
    v.mV.mafLane[0] += b.mV.mafLane[0];
    v.mV.mafLane[1] += b.mV.mafLane[1];
    v.mV.mafLane[2] += b.mV.mafLane[2];
    return v;
}

inline Vector3& operator -= (Vector3& v, Vector3::InParam b)
{
    v.mV.mafLane[0] -= b.mV.mafLane[0];
    v.mV.mafLane[1] -= b.mV.mafLane[1];
    v.mV.mafLane[2] -= b.mV.mafLane[2];
    return v;
}

inline Vector3& operator *= (Vector3& v, Vector3::InParam b)
{
    v.mV.mafLane[0] *= b.mV.mafLane[0];
    v.mV.mafLane[1] *= b.mV.mafLane[1];
    v.mV.mafLane[2] *= b.mV.mafLane[2];
    return v;
}

inline Vector3& operator /= (Vector3& v, Vector3::InParam b)
{
    v.mV.mafLane[0] /= b.mV.mafLane[0];
    v.mV.mafLane[1] /= b.mV.mafLane[1];
    v.mV.mafLane[2] /= b.mV.mafLane[2];
    return v;
}

inline Vector3 operator * (Vector3::InParam a, Vector3::InParam b) { return Mult(a, b); }
inline Vector3 operator / (Vector3::InParam a, Vector3::InParam b)
{
    Vector3 r;
    r.mV.mafLane[0] = a.mV.mafLane[0] / b.mV.mafLane[0];
    r.mV.mafLane[1] = a.mV.mafLane[1] / b.mV.mafLane[1];
    r.mV.mafLane[2] = a.mV.mafLane[2] / b.mV.mafLane[2];
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

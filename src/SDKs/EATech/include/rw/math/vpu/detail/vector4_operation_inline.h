#pragma once

// VPU Vector4 operations on the single SDK class definition.
// Recovered VMX primitives retain all four lanes; unrecovered scalar algorithms
// and declaration-only helpers below remain separate native-recovery work.

#include <cmath>

#include "SDKs/EATech/include/rw/math/vpu/vector4.h"
#include "SDKs/EATech/include/rw/math/vpu/scalar.h"

namespace rw
{
namespace math
{
namespace vpu
{

namespace detail
{
    // Independent full4-lane witnesses: MainMap82469BD0..C08 and the native
    // four-component normalization82BC2C5C..8C. Keep both multiply roundings
    // and the fused residual/correction boundaries of each refinement.
    __forceinline VectorIntrinsic Vector4NativeReciprocalRoot(VectorIntrinsic squared, int refinements)
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
}

// =====================================================================================
//  Equality / validity / similarity -- DECLARATION-ONLY + FLAGGED
// =====================================================================================

inline bool operator == (Vector4::InParam a, Vector4::InParam b) { return _asmIsEqualV4(a.mV, b.mV); }
inline bool operator != (Vector4::InParam a, Vector4::InParam b) { return !_asmIsEqualV4(a.mV, b.mV); }
bool IsZero(Vector4::InParam v, VecFloat::InParam tolerance);              // FLAGGED: _asmIsZeroV4
bool IsSimilar(Vector4::InParam a, Vector4::InParam b, VecFloat::InParam tolerance); // FLAGGED: _asmIsSimilarV4
// rwmath vector4_operation_inline.h: the four lanes' scalar predicates.
inline bool IsValid(Vector4::InParam v)
{
    return (IsValid(v.X()) && IsValid(v.Y()) && IsValid(v.Z()) && IsValid(v.W()));
}

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
    // ARTIST825B2768 uses distinct Vector4 rows of the native matrix inverse.
    return VecFloat(__vmsum4fp(a.mV, b.mV));
}

// rwmath 1.02.00 detail/vector4_operation_inline.h (LegacySrc :97..:169 / :260 / :362..:672 / :678..:717): the Xenon
// expansions of the VecFloat operator set (detail/vec_float_operation_inline.h header), as for Vector3.
__forceinline Vector4 Mult(Vector4::InParam v, VecFloat::InParam s)
{
    VectorIntrinsic ret;
    ret = __vmulfp((v.mV), (s.mV));
    return Vector4( ret );
}

__forceinline Vector4 operator * (Vector4::InParam v, Vector4::InParam s)
{
    VectorIntrinsic ret;
    ret = __vmulfp((v.mV), (s.mV));
    return Vector4( ret );
}

__forceinline Vector4 operator / (Vector4::InParam v, Vector4::InParam s)
{
    VectorIntrinsic ret;
    { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(s.mV);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (s.mV), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (s.mV), (vOne))), (secondEstimate)); }; ret = __vmulfp((reciprocal), (v.mV)); }
    return Vector4( ret );
}

__forceinline Vector4 Mult(Vector4::InParam v, float s)
{
    VectorIntrinsic result;
    { ::rw::math::vpu::VectorIntrinsic floatVec; { ::rw::math::vpu::VectorIntrinsic u = { s }; floatVec = __vspltw(u, 0); }; result = __vmulfp((v.mV), (floatVec));; };
    return Vector4( result );
}

template<VectorAxis INDEX> Vector4 Mult(Vector4::InParam v, VecFloatRef<INDEX> s)
{
    VectorIntrinsic result;
    { ::rw::math::vpu::VectorIntrinsic splattedAxis = __vspltw(s.mV, (INDEX)); result = __vmulfp((v.mV), (splattedAxis)); };
    return Vector4(result);
}

__forceinline Vector4 Mult(Vector4::InParam v, VecFloatRefIndex::InParam s)
{
    VectorIntrinsic result;
    VectorIntrinsic sVI;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(s.mIndex))), 0); sVI = __vperm(s.mV, s.mV, permConstant); };
    result = __vmulfp((v.mV), (sVI));;
    return Vector4(result);
}

__forceinline Vector4 Divide(Vector4::InParam v, VecFloat::InParam s)
{
    VectorIntrinsic result;
    { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(s.mV);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (s.mV), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (s.mV), (vOne))), (secondEstimate)); }; result = __vmulfp((reciprocal), (v.mV)); };
    return Vector4( result );
}

__forceinline Vector4 Divide(Vector4::InParam v, float s)
{
    VectorIntrinsic ret;
    { ::rw::math::vpu::VectorIntrinsic reciprocal; float floatRecip = 1.0f/(s); { ::rw::math::vpu::VectorIntrinsic u = { floatRecip }; reciprocal = __vspltw(u, 0); }; ret = __vmulfp((reciprocal), (v.mV)); };
    return Vector4( ret );
}

template<VectorAxis INDEX> Vector4 Divide(Vector4::InParam v, VecFloatRef<INDEX> s)
{
    VectorIntrinsic result;
    { ::rw::math::vpu::VectorIntrinsic splattedAxis = __vspltw(s.mV, (INDEX)); { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(splattedAxis);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (splattedAxis), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (splattedAxis), (vOne))), (secondEstimate)); }; result = __vmulfp((reciprocal), (v.mV)); }; };
    return Vector4(result);
}

__forceinline Vector4 Divide(Vector4::InParam v, VecFloatRefIndex::InParam s)
{
    VectorIntrinsic result;
    VectorIntrinsic sVI;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(s.mIndex))), 0); sVI = __vperm(s.mV, s.mV, permConstant); };
    { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(sVI);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (sVI), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (sVI), (vOne))), (secondEstimate)); }; result = __vmulfp((reciprocal), (v.mV)); };
    return Vector4(result);
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
    return Dot(v, v);
}

inline VecFloat MagnitudeRecip(Vector4::InParam v)
{
    return VecFloat(detail::Vector4NativeReciprocalRoot(__vmsum4fp(v.mV, v.mV), 2));
}

inline VecFloat Magnitude(Vector4::InParam v)
{
    const VectorIntrinsic squared = __vmsum4fp(v.mV, v.mV);
    const VectorIntrinsic reciprocal = detail::Vector4NativeReciprocalRoot(squared, 2);
    const VectorIntrinsic zero = __vspltisw(0);
    return VecFloat(__vsel(__vmulfp(squared, reciprocal), zero, __vcmpeqfp(squared, zero)));
}

// =====================================================================================
//  Normalize family (full-4)
// =====================================================================================

inline Vector4 NormalizeEstimate(Vector4::InParam v)
{
    const VectorIntrinsic squared = __vmsum4fp(v.mV, v.mV);
    return Vector4(__vmulfp(v.mV, detail::Vector4NativeReciprocalRoot(squared, 0)));
}

inline Vector4 NormalizeFast(Vector4::InParam v)
{
    const VectorIntrinsic squared = __vmsum4fp(v.mV, v.mV);
    return Vector4(__vmulfp(v.mV, detail::Vector4NativeReciprocalRoot(squared, 1)));
}

inline Vector4 Normalize(Vector4::InParam v)
{
    const VectorIntrinsic squared = __vmsum4fp(v.mV, v.mV);
    return Vector4(__vmulfp(v.mV, detail::Vector4NativeReciprocalRoot(squared, 2)));
}

inline Vector4 Normalize(Vector4::InParam v, VecFloat& magnitude)
{
    const VectorIntrinsic squared = __vmsum4fp(v.mV, v.mV);
    const VectorIntrinsic reciprocal = detail::Vector4NativeReciprocalRoot(squared, 2);
    const VectorIntrinsic zero = __vspltisw(0);
    magnitude = VecFloat(__vsel(__vmulfp(squared, reciprocal), zero, __vcmpeqfp(squared, zero)));
    return Vector4(__vmulfp(v.mV, reciprocal));
}

inline VecFloat NormalizeReturnMagnitude(Vector4::InParam v, Vector4& result)
{
    VecFloat magnitude;
    result = Normalize(v, magnitude);
    return magnitude;
}

// =====================================================================================
//  Lerp / NLerp (full-4)
// =====================================================================================

__forceinline Vector4 Lerp(Vector4::InParam v0, Vector4::InParam v1, VecFloat::InParam t)
{
    VectorIntrinsic ret;
    { ::rw::math::vpu::VectorIntrinsic temp_lerp_v; temp_lerp_v = __vsubfp( v1.mV, v0.mV ); ret = __vmaddfp((temp_lerp_v), (t.mV), (v0.mV));; };
    return Vector4(ret);
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

__forceinline Vector4& operator *= (Vector4& v, VecFloat::InParam a)
{
    v.mV = __vmulfp((v.mV), (a.mV));;
    return v;
}

__forceinline Vector4& operator /= (Vector4& v, VecFloat::InParam a)
{
    { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(a.mV);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (a.mV), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (a.mV), (vOne))), (secondEstimate)); }; v.mV = __vmulfp((reciprocal), (v.mV)); };
    return v;
}

__forceinline Vector4& operator += (Vector4& v, VecFloat::InParam a)
{
    v.mV = __vaddfp(v.mV, a.mV);
    return v;
}

__forceinline Vector4& operator -= (Vector4& v, VecFloat::InParam a)
{
    v.mV = __vsubfp( v.mV, a.mV );
    return v;
}

__forceinline Vector4 operator * (Vector4::InParam v, VecFloat::InParam s)
{
    VectorIntrinsic ret;
    ret = __vmulfp((v.mV), (s.mV));
    return Vector4( ret );
}

__forceinline Vector4 operator * (VecFloat::InParam s, Vector4::InParam v)
{
    VectorIntrinsic ret;
    ret = __vmulfp((s.mV), (v.mV));
    return Vector4( ret );
}

__forceinline Vector4 operator / (Vector4::InParam v, VecFloat::InParam s)
{
    VectorIntrinsic ret;
    { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(s.mV);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (s.mV), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (s.mV), (vOne))), (secondEstimate)); }; ret = __vmulfp((reciprocal), (v.mV)); }
    return Vector4( ret );
}

__forceinline Vector4 operator + (Vector4::InParam v, VecFloat::InParam s)
{
    return Vector4(__vaddfp(v.mV, s.mV));
}

__forceinline Vector4 operator - (Vector4::InParam v, VecFloat::InParam s)
{
    return Vector4(__vsubfp( v.mV, s.mV ));
}

__forceinline Vector4 operator + (VecFloat::InParam s, Vector4::InParam v)
{
    return Vector4(__vaddfp(v.mV, s.mV));
}

__forceinline Vector4 operator - (VecFloat::InParam s, Vector4::InParam v)
{
    return Vector4(__vsubfp( s.mV, v.mV ));
}

__forceinline Vector4& operator *= (Vector4& v, float a)
{
    { ::rw::math::vpu::VectorIntrinsic floatVec; { ::rw::math::vpu::VectorIntrinsic u = { a }; floatVec = __vspltw(u, 0); }; v.mV = __vmulfp((v.mV), (floatVec));; };
    return v;
}

__forceinline Vector4& operator /= (Vector4& v, float a)
{
    { ::rw::math::vpu::VectorIntrinsic reciprocal; float floatRecip = 1.0f/(a); { ::rw::math::vpu::VectorIntrinsic u = { floatRecip }; reciprocal = __vspltw(u, 0); }; v.mV = __vmulfp((reciprocal), (v.mV)); };
    return v;
}

__forceinline Vector4 operator * (Vector4::InParam v, float s)
{
    VectorIntrinsic result;
    { ::rw::math::vpu::VectorIntrinsic floatVec; { ::rw::math::vpu::VectorIntrinsic u = { s }; floatVec = __vspltw(u, 0); }; result = __vmulfp((v.mV), (floatVec));; };
    return Vector4( result );
}

__forceinline Vector4 operator * (float s, Vector4::InParam v)
{
    VectorIntrinsic result;
    { ::rw::math::vpu::VectorIntrinsic floatVec; { ::rw::math::vpu::VectorIntrinsic u = { s }; floatVec = __vspltw(u, 0); }; result = __vmulfp((v.mV), (floatVec));; };
    return Vector4( result );
}

__forceinline Vector4 operator / (Vector4::InParam v, float s)
{
    VectorIntrinsic result;
    { ::rw::math::vpu::VectorIntrinsic reciprocal; float floatRecip = 1.0f/(s); { ::rw::math::vpu::VectorIntrinsic u = { floatRecip }; reciprocal = __vspltw(u, 0); }; result = __vmulfp((reciprocal), (v.mV)); };
    return Vector4(result);
}

__forceinline Vector4 operator + (Vector4::InParam v, float s)
{
    VectorIntrinsic sVI;
    { ::rw::math::vpu::VectorIntrinsic u = { s }; sVI = __vspltw(u, 0); };
    return Vector4(__vaddfp(v.mV, sVI));
}

__forceinline Vector4 operator - (Vector4::InParam v, float s)
{
    VectorIntrinsic sVI;
    { ::rw::math::vpu::VectorIntrinsic u = { s }; sVI = __vspltw(u, 0); };
    return Vector4(__vsubfp( v.mV, sVI ));
}

__forceinline Vector4 operator + (float s, Vector4::InParam v)
{
    VectorIntrinsic sVI;
    { ::rw::math::vpu::VectorIntrinsic u = { s }; sVI = __vspltw(u, 0); };
    return Vector4(__vaddfp(v.mV, sVI));
}

__forceinline Vector4 operator - (float s, Vector4::InParam v)
{
    VectorIntrinsic sVI;
    { ::rw::math::vpu::VectorIntrinsic u = { s }; sVI = __vspltw(u, 0); };
    return Vector4(__vsubfp( sVI, v.mV ));
}

template<VectorAxis INDEX>
__forceinline Vector4& operator *= (Vector4& v, VecFloatRef<INDEX> a)
{
    { ::rw::math::vpu::VectorIntrinsic splattedAxis = __vspltw(a.mV, (INDEX)); v.mV = __vmulfp((v.mV), (splattedAxis)); };
    return v;
}

template<VectorAxis INDEX>
__forceinline Vector4& operator /= (Vector4& v, VecFloatRef<INDEX> a)
{
    { ::rw::math::vpu::VectorIntrinsic splattedAxis = __vspltw(a.mV, (INDEX)); { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(splattedAxis);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (splattedAxis), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (splattedAxis), (vOne))), (secondEstimate)); }; v.mV = __vmulfp((reciprocal), (v.mV)); }; };
    return v;
}

template<VectorAxis INDEX>
__forceinline Vector4& operator += (Vector4& v, VecFloatRef<INDEX> a)
{
    VectorIntrinsic aVI;
    { aVI = __vspltw(a.mV, (int)(INDEX)); };
    v.mV = __vaddfp(v.mV, aVI);
    return v;
}

template<VectorAxis INDEX>
__forceinline Vector4& operator -= (Vector4& v, VecFloatRef<INDEX> a)
{
    VectorIntrinsic aVI;
    { aVI = __vspltw(a.mV, (int)(INDEX)); };
    v.mV = __vsubfp( v.mV, aVI );
    return v;
}

template<VectorAxis INDEX>
__forceinline Vector4 operator * (Vector4::InParam v, VecFloatRef<INDEX> s)
{
    VectorIntrinsic result;
    { ::rw::math::vpu::VectorIntrinsic splattedAxis = __vspltw(s.mV, (INDEX)); result = __vmulfp((v.mV), (splattedAxis)); };
    return Vector4(result);
}

template<VectorAxis INDEX>
__forceinline Vector4 operator * (VecFloatRef<INDEX> s, Vector4::InParam v)
{
    VectorIntrinsic result;
    { ::rw::math::vpu::VectorIntrinsic splattedAxis = __vspltw(s.mV, (INDEX)); result = __vmulfp((v.mV), (splattedAxis)); };
    return Vector4(result);
}

template<VectorAxis INDEX>
__forceinline Vector4 operator / (Vector4::InParam v, VecFloatRef<INDEX> s)
{
    VectorIntrinsic result;
    { ::rw::math::vpu::VectorIntrinsic splattedAxis = __vspltw(s.mV, (INDEX)); { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(splattedAxis);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (splattedAxis), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (splattedAxis), (vOne))), (secondEstimate)); }; result = __vmulfp((reciprocal), (v.mV)); }; };
    return Vector4(result);
}

template<VectorAxis INDEX>
__forceinline Vector4 operator + (Vector4::InParam v, VecFloatRef<INDEX> s)
{
    VectorIntrinsic sVI;
    { sVI = __vspltw(s.mV, (int)(INDEX)); };
    VectorIntrinsic ret = __vaddfp(v.mV, sVI);
    return Vector4(ret);
}

template<VectorAxis INDEX>
__forceinline Vector4 operator - (Vector4::InParam v, VecFloatRef<INDEX> s)
{
    VectorIntrinsic sVI;
    { sVI = __vspltw(s.mV, (int)(INDEX)); };
    VectorIntrinsic ret = __vsubfp( v.mV, sVI );
    return Vector4(ret);
}

template<VectorAxis INDEX>
__forceinline Vector4 operator + (VecFloatRef<INDEX> s, Vector4::InParam v)
{
    VectorIntrinsic sVI;
    { sVI = __vspltw(s.mV, (int)(INDEX)); };
    VectorIntrinsic ret = __vaddfp(v.mV, sVI);
    return Vector4(ret);
}

template<VectorAxis INDEX>
__forceinline Vector4 operator - (VecFloatRef<INDEX> s, Vector4::InParam v)
{
    VectorIntrinsic sVI;
    { sVI = __vspltw(s.mV, (int)(INDEX)); };
    VectorIntrinsic ret = __vsubfp( sVI, v.mV );
    return Vector4(ret);
}

__forceinline Vector4& operator *= (Vector4& v, VecFloatRefIndex::InParam a)
{
    VectorIntrinsic aVI;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVI = __vperm(a.mV, a.mV, permConstant); };
    v.mV = __vmulfp((v.mV), (aVI));;
    return v;
}

__forceinline Vector4& operator /= (Vector4& v, VecFloatRefIndex::InParam a)
{
    VectorIntrinsic aVI;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVI = __vperm(a.mV, a.mV, permConstant); };
    { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(aVI);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (aVI), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (aVI), (vOne))), (secondEstimate)); }; v.mV = __vmulfp((reciprocal), (v.mV)); };
    return v;
}

__forceinline Vector4& operator += (Vector4& v, VecFloatRefIndex::InParam a)
{
    VectorIntrinsic aVI;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVI = __vperm(a.mV, a.mV, permConstant); };
    v.mV = __vaddfp(v.mV, aVI);
    return v;
}

__forceinline Vector4& operator -= (Vector4& v, VecFloatRefIndex::InParam a)
{
    VectorIntrinsic aVI;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVI = __vperm(a.mV, a.mV, permConstant); };
    v.mV = __vsubfp( v.mV, aVI );
    return v;
}

__forceinline Vector4 operator * (Vector4::InParam v, VecFloatRefIndex::InParam s)
{
    VectorIntrinsic result;
    VectorIntrinsic sVI;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(s.mIndex))), 0); sVI = __vperm(s.mV, s.mV, permConstant); };
    result = __vmulfp((v.mV), (sVI));;
    return Vector4(result);
}

__forceinline Vector4 operator * (VecFloatRefIndex::InParam s, Vector4::InParam v)
{
    VectorIntrinsic result;
    VectorIntrinsic sVI;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(s.mIndex))), 0); sVI = __vperm(s.mV, s.mV, permConstant); };
    result = __vmulfp((sVI), (v.mV));;
    return Vector4(result);
}

__forceinline Vector4 operator / (Vector4::InParam v, VecFloatRefIndex::InParam s)
{
    VectorIntrinsic result;
    VectorIntrinsic sVI;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(s.mIndex))), 0); sVI = __vperm(s.mV, s.mV, permConstant); };
    { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(sVI);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (sVI), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (sVI), (vOne))), (secondEstimate)); }; result = __vmulfp((reciprocal), (v.mV)); };
    return Vector4(result);
}

__forceinline Vector4 operator + (Vector4::InParam v, VecFloatRefIndex::InParam s)
{
    VectorIntrinsic sVI;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(s.mIndex))), 0); sVI = __vperm(s.mV, s.mV, permConstant); };
    VectorIntrinsic ret = __vaddfp(v.mV, sVI);
    return Vector4(ret);
}

__forceinline Vector4 operator - (Vector4::InParam v, VecFloatRefIndex::InParam s)
{
    VectorIntrinsic sVI;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(s.mIndex))), 0); sVI = __vperm(s.mV, s.mV, permConstant); };
    VectorIntrinsic ret = __vsubfp( v.mV, sVI );
    return Vector4(ret);
}

__forceinline Vector4 operator + (VecFloatRefIndex::InParam s, Vector4::InParam v)
{
    VectorIntrinsic sVI;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(s.mIndex))), 0); sVI = __vperm(s.mV, s.mV, permConstant); };
    VectorIntrinsic ret = __vaddfp(v.mV, sVI);
    return Vector4(ret);
}

__forceinline Vector4 operator - (VecFloatRefIndex::InParam s, Vector4::InParam v)
{
    VectorIntrinsic sVI;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(s.mIndex))), 0); sVI = __vperm(s.mV, s.mV, permConstant); };
    VectorIntrinsic ret = __vsubfp( sVI, v.mV );
    return Vector4(ret);
}

// =====================================================================================
//  Vector-Vector operators
// =====================================================================================

__forceinline Vector4 operator - (Vector4::InParam v)
{
    VectorIntrinsic result;
    { ::rw::math::vpu::VectorIntrinsic signMask; signMask = __vspltisw(-1); signMask = __vslw(signMask, signMask);; result = __vxor(v.mV, signMask); };
    return Vector4(result);
}

__forceinline Vector4& operator += (Vector4& v, Vector4::InParam b)
{
    v.mV = __vaddfp(v.mV, b.mV);
    return v;
}

__forceinline Vector4& operator -= (Vector4& v, Vector4::InParam b)
{
    v.mV = __vsubfp( v.mV, b.mV );
    return v;
}

__forceinline Vector4& operator *= (Vector4& v, Vector4::InParam b)
{
    v.mV = __vmulfp((v.mV), (b.mV));;
    return v;
}

__forceinline Vector4& operator /= (Vector4& v, Vector4::InParam b)
{
    { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(b.mV);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (b.mV), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (b.mV), (vOne))), (secondEstimate)); }; v.mV = __vmulfp((reciprocal), (v.mV)); };
    return v;
}

__forceinline Vector4 operator - (Vector4::InParam a,Vector4::InParam b)
{
    return Vector4(__vsubfp( a.mV, b.mV ));
}

__forceinline Vector4 operator + (Vector4::InParam a, Vector4::InParam b)
{
    return Vector4(__vaddfp(a.mV, b.mV));
}

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

#pragma once

// VPU Vector3 operations on the single SDK class definition.
// Recovered VMX bodies retain all four stored lanes and native operation order.
// Scalar/ref overloads and declaration-only helpers that have not been recovered
// remain existing debt; this header is not an all-operation parity claim.

#include <cmath>

#include "SDKs/EATech/include/rw/math/vpu/vector3.h"
#include "SDKs/EATech/include/rw/math/vpu/scalar.h"

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

// rwmath vector3_operation_inline.h:22 / :27 (DecFIGS the same): the three-lane predicate _asmIsEqualV3.
__forceinline bool operator == (Vector3::InParam a, Vector3::InParam b)
{
    return _asmIsEqualV3(a.mV, b.mV);
}

__forceinline bool operator != (Vector3::InParam a, Vector3::InParam b)
{
    return !_asmIsEqualV3(a.mV, b.mV);
}
bool IsZero(Vector3::InParam v, VecFloat::InParam tolerance);              // FLAGGED: _asmIsZeroV3
bool IsSimilar(Vector3::InParam a, Vector3::InParam b, VecFloat::InParam epsilon); // FLAGGED: _asmIsSimilarV3
// rwmath vector3_operation_inline.h: each lane splatted (VecFloat from VecFloatRef) and
// tested by the scalar predicate -- CgsBox::IsValid's 3 vspltw + 3 vcmpeqfp. in ARTIST.
inline bool IsValid(Vector3::InParam v)
{
    return (IsValid(v.X()) && IsValid(v.Y()) && IsValid(v.Z()));
}

// rwmath vector3_operation_inline.h:257: v against itself through _asmIsEqualV3's permute (lane X into W) and the
// all-equal predicate; a NaN lane fails its self-comparison.
__forceinline bool IsAnyNaN(Vector3::InParam v)
{
    bool equal;
    ::rw::math::vpu::VectorIntrinsic v0comp = __vrlimi(v.mV, v.mV, 1, 1); ::rw::math::vpu::VectorIntrinsic v1comp = __vrlimi(v.mV, v.mV, 1, 1); { unsigned int cr = 0; __vcmpeqfpR(v0comp, v1comp, &cr); equal = ((cr & 0x80) == 0x80); };
    return !equal;
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

// rwmath 1.02.00 detail/vector3_operation_inline.h (LegacySrc :32..:60 / :69..:90 / :214 / :335..:646): Xenon bodies through
// the macro expansions the VecFloat operator set attests (detail/vec_float_operation_inline.h header); Mult(v, float) is
// Initialise 0x82712DBC..0x82712DE0 (zfill splat, vmulfp(v, splat)), Mult(v, VecFloat) Initialise 0x82712DA0.
__forceinline Vector3 Mult(Vector3::InParam v, VecFloat::InParam s)
{
    VectorIntrinsic ret;
    ret = __vmulfp((v.mV), (s.mV));
    return Vector3( ret );
}

__forceinline Vector3 Mult(Vector3::InParam v, float s)
{
    VectorIntrinsic result;
    { ::rw::math::vpu::VectorIntrinsic floatVec; { ::rw::math::vpu::VectorIntrinsic u = { s }; floatVec = __vspltw(u, 0); }; result = __vmulfp((v.mV), (floatVec));; };
    return Vector3( result );
}

template<VectorAxis INDEX> Vector3 Mult(Vector3::InParam v, VecFloatRef<INDEX> s)
{
    VectorIntrinsic result;
    { ::rw::math::vpu::VectorIntrinsic splattedAxis = __vspltw(s.mV, (INDEX)); result = __vmulfp((v.mV), (splattedAxis)); };
    return Vector3(result);
}

__forceinline Vector3 Mult(Vector3::InParam v, VecFloatRefIndex::InParam s)
{
    VectorIntrinsic result;
    VectorIntrinsic sVI;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(s.mIndex))), 0); sVI = __vperm(s.mV, s.mV, permConstant); };
    result = __vmulfp((v.mV), (sVI));;
    return Vector3(result);
}

inline Vector3 Divide(Vector3::InParam v, VecFloat::InParam s)
{
    return Vector3(__vmulfp(detail::Vector3NativeReciprocal(s.mV), v.mV));
}

__forceinline Vector3 Divide(Vector3::InParam v, float s)
{
    VectorIntrinsic ret;
    { ::rw::math::vpu::VectorIntrinsic reciprocal; float floatRecip = 1.0f/(s); { ::rw::math::vpu::VectorIntrinsic u = { floatRecip }; reciprocal = __vspltw(u, 0); }; ret = __vmulfp((reciprocal), (v.mV)); };
    return Vector3( ret );
}

template<VectorAxis INDEX> Vector3 Divide(Vector3::InParam v, VecFloatRef<INDEX> s)
{
    VectorIntrinsic ret;
    { ::rw::math::vpu::VectorIntrinsic reciprocal; float floatRecip = 1.0f/(s); { ::rw::math::vpu::VectorIntrinsic u = { floatRecip }; reciprocal = __vspltw(u, 0); }; ret = __vmulfp((reciprocal), (v.mV)); };
    return Vector3( ret );
}

__forceinline Vector3 Divide(Vector3::InParam v, VecFloatRefIndex::InParam s)
{
    VectorIntrinsic result;
    VectorIntrinsic sVI;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(s.mIndex))), 0); sVI = __vperm(s.mV, s.mV, permConstant); };
    { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(sVI);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (sVI), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (sVI), (vOne))), (secondEstimate)); }; result = __vmulfp((reciprocal), (v.mV)); };
    return Vector3(result);
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
    // rwmath 1.02.00 Magnitude builds its mask as vcmpeqfp(zero, dotproduct) (LegacySrc
    // vector3_operation_inline.h:144), unlike Normalize's (dot, zero); ARTIST keeps that operand
    // order, e.g. RaceCarEntityModuleDebugComponent::Update 0x822D5680 `vcmpeqfp v12, v12(zero), v0`.
    return VecFloat(__vsel(__vmulfp(squared, reciprocal), zero, __vcmpeqfp(zero, squared)));
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

__forceinline Vector3 Lerp(Vector3::InParam v0, Vector3::InParam v1, VecFloat::InParam t)
{
    VectorIntrinsic ret;
    { ::rw::math::vpu::VectorIntrinsic temp_lerp_v; temp_lerp_v = __vsubfp( v1.mV, v0.mV ); ret = __vmaddfp((temp_lerp_v), (t.mV), (v0.mV));; };
    return Vector3(ret);
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
//  vnor mask, vsel select) bodies then. Console forms recorded in the LegacySrc source.
// =====================================================================================

// =====================================================================================
//  IsZero / IsSimilar already declared above (FLAGGED).
//  Compound assignment + operators (scalar / VecFloat / VecFloatRef / VecFloatRefIndex)
// =====================================================================================

__forceinline Vector3& operator *= (Vector3& v, VecFloat::InParam a)
{
    v.mV = __vmulfp((v.mV), (a.mV));;
    return v;
}

__forceinline Vector3& operator /= (Vector3& v, VecFloat::InParam a)
{
    { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(a.mV);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (a.mV), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (a.mV), (vOne))), (secondEstimate)); }; v.mV = __vmulfp((reciprocal), (v.mV)); };
    return v;
}

__forceinline Vector3& operator += (Vector3& v, VecFloat::InParam a)
{
    v.mV = __vaddfp(v.mV, a.mV);
    return v;
}

__forceinline Vector3& operator -= (Vector3& v, VecFloat::InParam a)
{
    v.mV = __vsubfp( v.mV, a.mV );
    return v;
}

__forceinline Vector3 operator * (Vector3::InParam v, VecFloat::InParam s)
{
    VectorIntrinsic ret;
    ret = __vmulfp((v.mV), (s.mV));
    return Vector3( ret );
}

__forceinline Vector3 operator * (VecFloat::InParam s, Vector3::InParam v)
{
    VectorIntrinsic ret;
    ret = __vmulfp((v.mV), (s.mV));
    return Vector3( ret );
}

__forceinline Vector3 operator / (Vector3::InParam v, VecFloat::InParam s)
{
    VectorIntrinsic ret;
    { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(s.mV);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (s.mV), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (s.mV), (vOne))), (secondEstimate)); }; ret = __vmulfp((reciprocal), (v.mV)); }
    return Vector3( ret );
}

__forceinline Vector3 operator + (Vector3::InParam v, VecFloat::InParam s)
{
    return Vector3(__vaddfp(v.mV, s.mV));
}

__forceinline Vector3 operator - (Vector3::InParam v, VecFloat::InParam s)
{
    return Vector3(__vsubfp( v.mV, s.mV ));
}

__forceinline Vector3 operator + (VecFloat::InParam s, Vector3::InParam v)
{
    return Vector3(__vaddfp(v.mV, s.mV));
}

__forceinline Vector3 operator - (VecFloat::InParam s, Vector3::InParam v)
{
    return Vector3(__vsubfp( s.mV, v.mV ));
}

__forceinline Vector3& operator *= (Vector3& v, float s)
{
    VectorIntrinsic ret;
    ret = __vmulfp((v.mV), (RW_MATH_VPU_CreateVectorFromScalar_R(s)));;
    v.mV = ret;
    return v;
}

__forceinline Vector3& operator /= (Vector3& v, float s)
{
    VectorIntrinsic result;
    VectorIntrinsic divisor;
    divisor = RW_MATH_VPU_CreateVectorFromScalar_R(s);
    { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(divisor);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (divisor), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (divisor), (vOne))), (secondEstimate)); }; result = __vmulfp((reciprocal), (v.mV)); };
    v.mV = result;
    return v;
}

__forceinline Vector3 operator * (Vector3::InParam v, float s)
{
    VectorIntrinsic ret;
    ret = __vmulfp((v.mV), (RW_MATH_VPU_CreateVectorFromScalar_R(s)));;
    return Vector3(ret);
}

__forceinline Vector3 operator * (float s, Vector3::InParam v)
{
    VectorIntrinsic ret;
    ret = __vmulfp((v.mV), (RW_MATH_VPU_CreateVectorFromScalar_R(s)));;
    return Vector3(ret);
}

__forceinline Vector3 operator / (Vector3::InParam v, float s)
{
    VectorIntrinsic result;
    VectorIntrinsic divisor;
    divisor = RW_MATH_VPU_CreateVectorFromScalar_R(s);
    { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(divisor);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (divisor), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (divisor), (vOne))), (secondEstimate)); }; result = __vmulfp((reciprocal), (v.mV)); };
    return Vector3(result);
}

__forceinline Vector3 operator + (Vector3::InParam v, float s)
{
    return Vector3(__vaddfp(v.mV, RW_MATH_VPU_CreateVectorFromScalar_R(s)));
}

__forceinline Vector3 operator - (Vector3::InParam v, float s)
{
    return Vector3(__vsubfp( v.mV, RW_MATH_VPU_CreateVectorFromScalar_R(s) ));
}

__forceinline Vector3 operator + (float s, Vector3::InParam v)
{
    return Vector3(__vaddfp(v.mV, RW_MATH_VPU_CreateVectorFromScalar_R(s)));
}

__forceinline Vector3 operator - (float s, Vector3::InParam v)
{
    return Vector3(__vsubfp( RW_MATH_VPU_CreateVectorFromScalar_R(s), v.mV ));
}

template<VectorAxis INDEX>
__forceinline Vector3& operator *= (Vector3& v, VecFloatRef<INDEX> a)
{
    { ::rw::math::vpu::VectorIntrinsic splattedAxis = __vspltw(a.mV, (INDEX)); v.mV = __vmulfp((v.mV), (splattedAxis)); };
    return v;
}

template<VectorAxis INDEX>
__forceinline Vector3& operator /= (Vector3& v, VecFloatRef<INDEX> a)
{
    { ::rw::math::vpu::VectorIntrinsic splattedAxis = __vspltw(a.mV, (INDEX)); { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(splattedAxis);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (splattedAxis), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (splattedAxis), (vOne))), (secondEstimate)); }; v.mV = __vmulfp((reciprocal), (v.mV)); }; };
    return v;
}

template<VectorAxis INDEX>
__forceinline Vector3& operator += (Vector3& v, VecFloatRef<INDEX> a)
{
    VectorIntrinsic aVI;
    { aVI = __vspltw(a.mV, (int)(INDEX)); };
    v.mV = __vaddfp(v.mV, aVI);
    return v;
}

template<VectorAxis INDEX>
__forceinline Vector3& operator -= (Vector3& v, VecFloatRef<INDEX> a)
{
    VectorIntrinsic aVI;
    { aVI = __vspltw(a.mV, (int)(INDEX)); };
    v.mV = __vsubfp( v.mV, aVI );
    return v;
}

template<VectorAxis INDEX>
__forceinline Vector3 operator * (Vector3::InParam v, VecFloatRef<INDEX> s)
{
    VectorIntrinsic result;
    { ::rw::math::vpu::VectorIntrinsic splattedAxis = __vspltw(s.mV, (INDEX)); result = __vmulfp((v.mV), (splattedAxis)); };
    return Vector3(result);
}

template<VectorAxis INDEX>
__forceinline Vector3 operator * (VecFloatRef<INDEX> s, Vector3::InParam v)
{
    VectorIntrinsic result;
    { ::rw::math::vpu::VectorIntrinsic splattedAxis = __vspltw(s.mV, (INDEX)); result = __vmulfp((v.mV), (splattedAxis)); };
    return Vector3(result);
}

template<VectorAxis INDEX>
__forceinline Vector3 operator / (Vector3::InParam v, VecFloatRef<INDEX> s)
{
    VectorIntrinsic result;
    { ::rw::math::vpu::VectorIntrinsic splattedAxis = __vspltw(s.mV, (INDEX)); { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(splattedAxis);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (splattedAxis), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (splattedAxis), (vOne))), (secondEstimate)); }; result = __vmulfp((reciprocal), (v.mV)); }; };
    return Vector3(result);
}

template<VectorAxis INDEX>
__forceinline Vector3 operator + (Vector3::InParam v, VecFloatRef<INDEX> s)
{
    VectorIntrinsic sVI;
    { sVI = __vspltw(s.mV, (int)(INDEX)); };
    VectorIntrinsic ret = __vaddfp(v.mV, sVI);
    return Vector3(ret);
}

template<VectorAxis INDEX>
__forceinline Vector3 operator - (Vector3::InParam v, VecFloatRef<INDEX> s)
{
    VectorIntrinsic sVI;
    { sVI = __vspltw(s.mV, (int)(INDEX)); };
    VectorIntrinsic ret = __vsubfp( v.mV, sVI );
    return Vector3(ret);
}

template<VectorAxis INDEX>
__forceinline Vector3 operator + (VecFloatRef<INDEX> s, Vector3::InParam v)
{
    VectorIntrinsic sVI;
    { sVI = __vspltw(s.mV, (int)(INDEX)); };
    VectorIntrinsic ret = __vaddfp(v.mV, sVI);
    return Vector3(ret);
}

template<VectorAxis INDEX>
__forceinline Vector3 operator - (VecFloatRef<INDEX> s, Vector3::InParam v)
{
    VectorIntrinsic sVI;
    { sVI = __vspltw(s.mV, (int)(INDEX)); };
    VectorIntrinsic ret = __vsubfp( sVI, v.mV );
    return Vector3(ret);
}

__forceinline Vector3& operator *= (Vector3& v, VecFloatRefIndex::InParam a)
{
    VectorIntrinsic aVI;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVI = __vperm(a.mV, a.mV, permConstant); };
    v.mV = __vmulfp((v.mV), (aVI));;
    return v;
}

__forceinline Vector3& operator /= (Vector3& v, VecFloatRefIndex::InParam a)
{
    VectorIntrinsic aVI;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVI = __vperm(a.mV, a.mV, permConstant); };
    { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(aVI);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (aVI), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (aVI), (vOne))), (secondEstimate)); }; v.mV = __vmulfp((reciprocal), (v.mV)); };
    return v;
}

__forceinline Vector3& operator += (Vector3& v, VecFloatRefIndex::InParam a)
{
    VectorIntrinsic aVI;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVI = __vperm(a.mV, a.mV, permConstant); };
    v.mV = __vaddfp(v.mV, aVI);
    return v;
}

__forceinline Vector3& operator -= (Vector3& v, VecFloatRefIndex::InParam a)
{
    VectorIntrinsic aVI;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVI = __vperm(a.mV, a.mV, permConstant); };
    v.mV = __vsubfp( v.mV, aVI );
    return v;
}

__forceinline Vector3 operator * (Vector3::InParam v, VecFloatRefIndex::InParam s)
{
    VectorIntrinsic result;
    VectorIntrinsic sVI;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(s.mIndex))), 0); sVI = __vperm(s.mV, s.mV, permConstant); };
    result = __vmulfp((v.mV), (sVI));;
    return Vector3(result);
}

__forceinline Vector3 operator * (VecFloatRefIndex::InParam s, Vector3::InParam v)
{
    VectorIntrinsic result;
    VectorIntrinsic sVI;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(s.mIndex))), 0); sVI = __vperm(s.mV, s.mV, permConstant); };
    result = __vmulfp((sVI), (v.mV));;
    return Vector3(result);
}

__forceinline Vector3 operator / (Vector3::InParam v, VecFloatRefIndex::InParam s)
{
    VectorIntrinsic result;
    VectorIntrinsic sVI;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(s.mIndex))), 0); sVI = __vperm(s.mV, s.mV, permConstant); };
    { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(sVI);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (sVI), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (sVI), (vOne))), (secondEstimate)); }; result = __vmulfp((reciprocal), (v.mV)); };
    return Vector3(result);
}

__forceinline Vector3 operator + (Vector3::InParam v, VecFloatRefIndex::InParam s)
{
    VectorIntrinsic sVI;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(s.mIndex))), 0); sVI = __vperm(s.mV, s.mV, permConstant); };
    VectorIntrinsic ret = __vaddfp(v.mV, sVI);
    return Vector3(ret);
}

__forceinline Vector3 operator - (Vector3::InParam v, VecFloatRefIndex::InParam s)
{
    VectorIntrinsic sVI;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(s.mIndex))), 0); sVI = __vperm(s.mV, s.mV, permConstant); };
    VectorIntrinsic ret = __vsubfp( v.mV, sVI );
    return Vector3(ret);
}

__forceinline Vector3 operator + (VecFloatRefIndex::InParam s, Vector3::InParam v)
{
    VectorIntrinsic sVI;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(s.mIndex))), 0); sVI = __vperm(s.mV, s.mV, permConstant); };
    VectorIntrinsic ret = __vaddfp(v.mV, sVI);
    return Vector3(ret);
}

__forceinline Vector3 operator - (VecFloatRefIndex::InParam s, Vector3::InParam v)
{
    VectorIntrinsic sVI;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(s.mIndex))), 0); sVI = __vperm(s.mV, s.mV, permConstant); };
    VectorIntrinsic ret = __vsubfp( sVI, v.mV );
    return Vector3(ret);
}

// =====================================================================================
//  Vector-Vector operators
// =====================================================================================

// LegacySrc :109..:121 / :652..:690, the Xenon expansions of the VecFloat operator set.
__forceinline Vector3 operator * (Vector3::InParam a, Vector3::InParam b)
{
    VectorIntrinsic ret;
    ret = __vmulfp((a.mV), (b.mV));;
    return Vector3( ret );
}

__forceinline Vector3 operator / (Vector3::InParam a, Vector3::InParam b)
{
    VectorIntrinsic ret;
    { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(b.mV);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (b.mV), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (b.mV), (vOne))), (secondEstimate)); }; ret = __vmulfp((reciprocal), (a.mV)); };
    return Vector3( ret );
}

__forceinline Vector3 operator - (Vector3::InParam v)
{
    VectorIntrinsic result;
    { ::rw::math::vpu::VectorIntrinsic signMask; signMask = __vspltisw(-1); signMask = __vslw(signMask, signMask);; result = __vxor(v.mV, signMask); };
    return Vector3(result);
}

__forceinline Vector3& operator += (Vector3& v, Vector3::InParam b)
{
    v.mV = __vaddfp(v.mV, b.mV);
    return v;
}

__forceinline Vector3& operator -= (Vector3& v, Vector3::InParam b)
{
    v.mV = __vsubfp( v.mV, b.mV );
    return v;
}

__forceinline Vector3& operator *= (Vector3& v, Vector3::InParam b)
{
    v.mV = __vmulfp((v.mV), (b.mV));;
    return v;
}

__forceinline Vector3& operator /= (Vector3& v, Vector3::InParam b)
{
    { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(b.mV);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (b.mV), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (b.mV), (vOne))), (secondEstimate)); }; v.mV = __vmulfp((reciprocal), (v.mV)); };
    return v;
}

__forceinline Vector3 operator - (Vector3::InParam a,Vector3::InParam b)
{
    return Vector3(__vsubfp( a.mV, b.mV ));
}

__forceinline Vector3 operator + (Vector3::InParam a, Vector3::InParam b)
{
    return Vector3( __vaddfp(a.mV, b.mV) );
}

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

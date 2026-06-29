#pragma once

// Canonical RenderWare SDK home for the rw::math::vpu Vector3 operation vocabulary
// (EARenderWare rwmath 1.02.00, rw/math/vpu/vector3_operation.h -- sibling to types.h).
// Reconstructed from BURNOUT_X360_ARTIST.XEX with the public-header names and signatures
// locked from the Feb-2007 partial source (rwmath/1.02.00/include/rw/math/vpu/vector3_operation.h
// + detail/vector3_operation_inline.h) and the DecFIGS DWARF (BrnMathUnity attests the
// live Vector3 instances of MagnitudeSquared/Cross/Normalize/IsZero, fpu mirror confirms
// the free-function family).
//
// The console SDK implements these over AltiVec/VMX SIMD on a 16-byte `VectorIntrinsic mV`
// register (operator-/+ = vsubfp/vaddfp; Mult/Dot/Cross = vmaddfp/vnmsubfp + permutes;
// Magnitude/Normalize = vrsqrtefp + one or two Newton refinement steps; IsValid = per-lane
// vcmpeqfp self-equality NaN test). The PC reconstruction operates on the named float lanes
// of the flat Vector3 aggregate in types.h -- the SIMD machinery (VecFloat / VectorIntrinsic
// / VecFloatRef / Mask3) is NOT modelled here, so:
//   * functions the SDK returns as a broadcast VecFloat are de-modelled to scalar `float`
//     (Dot/Magnitude/MagnitudeSquared); call sites read a single lane.
//   * the rsqrt-estimate-plus-refinement reciprocal-magnitude is de-optimised to an exact
//     std::sqrt; numerically a touch tighter than the console's two-step estimate, never
//     a placeholder.
//   * tolerance arguments the SDK types as VecFloat are taken as plain `float`.
//
// DELIBERATELY OUT OF SCOPE (attested in the X360 binary under class:rw::math::vpu but NOT
// Vector3 operations, and with NO PC consumer -- see report; reconstructing them would
// require the un-modelled VecFloat / Matrix44-operation type machinery):
//   * rw::math::vpu::operator<            @0x821F0E20  -- scalar VecFloat < VecFloat (vcmpgtfp.)
//   * rw::math::vpu::GetVecFloat_Two      @0x821F0E38  -- VecFloat constant 2.0f (vspltisw/vcfsx)
//   * rw::math::vpu::QueryRotateDegenerateUnitAxis @0x82203768 -- rotation/axis helper (matrix domain)
//   * rw::math::vpu::Inverse              @0x825B2628  -- 4x4 Matrix44 inverse (matrix domain)
//
// gen-tool note: tools/renderware/generate_headers.py only writes rwcore_enums.h/rwcore_structs.h/
// rwcore.h under include/rw/; it never touches rw/math/vpu/, so this hand-maintained header
// (like its sibling types.h) is immune to regeneration.

#include <cmath>                  // std::sqrt for Magnitude/Length/Normalize
#include "rw/math/vpu/types.h"    // rw::math::vpu::Vector3 (Vector2/3/4 + matrices)

namespace rw
{
namespace math
{
namespace vpu
{
    // -- additive / subtractive --------------------------------------------------------

    // X360 0x82276868. Full 4-lane vsubfp (binary `vsubfp v1,v1,v2`, SDK Subtract over mV).
    // Consumed by BrnOfflineGameMode.cpp (lv3LandmarkPosition - lv3Origin) and
    // CgsMicrophone.cpp (lCurrent.maRows[3] - lPrevious.maRows[3]).
    inline Vector3 operator-(Vector3 lLhs, Vector3 lRhs)
    {
        return Vector3{ lLhs.x - lRhs.x, lLhs.y - lRhs.y, lLhs.z - lRhs.z, lLhs.w - lRhs.w };
    }

    // X360 0x825B2620. Full 4-lane vaddfp (binary `vaddfp v1,v1,v2`, SDK Add over mV).
    inline Vector3 operator+(Vector3 lLhs, Vector3 lRhs)
    {
        return Vector3{ lLhs.x + lRhs.x, lLhs.y + lRhs.y, lLhs.z + lRhs.z, lLhs.w + lRhs.w };
    }

    // SDK Add/Subtract free functions -- the named-call spellings of operator+/operator-.
    inline Vector3 Add(Vector3 lLhs, Vector3 lRhs)      { return lLhs + lRhs; }
    inline Vector3 Subtract(Vector3 lLhs, Vector3 lRhs) { return lLhs - lRhs; }

    // SDK Negate(v) = vxor with the sign-bit mask (per-lane unary minus over all 4 lanes).
    // Both the free function and the unary operator- the SDK declares are provided.
    inline Vector3 Negate(Vector3 lrVector)
    {
        return Vector3{ -lrVector.x, -lrVector.y, -lrVector.z, -lrVector.w };
    }
    inline Vector3 operator-(Vector3 lrVector) { return Negate(lrVector); }

    // -- multiplicative -----------------------------------------------------------------

    // SDK component-wise Mult(a,b) (vmaddfp a,b,0). Distinct from Dot (which sums lanes).
    inline Vector3 Mult(Vector3 lLhs, Vector3 lRhs)
    {
        return Vector3{ lLhs.x * lRhs.x, lLhs.y * lRhs.y, lLhs.z * lRhs.z, lLhs.w * lRhs.w };
    }

    // SDK MultAdd(a,b,c) = vmaddfp a,b,c (a*b + c, fused over all 4 lanes).
    inline Vector3 MultAdd(Vector3 lA, Vector3 lB, Vector3 lC)
    {
        return Vector3{ lA.x * lB.x + lC.x, lA.y * lB.y + lC.y,
                        lA.z * lB.z + lC.z, lA.w * lB.w + lC.w };
    }

    // Scalar broadcast Mult/operator* (SDK `operator*(Vector3, VecFloat)` = vmaddfp v,s,0).
    // The VecFloat scalar is de-modelled to a plain float broadcast over the lanes.
    inline Vector3 Mult(Vector3 lrVector, float lfScalar)
    {
        return Vector3{ lrVector.x * lfScalar, lrVector.y * lfScalar,
                        lrVector.z * lfScalar, lrVector.w * lfScalar };
    }
    inline Vector3 operator*(Vector3 lrVector, float lfScalar) { return Mult(lrVector, lfScalar); }
    inline Vector3 operator*(float lfScalar, Vector3 lrVector) { return Mult(lrVector, lfScalar); }

    // Scalar reciprocal Divide/operator/ (SDK `operator/(Vector3, VecFloat)` is a vrefp
    // estimate + two Newton steps; de-optimised to an exact lane-wise divide). The xyz/w
    // lanes are all divided to match the 4-lane SDK form. Consumed by CgsMicrophone.cpp
    // (mVelocity = (... - ...) / lfFrameTime).
    inline Vector3 Divide(Vector3 lrVector, float lfScalar)
    {
        return Vector3{ lrVector.x / lfScalar, lrVector.y / lfScalar,
                        lrVector.z / lfScalar, lrVector.w / lfScalar };
    }
    inline Vector3 operator/(Vector3 lrVector, float lfScalar) { return Divide(lrVector, lfScalar); }

    // -- products / magnitude -----------------------------------------------------------

    // SDK Dot returns a broadcast VecFloat; scalar call sites (BrnOfflineGameMode.cpp) read
    // one lane -> reconstructed as scalar float. xyz lanes only (w excluded).
    inline float Dot(Vector3 lLhs, Vector3 lRhs)
    {
        return lLhs.x * lRhs.x + lLhs.y * lRhs.y + lLhs.z * lRhs.z;
    }

    // SDK Cross(a,b) -- the permute/vnmsubfp cross product. Standard xyz cross; the w lane
    // is cleared (the SDK leaves it as the permute residue, semantically unused). DWARF
    // BrnMathUnity attests rw::math::vpu::Cross over Vector3.
    inline Vector3 Cross(Vector3 lLhs, Vector3 lRhs)
    {
        return Vector3{
            lLhs.y * lRhs.z - lLhs.z * lRhs.y,
            lLhs.z * lRhs.x - lLhs.x * lRhs.z,
            lLhs.x * lRhs.y - lLhs.y * lRhs.x,
            0.0f };
    }

    // SDK MagnitudeSquared returns a broadcast VecFloat; scalar here, xyz lanes only.
    // DWARF BrnMathUnity attests rw::math::vpu::MagnitudeSquared over Vector3 (many sites).
    inline float MagnitudeSquared(Vector3 lrVector)
    {
        return lrVector.x * lrVector.x + lrVector.y * lrVector.y + lrVector.z * lrVector.z;
    }

    // SDK free function is Magnitude; reconstructed game source spells it Length. Both
    // provided. De-optimised from the X360 rsqrt-refinement to a plain std::sqrt over xyz.
    // Consumed via lfDistance = Length(lv3Delta).
    inline float Magnitude(Vector3 lrVector)
    {
        return std::sqrt(MagnitudeSquared(lrVector));
    }
    inline float Length(Vector3 lrVector) { return Magnitude(lrVector); }

    // SDK Normalize(v) (rsqrt estimate + two refinement steps, then vmaddfp v * 1/|v|).
    // De-optimised to an exact 1/std::sqrt scale. Degenerate (zero-length) input returns the
    // zero vector, matching the SDK's vsel-against-zero guard. DWARF BrnMathUnity attests
    // rw::math::vpu::Normalize over Vector3; consumed by CgsMicrophone.cpp
    // (mDirection = Normalize(lCurrent.maRows[2])).
    inline Vector3 Normalize(Vector3 lrVector)
    {
        const float lfMagnitudeSquared = MagnitudeSquared(lrVector);
        if (lfMagnitudeSquared <= 0.0f)
            return Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };

        const float lfInverseMagnitude = 1.0f / std::sqrt(lfMagnitudeSquared);
        return Vector3{ lrVector.x * lfInverseMagnitude, lrVector.y * lfInverseMagnitude,
                        lrVector.z * lfInverseMagnitude, lrVector.w * lfInverseMagnitude };
    }

    // ADDITIVE GROW (consumed by BrnLooker.cpp): the SDK's NormalizeFast(v) -- on console a
    // single vrsqrtefp estimate (no Newton refinement) scaled into the vector. The estimate
    // error is irrelevant to the PC parity build, so it reduces to the exact Normalize above.
    inline Vector3 NormalizeFast(Vector3 lrVector) { return Normalize(lrVector); }

    // ADDITIVE GROW (consumed by BrnLooker.cpp): the SDK's SqrtFast(s) -- a vrsqrtefp-based
    // reciprocal-sqrt estimate refined to sqrt. Modelled as the exact scalar std::sqrt
    // (broadcast across the lanes by the VecFloat alias); negative/zero inputs return 0,
    // matching the console vsel-against-zero guard.
    inline Vector4 SqrtFast(Vector4 lvValue)
    {
        const float lfResult = (lvValue.x > 0.0f) ? std::sqrt(lvValue.x) : 0.0f;
        return Vector4{ lfResult, lfResult, lfResult, lfResult };
    }

    // -- lane accessors -----------------------------------------------------------------

    // SDK Vector3::GetX/Y/Z (vector3_type_inline.h) return VecFloatRef lane refs; modelled
    // as free accessors over the named lanes because the committed PC Vector3 is a flat
    // float-lane aggregate (no VecFloatRef machinery). GetY consumed by BrnOfflineGameMode.cpp.
    inline float GetX(const Vector3& lrVector) { return lrVector.x; }
    inline float GetY(const Vector3& lrVector) { return lrVector.y; }
    inline float GetZ(const Vector3& lrVector) { return lrVector.z; }

    // -- clamping / extrema --------------------------------------------------------------

    // SDK Min/Max/Clamp = vminfp / vmaxfp / vmaxfp-then-vminfp, lane-wise over all 4 lanes.
    inline Vector3 Min(Vector3 lLhs, Vector3 lRhs)
    {
        return Vector3{
            lLhs.x < lRhs.x ? lLhs.x : lRhs.x,
            lLhs.y < lRhs.y ? lLhs.y : lRhs.y,
            lLhs.z < lRhs.z ? lLhs.z : lRhs.z,
            lLhs.w < lRhs.w ? lLhs.w : lRhs.w };
    }
    inline Vector3 Max(Vector3 lLhs, Vector3 lRhs)
    {
        return Vector3{
            lLhs.x > lRhs.x ? lLhs.x : lRhs.x,
            lLhs.y > lRhs.y ? lLhs.y : lRhs.y,
            lLhs.z > lRhs.z ? lLhs.z : lRhs.z,
            lLhs.w > lRhs.w ? lLhs.w : lRhs.w };
    }
    inline Vector3 Clamp(Vector3 lrValue, Vector3 lrMin, Vector3 lrMax)
    {
        return Min(Max(lrValue, lrMin), lrMax);
    }

    // SDK Abs(v) = vandc against the sign-bit mask, lane-wise over all 4 lanes.
    inline Vector3 Abs(Vector3 lrVector)
    {
        return Vector3{ std::fabs(lrVector.x), std::fabs(lrVector.y),
                        std::fabs(lrVector.z), std::fabs(lrVector.w) };
    }

    // -- interpolation -------------------------------------------------------------------

    // SDK Lerp(v0,v1,t) = vmaddfp((v1 - v0), t, v0). The VecFloat t is de-modelled to a
    // plain float; xyz lanes interpolated (w follows the same lane arithmetic).
    inline Vector3 Lerp(Vector3 lV0, Vector3 lV1, float lfT)
    {
        return Vector3{
            lV0.x + (lV1.x - lV0.x) * lfT,
            lV0.y + (lV1.y - lV0.y) * lfT,
            lV0.z + (lV1.z - lV0.z) * lfT,
            lV0.w + (lV1.w - lV0.w) * lfT };
    }

    // -- predicates ----------------------------------------------------------------------

    // SDK operator==/!= = _asmIsEqualV3 (per-lane exact compare over xyz). Reconstructed as
    // exact lane equality; the w lane is excluded to match the V3 (three-lane) comparison.
    inline bool operator==(const Vector3& lLhs, const Vector3& lRhs)
    {
        return lLhs.x == lRhs.x && lLhs.y == lRhs.y && lLhs.z == lRhs.z;
    }
    inline bool operator!=(const Vector3& lLhs, const Vector3& lRhs) { return !(lLhs == lRhs); }

    // SDK IsZero(v, tolerance) = _asmIsZeroV3 (|lane| <= tolerance over xyz). VecFloat
    // tolerance de-modelled to a plain float; defaults to the SDK EPSILON spirit (small).
    // DWARF BrnMathUnity attests rw::math::vpu::IsZero adjacent to the Vector3 family.
    inline bool IsZero(const Vector3& lrVector, float lfTolerance = 1.0e-6f)
    {
        return std::fabs(lrVector.x) <= lfTolerance
            && std::fabs(lrVector.y) <= lfTolerance
            && std::fabs(lrVector.z) <= lfTolerance;
    }

    // SDK IsAnyNaN(v) = vcmpeqfp_p self-compare predicate, negated (a lane is NaN iff it is
    // not equal to itself). xyz lanes.
    inline bool IsAnyNaN(const Vector3& lrVector)
    {
        return !(lrVector.x == lrVector.x)
            || !(lrVector.y == lrVector.y)
            || !(lrVector.z == lrVector.z);
    }

    // Canonical home is rw::math::vpu (DWARF: rw::math::vpu::IsValid resolved 8x in
    // BrnTrafficLightCollection.cpp; SDK defines IsValid(Vector3) = IsValid(X)&&IsValid(Y)&&
    // IsValid(Z), each lane's scalar IsValid being the vcmpeqfp self-equality NaN test).
    // Reconstructed as per-lane x==x self-comparison (the negation of IsAnyNaN over xyz).
    // NOT RwMath:: -- that string is only the verbatim baked assert literal preserved at
    // the BrnTrafficLightTrigger.cpp call sites.
    inline bool IsValid(const Vector3& lrVector)
    {
        return lrVector.x == lrVector.x
            && lrVector.y == lrVector.y
            && lrVector.z == lrVector.z;
    }
}
}
}

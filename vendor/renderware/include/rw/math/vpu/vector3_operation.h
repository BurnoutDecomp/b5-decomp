#pragma once

// Shared include spelling for the genuine SDK VPU Vector3 operations.
// types.h imports the one SDK class and its recovered native operation bodies.
// Remaining source convenience overloads preserve existing callers; legacy scalar
// and Vector4-as-scalar APIs here remain distinct from the genuine SDK VecFloat class.

#include <vectorintrinsics.h>
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


    // X360 0x825B2620. Full 4-lane vaddfp (binary `vaddfp v1,v1,v2`, SDK Add over mV).


    // SDK Add/Subtract free functions -- the named-call spellings of operator+/operator-.



    // SDK Negate(v) = vxor with the sign-bit mask (per-lane unary minus over all 4 lanes).
    // Both the free function and the unary operator- the SDK declares are provided.



    // -- multiplicative -----------------------------------------------------------------

    // SDK component-wise Mult(a,b) (vmaddfp a,b,0). Distinct from Dot (which sums lanes).


    // SDK MultAdd(a,b,c) = vmaddfp a,b,c (a*b + c, fused over all 4 lanes).


    // Scalar broadcast Mult/operator* (SDK `operator*(Vector3, VecFloat)` = vmaddfp v,s,0).
    // Vector4 is the portable VecFloat storage type (BrnCommonTypes.h); retain a float overload
    // as a convenience for older call sites that de-modelled the broadcast scalar.
    inline Vector3 Mult(Vector3 lrVector, Vector4 lvfScalar)
    {
        { const VectorIntrinsic lResult = { lrVector.mV.x * lvfScalar.mV.x, lrVector.mV.y * lvfScalar.mV.y,
                        lrVector.mV.z * lvfScalar.mV.z, lrVector.mV.w * lvfScalar.mV.w }; return Vector3(lResult); }
    }
    inline Vector3 operator*(Vector3 lrVector, Vector4 lvfScalar) { return Mult(lrVector, lvfScalar); }
    inline Vector3 operator*(Vector4 lvfScalar, Vector3 lrVector) { return Mult(lrVector, lvfScalar); }





    // Scalar reciprocal Divide/operator/ (SDK `operator/(Vector3, VecFloat)` is a vrefp
    // estimate + two Newton steps; de-optimised to an exact lane-wise divide). The xyz/w
    // lanes are all divided to match the 4-lane SDK form. Consumed by CgsMicrophone.cpp
    // (mVelocity = (... - ...) / lfFrameTime).



    // -- products / magnitude -----------------------------------------------------------

    // SDK Dot returns a broadcast VecFloat; scalar call sites (BrnOfflineGameMode.cpp) read
    // one lane -> reconstructed as scalar float. xyz lanes only (w excluded).


    // SDK Cross(a,b) -- the permute/vnmsubfp cross product. Standard xyz cross; the w lane
    // is cleared (the SDK leaves it as the permute residue, semantically unused). DWARF
    // BrnMathUnity attests rw::math::vpu::Cross over Vector3.


    // SDK MagnitudeSquared returns a broadcast VecFloat; scalar here, xyz lanes only.
    // DWARF BrnMathUnity attests rw::math::vpu::MagnitudeSquared over Vector3 (many sites).


    // SDK free function is Magnitude; reconstructed game source spells it Length. Both
    // provided. De-optimised from the X360 rsqrt-refinement to a plain std::sqrt over xyz.
    // Consumed via lfDistance = Length(lv3Delta).

    inline float Length(Vector3 lrVector) { return Magnitude(lrVector); }

    // SDK Normalize(v) (rsqrt estimate + two refinement steps, then vmaddfp v * 1/|v|).
    // De-optimised to an exact 1/std::sqrt scale. Degenerate (zero-length) input returns the
    // zero vector, matching the SDK's vsel-against-zero guard. DWARF BrnMathUnity attests
    // rw::math::vpu::Normalize over Vector3; consumed by CgsMicrophone.cpp
    // (mDirection = Normalize(lCurrent.maRows[2])).


    // ADDITIVE GROW (consumed by BrnLooker.cpp): the SDK's NormalizeFast(v) -- on console a
    // single vrsqrtefp estimate (no Newton refinement) scaled into the vector. The estimate
    // error is irrelevant to the PC parity build, so it reduces to the exact Normalize above.


    // ADDITIVE GROW 2026-08-02 (consumed by BehaviourGameplayExternal::Update .cpp:207):
    // the SDK's NormalizeReturnMagnitude(v, &out) -- ONE rsqrt pipeline that publishes BOTH
    // the unit vector (through the out-parameter) and the magnitude (as the return value).
    // DecFIGS names it -- detail/vector3_operation_inline.h:207 -- and the X360 twin proves
    // the double publish: @0x82240924 `vmulfp128 v0, v0, v13` is |v|^2 * rsqrt (the LENGTH)
    // while @0x82240928 `vmulfp128 v127, v12, v13` is v * rsqrt (the DIRECTION), both off the
    // same refined estimate, and the zero-length `vsel128` guard at 0x8224092C substitutes
    // ZERO for the magnitude. Reproduced here as the exact scalar form, exactly as the
    // Normalize above already is.
    // ⚠️ ON A ZERO-LENGTH INPUT the console publishes a zero magnitude AND a zero direction
    //   (both fall out of the same vsel), so the guard is reproduced on both outputs.


    // ADDITIVE GROW (consumed by BrnLooker.cpp): the SDK's SqrtFast(s) -- a vrsqrtefp-based
    // reciprocal-sqrt estimate refined to sqrt. Modelled as the exact scalar std::sqrt
    // (broadcast across the lanes by the VecFloat alias); negative/zero inputs return 0,
    // matching the console vsel-against-zero guard.
    inline Vector4 SqrtFast(Vector4 lvValue)
    {
        const float lfResult = (lvValue.mV.x > 0.0f) ? std::sqrt(lvValue.mV.x) : 0.0f;
        { const VectorIntrinsic lResult = { lfResult, lfResult, lfResult, lfResult }; return Vector4(lResult); }
    }

    // -- lane accessors -----------------------------------------------------------------

    // SDK Vector3::GetX/Y/Z (vector3_type_inline.h) return VecFloatRef lane refs; modelled
    // as free accessors over the named lanes because the committed PC Vector3 is a flat
    // float-lane aggregate (no VecFloatRef machinery). GetY consumed by BrnOfflineGameMode.cpp.
    inline float GetX(const Vector3& lrVector) { return lrVector.mV.x; }
    inline float GetY(const Vector3& lrVector) { return lrVector.mV.y; }
    inline float GetZ(const Vector3& lrVector) { return lrVector.mV.z; }

    // -- clamping / extrema --------------------------------------------------------------

    // SDK Min/Max/Clamp = vminfp / vmaxfp / vmaxfp-then-vminfp, lane-wise over all 4 lanes.




    // SDK Abs(v) = vandc against the sign-bit mask, lane-wise over all 4 lanes.


    // -- interpolation -------------------------------------------------------------------

    // SDK Lerp(v0,v1,t) = vmaddfp((v1 - v0), t, v0). The VecFloat t is de-modelled to a
    // plain float; xyz lanes interpolated (w follows the same lane arithmetic).
    inline Vector3 Lerp(Vector3 lV0, Vector3 lV1, float lfT)
    {
        { const VectorIntrinsic lResult = {
            lV0.mV.x + (lV1.mV.x - lV0.mV.x) * lfT,
            lV0.mV.y + (lV1.mV.y - lV0.mV.y) * lfT,
            lV0.mV.z + (lV1.mV.z - lV0.mV.z) * lfT,
            lV0.mV.w + (lV1.mV.w - lV0.mV.w) * lfT }; return Vector3(lResult); }
    }

    // -- predicates ----------------------------------------------------------------------

    // SDK operator==/!= = _asmIsEqualV3 (per-lane exact compare over xyz). Reconstructed as
    // exact lane equality; the w lane is excluded to match the V3 (three-lane) comparison.



    // SDK IsZero(v, tolerance) = _asmIsZeroV3 (|lane| <= tolerance over xyz). VecFloat
    // tolerance de-modelled to a plain float; defaults to the SDK EPSILON spirit (small).
    // DWARF BrnMathUnity attests rw::math::vpu::IsZero adjacent to the Vector3 family.
    inline bool IsZero(const Vector3& lrVector, float lfTolerance = 1.0e-6f)
    {
        return std::fabs(lrVector.mV.x) <= lfTolerance
            && std::fabs(lrVector.mV.y) <= lfTolerance
            && std::fabs(lrVector.mV.z) <= lfTolerance;
    }

    // SDK IsAnyNaN(v) = vcmpeqfp_p self-compare predicate, negated (a lane is NaN iff it is
    // not equal to itself). xyz lanes.


    // Canonical home is rw::math::vpu (DWARF: rw::math::vpu::IsValid resolved 8x in
    // BrnTrafficLightCollection.cpp; SDK defines IsValid(Vector3) = IsValid(X)&&IsValid(Y)&&
    // IsValid(Z), each lane's scalar IsValid being the vcmpeqfp self-equality NaN test).
    // Reconstructed as per-lane x==x self-comparison (the negation of IsAnyNaN over xyz).
    // NOT RwMath:: -- that string is only the verbatim baked assert literal preserved at
    // the BrnTrafficLightTrigger.cpp call sites.


    // ===================================================================================
    // ADDITIVE GROW 2026-08-19 (wave Q6 / the jointed lean+tilt prop response). The five
    // entries below are the vocabulary BrnPhysics::Props::PropManager::HandleContactWith
    // Lean/TiltProp (X360 0x8260FB60 / 0x826108B8) consumes and the tree had no home for.
    //
    // ⭐ EVERY SIGNATURE HERE IS SDK-EXACT, not inferred. Two independent sources agree:
    //   * the Feb-2007 leak's own public header, references/Feb-2007/BrnEntityModuleUnity/
    //     EARenderWare/stable/external/rwmath/1.02.00/include/rw/math/vpu/vector3_operation.h
    //     :121/:122/:127 and mask3.h -- the declarations are copied from there;
    //   * the DecFIGS DWARF, which names the same calls in this exact function
    //     (references/DecFIGS/dwarfdump/GameSource/Physics/PropManager/BrnPropManager.cpp
    //     :1500 GetVector3_YAxis, :1502 CompLessThan, :1545 Select, :1547 CompGreaterThan,
    //     :1796 GetVector3_ZAxis) and dumps the SDK bodies at
    //     dwarfdump/SDKs/EATech/include/rw/math/vpu/detail/vector3_operation_inline.h:1166/
    //     :1169/:1883 and .../mask3_type_inline.h:71/:78/:85.
    //
    // ⚠️ THE SDK'S `Select` TAKES THE MASK **FIRST**, then the TRUE value, then the FALSE
    //    value -- `Select(mask, trueValue, falseValue)`, lowering to
    //    `vec_sel(falseValue, trueValue, mask)` (Feb-2007 vector3_operation_inline.h:738-743).
    //    The Vector4 `Select` already committed in the sibling vector4_operation.h has the
    //    OPPOSITE order (`Select(false, true, mask)`); that is a pre-existing divergence,
    //    flagged at its own site, and NOT copied here. Getting either one wrong is a COMPILE
    //    error, never a silent behaviour change, because MaskScalar and Vector3/Vector4 are
    //    distinct structs with no conversion between them.
    // ===================================================================================

    // The SDK's THREE-LANE comparison-result wrapper -- what a VMX vcmpgefp/vcmpgtfp leaves
    // in a vector register when the operands are Vector3s: four lanes of 0x00000000 (false)
    // or 0xFFFFFFFF (true), of which only x/y/z carry meaning. Same 16-byte, 16-aligned
    // register layout as MaskScalar (vpu/types.h), and stored the same way -- four named
    // float lanes where "true" is any non-zero lane -- so the compare semantics reduce
    // exactly and Mask3 stays an aggregate.
    //
    // ⚠️ CANONICAL SDK HOME is rw/math/vpu/mask3.h, which does not exist in this tree; it
    //    lives here, beside its only producers, on exactly the footing vector4_operation.h
    //    already records for IsValid(Quaternion) ("until that header exists"). Move it to a
    //    real mask3.h the day one lands. The SDK class also carries SetX/SetY/SetZ, the
    //    (bool,bool,bool) / (uint32_t x3) / (MaskScalar x3) constructors and the free
    //    reduction `MaskScalar CompAllTrue(Mask3)` (mask3_operation_inline.h:73); none of
    //    those is consumed by reconstructed game code yet, so none is reproduced -- this is
    //    the operation vocabulary the tree actually uses, not a full SDK port.
    struct CGS_ALIGN(16) Mask3
    {
        float x, y, z, w;

        // Feb-2007 mask3_type_inline.h:71/:78/:85 -- each is one `vspltw mV, <lane>`, i.e.
        // the lane broadcast into all four lanes of a MaskScalar. HandleContactWithLeanProp
        // inlines exactly these six broadcasts at 0x82610018/0x8261001C/0x82610020 (the
        // less-than mask) and 0x82610028/0x8261002C/0x82610034 (the greater-than mask).
        MaskScalar GetX() const { return MaskScalar(__vspltw(__lvx(this, 0), 0)); }
        MaskScalar GetY() const { return MaskScalar(__vspltw(__lvx(this, 0), 1)); }
        MaskScalar GetZ() const { return MaskScalar(__vspltw(__lvx(this, 0), 2)); }
    };

    // CompLessThan(a, b) -- Feb-2007 vector3_operation_inline.h:693-699. ⚠️ THE SDK DOES NOT
    // EMIT A "LESS THAN" COMPARE. It emits `vcmpgefp(a, b)` and then `vnor` -- the NEGATED
    // greater-or-equal. That is AGENTS.md gotcha 4 in its purest form: on a NaN lane
    // `a >= b` is false, so the negation makes the lane TRUE, whereas a host `a < b` would
    // make it FALSE. Written below as the literal `!(a >= b)` so the NaN polarity is
    // reproduced rather than paraphrased away. MEASURED in the consumer too:
    // `vcmpgefp128 v12, v127, v13` @0x8260FFF0 followed by `vnot v12, v12` @0x8261000C.
    inline Mask3 CompLessThan(Vector3 lA, Vector3 lB)
    {
        const Mask3 lResult = {
            !(lA.mV.x >= lB.mV.x) ? 1.0f : 0.0f,
            !(lA.mV.y >= lB.mV.y) ? 1.0f : 0.0f,
            !(lA.mV.z >= lB.mV.z) ? 1.0f : 0.0f,
            !(lA.mV.w >= lB.mV.w) ? 1.0f : 0.0f };
        return lResult;
    }

    // CompGreaterThan(a, b) -- Feb-2007 vector3_operation_inline.h:701-706: a bare
    // `vcmpgtfp(a, b)` with no negation, so a NaN lane comes out FALSE (the plain C++ `>`
    // polarity). The two functions are deliberately NOT symmetric; see CompLessThan.
    // MEASURED in the consumer: `vcmpgtfp128 v13, v127, v0` @0x82610008.
    inline Mask3 CompGreaterThan(Vector3 lA, Vector3 lB)
    {
        const Mask3 lResult = {
            lA.mV.x > lB.mV.x ? 1.0f : 0.0f,
            lA.mV.y > lB.mV.y ? 1.0f : 0.0f,
            lA.mV.z > lB.mV.z ? 1.0f : 0.0f,
            lA.mV.w > lB.mV.w ? 1.0f : 0.0f };
        return lResult;
    }

    // Select(mask, trueValue, falseValue) -- Feb-2007 vector3_operation_inline.h:738-743,
    // `vec_sel(falseValue, trueValue, mask)`: per lane, a non-zero mask lane picks
    // trueValue. MEASURED at the consumer: `vsel128 v125, v0, v119, v125` @0x8261005C with
    // vA = the normalised rotation axis (the FALSE value), vB = the car's xAxis (the TRUE
    // value) and vC = the degenerate-axis mask -- PPC `vsel vD,vA,vB,vC` is `vC ? vB : vA`.
    // The SDK also declares a Mask3-masked overload (:745-750); it has no consumer in this
    // tree yet and is not reproduced.
    inline Vector3 Select(MaskScalar lMask, Vector3 lTrueValue, Vector3 lFalseValue)
    {
        { const VectorIntrinsic lResult = {
            lMask.mV.x != 0.0f ? lTrueValue.mV.x : lFalseValue.mV.x,
            lMask.mV.y != 0.0f ? lTrueValue.mV.y : lFalseValue.mV.y,
            lMask.mV.z != 0.0f ? lTrueValue.mV.z : lFalseValue.mV.z,
            lMask.mV.w != 0.0f ? lTrueValue.mV.w : lFalseValue.mV.w }; return Vector3(lResult); }
    }

    // The three world basis vectors -- Feb-2007 constants_operation.h:42-44, whose bodies
    // (detail/constants_operation_inline.h:111-124) just return the SDK's three rodata
    // registers `detail::gIVector` / `gJVector` / `gKVector`. Those three sit contiguously
    // in the shipped X360 image at 0x82181500 / 0x82181510 / 0x82181520 -- the existing
    // banner at the head of this file already names gIVector @0x82181500 -- and
    // HandleContactWithLeanProp loads 0x82181510 (`addi r28, r11, unk_82181510` @0x8260FC48,
    // consumed by the `vmsum3fp128 v0, v118, v0` up-axis dot at 0x8260FC74 and by the
    // Cross at 0x8260FEC0) while HandleContactWithTiltProp loads 0x82181520
    // (`addi r10, r10, unk_82181520` @0x82610A94 -> `vmsum3fp128 v12, v126, v12`).
    // PropCollisions.cpp:95-96 independently attests 0x82181510 == GetVector3_YAxis().
    //
    // ⚠️ CANONICAL SDK HOME is rw/math/vpu/constants_operation.h, absent from this tree;
    //    same "until that header exists" footing as Mask3 above.
    inline Vector3 GetVector3_XAxis() { { const VectorIntrinsic lResult = { 1.0f, 0.0f, 0.0f, 0.0f }; return Vector3(lResult); } }
    inline Vector3 GetVector3_YAxis() { { const VectorIntrinsic lResult = { 0.0f, 1.0f, 0.0f, 0.0f }; return Vector3(lResult); } }
    inline Vector3 GetVector3_ZAxis() { { const VectorIntrinsic lResult = { 0.0f, 0.0f, 1.0f, 0.0f }; return Vector3(lResult); } }
}
}
}

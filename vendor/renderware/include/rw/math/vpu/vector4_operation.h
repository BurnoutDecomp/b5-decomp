#pragma once

// Shared include spelling for the genuine SDK VPU Vector4 operations.
// types.h imports the one SDK class and its native primitive bodies.
// Existing scalar/mask convenience APIs below retain their declarations; the old
// global VecFloat alias and separate SDK VecFloat remain an explicit identity debt.

#include "rw/math/vpu/types.h"   // Vector4, MaskScalar

#include <vectorintrinsics.h>
#include <cmath>                 // std::sqrt (Magnitude)

namespace rw
{
namespace math
{
namespace vpu
{
    // -- arithmetic (lane-wise; vaddfp / vsubfp / vmulfp) --------------------------------


    // Per-lane product (vmulfp128). The broadcast-scalar (VecFloat) case falls out of this:
    // a scalar held in every lane multiplies each lane of the other operand.


    // -- the all-ones vector (vspltisw v,1 ; vcfsx v,v,0 -> 1.0 per lane) -----------------
    inline Vector4 GetVector4_One() { { const VectorIntrinsic lResult = { 1.0f, 1.0f, 1.0f, 1.0f }; return Vector4(lResult); } }

    // Broadcast one scalar into all four lanes (the VecFloat construction the SDK spells as
    // `VecFloat(float)` / a vspltw of a loaded scalar). A VecFloat *is* such a Vector4.
    inline Vector4 Splat(float lfScalar) { { const VectorIntrinsic lResult = { lfScalar, lfScalar, lfScalar, lfScalar }; return Vector4(lResult); } }

    // -- extrema / clamp (vminfp / vmaxfp; Clamp = max-then-min) --------------------------




    // -- lane accessor (Vector4::operator[]) ---------------------------------------------
    inline float GetComponent(const Vector4& lrVector, int liLane)
    {
        return (&lrVector.mV.x)[liLane];
    }

    // -- mask ops ------------------------------------------------------------------------
    // And(a,b) = vand, lane-wise over the 0x00000000 / 0xFFFFFFFF compare masks.
    __forceinline MaskScalar And(MaskScalar lLhs, MaskScalar lRhs)
    {
        MaskScalar lResult;
        __stvx(__vand(__lvx(&lLhs, 0), __lvx(&lRhs, 0)), &lResult, 0);
        return lResult;
    }

    // Select(falseVal, trueVal, mask) = vsel: per-lane mask ? trueVal : falseVal.
    //
    // ⚠️⚠️ FLAG (argument order, 2026-08-19 / wave Q6) -- NOT a behaviour bug today, but a
    // real divergence from the SDK that the next grow of this family must not copy. The
    // shipped rwmath 1.02.00 public header spells this
    //     VecFloat Select(MaskScalar::InParam mask, VecFloat::InParam trueValue,
    //                     VecFloat::InParam falseValue);
    // (references/Feb-2007/.../rwmath/1.02.00/include/rw/math/vpu/scalar.h:140, bodied at
    // detail/scalar_operation_inline.h as `vec_sel(falseValue, trueValue, mask)`), i.e. the
    // MASK COMES FIRST. The committed signature below reverses that. It has exactly ONE
    // caller -- BrnTrafficFuzzyLogicBehaviours.cpp:277-280, which passes the arguments in
    // this file's order and even labels them `/*false:*/` / `/*true :*/` -- so the two are
    // self-consistent and nothing is mis-selecting today.
    //   HOW TO RETIRE (a conductor-sized change, NOT this owner's file): reorder to
    //   `Select(MaskScalar lMask, Vector4 lTrueValue, Vector4 lFalseValue)` and update that
    //   one call site in the same commit. Any site that is missed becomes a COMPILE error
    //   (MaskScalar and Vector4 are distinct structs with no conversion), never a silent
    //   flip. The Vector3 sibling landed in vector3_operation.h on 2026-08-19 deliberately
    //   uses the SDK order, so the two spellings differ until this is done.
    inline Vector4 Select(Vector4 lFalse, Vector4 lTrue, MaskScalar lMask)
    {
        { const VectorIntrinsic lResult = {
            lMask.x != 0.0f ? lTrue.mV.x : lFalse.mV.x,
            lMask.y != 0.0f ? lTrue.mV.y : lFalse.mV.y,
            lMask.z != 0.0f ? lTrue.mV.z : lFalse.mV.z,
            lMask.w != 0.0f ? lTrue.mV.w : lFalse.mV.w }; return Vector4(lResult); }
    }

    // ===================================================================================
    // ADDITIVE GROW 2026-08-19 (wave Q6 / the jointed lean+tilt prop response): the two
    // VecFloat compare-to-mask primitives, under their SDK names.
    //
    // The shipped rwmath public header declares them at
    // references/Feb-2007/.../rwmath/1.02.00/include/rw/math/vpu/scalar.h:134-135 as
    //     MaskScalar CompLessThan(VecFloat::InParam a, VecFloat::InParam b);
    //     MaskScalar CompGreaterThan(VecFloat::InParam a, VecFloat::InParam b);
    // and bodies them at detail/scalar_operation_inline.h:242-255. VecFloat is Vector4 in
    // this tree (BrnCommonTypes.h:23), which is why they are typed Vector4 here. The DecFIGS
    // DWARF names both in the consumers (BrnPropManager.cpp:1502 / :1547 / :1791).
    //
    // ⚠️ THE TWO ARE NOT SYMMETRIC, and this is AGENTS.md gotcha 4:
    //     CompLessThan    = vcmpgefp + vnor  -> the NEGATED >=, so a NaN lane is TRUE
    //     CompGreaterThan = vcmpgtfp (bare)  -> a NaN lane is FALSE
    // Spelled below as the literal `!(a >= b)` and `a > b` so the polarity is reproduced,
    // not paraphrased. MEASURED in HandleContactWithLeanProp: `vcmpgefp v0, v0, v11`
    // @0x8260FC9C followed by `vnot128 v114, v0` @0x8260FCAC (the lean-limit test), against
    // HandleContactWithTiltProp's bare `vcmpgtfp128 v126, v12, v127` @0x82610ACC (the
    // "normal points along +Z" test).
    // ===================================================================================
    inline MaskScalar CompLessThan(Vector4 lA, Vector4 lB)
    {
        { const MaskScalar lResult = {
            !(lA.mV.x >= lB.mV.x) ? 1.0f : 0.0f,
            !(lA.mV.y >= lB.mV.y) ? 1.0f : 0.0f,
            !(lA.mV.z >= lB.mV.z) ? 1.0f : 0.0f,
            !(lA.mV.w >= lB.mV.w) ? 1.0f : 0.0f }; return lResult; }
    }

    inline MaskScalar CompGreaterThan(Vector4 lA, Vector4 lB)
    {
        { const MaskScalar lResult = {
            lA.mV.x > lB.mV.x ? 1.0f : 0.0f,
            lA.mV.y > lB.mV.y ? 1.0f : 0.0f,
            lA.mV.z > lB.mV.z ? 1.0f : 0.0f,
            lA.mV.w > lB.mV.w ? 1.0f : 0.0f }; return lResult; }
    }

    // Greater-than compare producing a per-lane mask (vcmpgtfp): 1.0 where lLhs>lRhs.
    // ⚠️ `IsGreater` IS NOT AN SDK NAME -- it was this tree's stand-in for CompGreaterThan
    // before that one had a home (the census in PropManager_wQ2_01.cpp:495-496 says so in
    // as many words: "CompGreaterThan does NOT exist under that name; the tree's home for
    // it is IsGreater"). Now that the SDK spelling is committed above, this is kept ONLY as
    // a forwarder so its two existing callers (BrnTrafficFuzzyLogicBehaviours.cpp:273/:274)
    // keep compiling -- it is deliberately NOT a second copy of the logic. New code must
    // call CompGreaterThan. DELETE-WHEN those two call sites are re-spelled.
    inline MaskScalar IsGreater(Vector4 lLhs, Vector4 lRhs)
    {
        return CompGreaterThan(lLhs, lRhs);
    }

    // ADDITIVE GROW 2026-08-27 (stunt-race UI wave): the full-4 Magnitude, under its SDK
    // name. Shipped rwmath bodies it at the EATech reference copy
    // (src/SDKs/EATech/include/rw/math/vpu/detail/vector4_operation_inline.h:273) as the
    // 4-lane dot (vmsum4fp128) -> vrsqrtefp + Newton refinement, with a vsel that forces the
    // result to 0 when the dot is exactly 0 -- i.e. sqrt(dot) with sqrt(0) == 0. Scalar
    // lowering here per this header's convention (the Vector3 sibling does the same); the
    // zero-dot guard falls out of std::sqrt(0.0f) == 0.0f. First consumer: the
    // MainMapComponent::Construct world-rect sanity assert @0x8245E514.
    inline float Magnitude(const Vector4& lrVector)
    {
        const float lfDot = lrVector.mV.x * lrVector.mV.x + lrVector.mV.y * lrVector.mV.y +
                            lrVector.mV.z * lrVector.mV.z + lrVector.mV.w * lrVector.mV.w;
        return std::sqrt(lfDot);
    }

    // ADDITIVE GROW 2026-08-04 (task #144): the FOUR-lane sibling of the three-lane
    // `IsValid(const Vector3&)` in vector3_operation.h -- the finite-value ("RwMathVPU::IsValid")
    // tripwire the console bakes into its asserts, here over all four lanes of a Quaternion.
    //
    // X360-ATTESTED SHAPE, and the lane COUNT is what types the argument.
    // PhysicsSimulationModule::ProcessAddJointQueue @0x828A40F0 inlines this check eleven times
    // and the widths separate cleanly: FOUR `vspltw`+`vcmpeqfp.` pairs (lanes 0,1,2,3) for
    // JointFrames::GetChildAngularFrame/GetParentAngularFrame/GetParentLinearFrame -- the three
    // quaternions -- against THREE for every Vector3 accessor and ONE for each f32. Each lane is
    // the self-equality NaN test `x == x`, identical to the Vector3 body.
    //
    // ⚠️ CANONICAL HOME is rw/math/vpu/quaternion_operation.h, which does not exist in this tree
    // yet; it lives here, in the 4-lane operation header, on the same "until that header exists"
    // footing JointFrames.hpp already records for QuaternionFromMatrix33.
    inline bool IsValid(const Quaternion& lrQuaternion)
    {
        return lrQuaternion.x == lrQuaternion.x
            && lrQuaternion.y == lrQuaternion.y
            && lrQuaternion.z == lrQuaternion.z
            && lrQuaternion.w == lrQuaternion.w;
    }
}
}
}

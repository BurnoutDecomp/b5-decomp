#pragma once

// Shared include spelling for the genuine SDK VPU Vector4 operations.
// types.h imports the one SDK class and its native primitive bodies.
// Scalar comparison and mask primitives use their genuine SDK declarations.
// Remaining legacy convenience functions below require their separate recovery.

#include "rw/math/vpu/types.h"   // Vector4, MaskScalar
#include "SDKs/EATech/include/rw/math/vpu/scalar.h"

#include <vectorintrinsics.h>

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
            lMask.mV.x != 0.0f ? lTrue.mV.x : lFalse.mV.x,
            lMask.mV.y != 0.0f ? lTrue.mV.y : lFalse.mV.y,
            lMask.mV.z != 0.0f ? lTrue.mV.z : lFalse.mV.z,
            lMask.mV.w != 0.0f ? lTrue.mV.w : lFalse.mV.w }; return Vector4(lResult); }
    }

    // Magnitude is the genuine SDK VecFloat(Vector4::InParam) imported above.
    // A scalar const-reference overload would make this shared include ambiguous
    // and would expose the wrong native return convention.

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

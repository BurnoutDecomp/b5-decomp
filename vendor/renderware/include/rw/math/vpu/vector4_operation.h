#pragma once

// Portable PC reconstruction of the RenderWare rwmath VPU Vector4 / VecFloat operation
// vocabulary (rw/math/vpu/vector4_operation.h + vec_float_*_inline.h). The console SDK
// implements these over AltiVec/VMX 128-bit float registers; here every op is the obvious
// per-lane scalar lowering over the 4 named lanes of `Vector4` (see vpu/types.h). Only the
// ops actually consumed by reconstructed game code are provided; this is the type-operation
// vocabulary, not a full SDK port.
//
// VecFloat is `Vector4` (a broadcast scalar held in all 4 lanes); the same operators serve
// both -- a VecFloat is just a Vector4 whose lanes happen to be equal.

#include "rw/math/vpu/types.h"   // Vector4, MaskScalar

#include <cmath>                 // std::sqrt (Magnitude)

namespace rw
{
namespace math
{
namespace vpu
{
    // -- arithmetic (lane-wise; vaddfp / vsubfp / vmulfp) --------------------------------
    inline Vector4 operator+(Vector4 lLhs, Vector4 lRhs)
    {
        return Vector4{ lLhs.x + lRhs.x, lLhs.y + lRhs.y, lLhs.z + lRhs.z, lLhs.w + lRhs.w };
    }
    inline Vector4 operator-(Vector4 lLhs, Vector4 lRhs)
    {
        return Vector4{ lLhs.x - lRhs.x, lLhs.y - lRhs.y, lLhs.z - lRhs.z, lLhs.w - lRhs.w };
    }
    // Per-lane product (vmulfp128). The broadcast-scalar (VecFloat) case falls out of this:
    // a scalar held in every lane multiplies each lane of the other operand.
    inline Vector4 operator*(Vector4 lLhs, Vector4 lRhs)
    {
        return Vector4{ lLhs.x * lRhs.x, lLhs.y * lRhs.y, lLhs.z * lRhs.z, lLhs.w * lRhs.w };
    }

    // -- the all-ones vector (vspltisw v,1 ; vcfsx v,v,0 -> 1.0 per lane) -----------------
    inline Vector4 GetVector4_One() { return Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }; }

    // Broadcast one scalar into all four lanes (the VecFloat construction the SDK spells as
    // `VecFloat(float)` / a vspltw of a loaded scalar). A VecFloat *is* such a Vector4.
    inline Vector4 Splat(float lfScalar) { return Vector4{ lfScalar, lfScalar, lfScalar, lfScalar }; }

    // -- extrema / clamp (vminfp / vmaxfp; Clamp = max-then-min) --------------------------
    inline Vector4 Min(Vector4 lLhs, Vector4 lRhs)
    {
        return Vector4{
            lLhs.x < lRhs.x ? lLhs.x : lRhs.x,
            lLhs.y < lRhs.y ? lLhs.y : lRhs.y,
            lLhs.z < lRhs.z ? lLhs.z : lRhs.z,
            lLhs.w < lRhs.w ? lLhs.w : lRhs.w };
    }
    inline Vector4 Max(Vector4 lLhs, Vector4 lRhs)
    {
        return Vector4{
            lLhs.x > lRhs.x ? lLhs.x : lRhs.x,
            lLhs.y > lRhs.y ? lLhs.y : lRhs.y,
            lLhs.z > lRhs.z ? lLhs.z : lRhs.z,
            lLhs.w > lRhs.w ? lLhs.w : lRhs.w };
    }
    inline Vector4 Clamp(Vector4 lrValue, Vector4 lrMin, Vector4 lrMax)
    {
        return Min(Max(lrValue, lrMin), lrMax);
    }

    // -- lane accessor (Vector4::operator[]) ---------------------------------------------
    inline float GetComponent(const Vector4& lrVector, int liLane)
    {
        return (&lrVector.x)[liLane];
    }

    // -- mask ops ------------------------------------------------------------------------
    // And(a,b) = vand, lane-wise over the 0x00000000 / 0xFFFFFFFF compare masks.
    inline MaskScalar And(MaskScalar lLhs, MaskScalar lRhs)
    {
        // Lower to the float-domain mask convention used by MaskScalar (see vpu/types.h):
        // a lane is "true" iff non-zero. The bitwise AND of two broadcast masks is the
        // logical AND of their per-lane truth.
        auto lTrue = [](float f) { return f != 0.0f; };
        return MaskScalar{
            (lTrue(lLhs.x) && lTrue(lRhs.x)) ? 1.0f : 0.0f,
            (lTrue(lLhs.y) && lTrue(lRhs.y)) ? 1.0f : 0.0f,
            (lTrue(lLhs.z) && lTrue(lRhs.z)) ? 1.0f : 0.0f,
            (lTrue(lLhs.w) && lTrue(lRhs.w)) ? 1.0f : 0.0f };
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
        return Vector4{
            lMask.x != 0.0f ? lTrue.x : lFalse.x,
            lMask.y != 0.0f ? lTrue.y : lFalse.y,
            lMask.z != 0.0f ? lTrue.z : lFalse.z,
            lMask.w != 0.0f ? lTrue.w : lFalse.w };
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
        return MaskScalar{
            !(lA.x >= lB.x) ? 1.0f : 0.0f,
            !(lA.y >= lB.y) ? 1.0f : 0.0f,
            !(lA.z >= lB.z) ? 1.0f : 0.0f,
            !(lA.w >= lB.w) ? 1.0f : 0.0f };
    }

    inline MaskScalar CompGreaterThan(Vector4 lA, Vector4 lB)
    {
        return MaskScalar{
            lA.x > lB.x ? 1.0f : 0.0f,
            lA.y > lB.y ? 1.0f : 0.0f,
            lA.z > lB.z ? 1.0f : 0.0f,
            lA.w > lB.w ? 1.0f : 0.0f };
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
        const float lfDot = lrVector.x * lrVector.x + lrVector.y * lrVector.y +
                            lrVector.z * lrVector.z + lrVector.w * lrVector.w;
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

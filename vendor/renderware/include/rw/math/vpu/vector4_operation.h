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
    inline Vector4 Select(Vector4 lFalse, Vector4 lTrue, MaskScalar lMask)
    {
        return Vector4{
            lMask.x != 0.0f ? lTrue.x : lFalse.x,
            lMask.y != 0.0f ? lTrue.y : lFalse.y,
            lMask.z != 0.0f ? lTrue.z : lFalse.z,
            lMask.w != 0.0f ? lTrue.w : lFalse.w };
    }

    // Greater-than compare producing a per-lane mask (vcmpgtfp): 1.0 where lLhs>lRhs.
    inline MaskScalar IsGreater(Vector4 lLhs, Vector4 lRhs)
    {
        return MaskScalar{
            lLhs.x > lRhs.x ? 1.0f : 0.0f,
            lLhs.y > lRhs.y ? 1.0f : 0.0f,
            lLhs.z > lRhs.z ? 1.0f : 0.0f,
            lLhs.w > lRhs.w ? 1.0f : 0.0f };
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

#pragma once

#include <vectorintrinsics.h>
#include "SDKs/EATech/include/rw/math/vpu/vector3.h"
#include "SDKs/EATech/include/rw/math/vpu/vector4.h"

// Shared rwmath type entry point. Vector3/4 use their genuine SDK class homes above.
// Vector2 remains the separately recovered VPU16 type; packed FPU/Basic2D pairs
// and the existing matrix/quaternion declarations retain their distinct contracts.

namespace rw
{
namespace math
{
namespace vpu
{
    // ARTIST821F0DB0 / DecFIGS vector2.h:45/49/130: genuine VPU register.
    // Copy construction and assignment stay implicit for the native value ABI.
    struct CGS_ALIGN(16) __declspec(passinreg) Vector2
    {
        __vector4 mV;
        typedef const Vector2 InParam;
        Vector2() {}
        Vector2(float lfX, float lfY);
        void SetZero() { mV = __vspltisw(0); }
    };

    // ADDITIVE GROW (rw-physics solver group): the SDK quaternion -- one 16-byte lane
    // register, (x, y, z, w) with w the scalar part. The DWARF spells
    // JointFrames::mQuatA/mQuatB/mQuatL, DriveFrames::mQuatA/mQuatB and RigidBody::mQuat as
    // `Quaternion`, and the rwmath quaternion_operation family returns it.
    //
    // This type was ALREADY WRITTEN, in src/vendor/renderware/physics/JointFrames.hpp, whose
    // own banner says: "defined here because their canonical homes (the vendor
    // rw/math/vpu/types.h ...) sit outside this wave's writable tree; move them there on the
    // next grow of that header". This is that grow -- the definition moved here verbatim and
    // JointFrames.hpp now includes it rather than declaring a second copy.
    struct CGS_ALIGN(16) Quaternion
    {
        float x, y, z, w;
        void SetIdentity() { x = y = z = 0.0f; w = 1.0f; }
    };
    struct CGS_ALIGN(16) Vector3Plus
    {
        float x, y, z, w;

        void SetZero() { x = y = z = w = 0.0f; }
        Vector3 GetVector3() const { return Vector3(x, y, z); }
        void SetVector3(Vector3 lVector) { x = lVector.mV.x; y = lVector.mV.y; z = lVector.mV.z; }
        float GetPlus() const { return w; }
        void SetPlus(float lfValue) { w = lfValue; }
    }; // w: the "plus" lane

    // Row-major; the last row of an affine is implicit (0,0,0,1) but still stored.
    struct CGS_ALIGN(16) Matrix44
    {
        typedef const Matrix44& InParam;
        Vector4 xAxis, yAxis, zAxis, wAxis;
        // DecFIGS matrix44.h:28/31; the nontrivial copy is also required by
        // ARTIST's pointer ABI for Matrix44 values (for example 8227B8A0).
        Matrix44() {}
        Matrix44(const Matrix44& lrMatrix)
            : xAxis(lrMatrix.xAxis), yAxis(lrMatrix.yAxis),
              zAxis(lrMatrix.zAxis), wAxis(lrMatrix.wAxis) {}
        void SetZero() { xAxis.SetZero(); yAxis.SetZero(); zAxis.SetZero(); wAxis.SetZero(); }
        void SetIdentity()
        {
            const Vector4 lxAxis(1.0f, 0.0f, 0.0f, 0.0f); xAxis = lxAxis; const Vector4 lyAxis(0.0f, 1.0f, 0.0f, 0.0f); yAxis = lyAxis;
            const Vector4 lzAxis(0.0f, 0.0f, 1.0f, 0.0f); zAxis = lzAxis; const Vector4 lwAxis(0.0f, 0.0f, 0.0f, 1.0f); wAxis = lwAxis;
        }
    };
    struct CGS_ALIGN(16) Matrix44Affine
    {
        typedef const Matrix44Affine& InParam;
        Vector3 xAxis, yAxis, zAxis, wAxis;
        // DecFIGS matrix44affine.h:61/63/64. ARTIST 822B33B8 receives
        // this value through r5; the explicit copy preserves all four raw rows.
        Matrix44Affine() {}
        Matrix44Affine(const Matrix44Affine& lrMatrix)
            : xAxis(lrMatrix.xAxis), yAxis(lrMatrix.yAxis),
              zAxis(lrMatrix.zAxis), wAxis(lrMatrix.wAxis) {}
        Matrix44Affine(Vector3 lxAxis, Vector3 lyAxis, Vector3 lzAxis, Vector3 lwAxis)
            : xAxis(lxAxis), yAxis(lyAxis), zAxis(lzAxis), wAxis(lwAxis) {}
        void SetZero() { xAxis.SetZero(); yAxis.SetZero(); zAxis.SetZero(); wAxis.SetZero(); }
        void SetIdentity()
        {
            const Vector3 lxAxis(1.0f, 0.0f, 0.0f); xAxis = lxAxis; const Vector3 lyAxis(0.0f, 1.0f, 0.0f); yAxis = lyAxis;
            const Vector3 lzAxis(0.0f, 0.0f, 1.0f); zAxis = lzAxis; const Vector3 lwAxis(0.0f, 0.0f, 0.0f); wAxis = lwAxis;
        }

        Vector3& Right() { return xAxis; }
        Vector3& Up() { return yAxis; }
        Vector3& At() { return zAxis; }
        Vector3& Pos() { return wAxis; }

        const Vector3& Right() const { return xAxis; }
        const Vector3& Up() const { return yAxis; }
        const Vector3& At() const { return zAxis; }
        const Vector3& Pos() const { return wAxis; }
    };

    // ADDITIVE GROW (flagged by BrnPhysics-bodies group): a 3x3 rotation/inertia matrix,
    // stored as three 16-byte rows (each a Vector3 lane register), matching the console
    // `rw::math::vpu::Matrix33` layout (48 bytes, 16-aligned). Needed as the storage type
    // for BrnPhysics::ExternalPhysicsBody::m{Local,World}InverseInertia (DWARF spells those
    // members `Matrix33`). No existing user; purely a new type-vocabulary entry in the
    // canonical RW math home. SetIdentity sets the upper-3x3 to the identity basis.
    struct CGS_ALIGN(16) Matrix33
    {
        Vector3 xAxis, yAxis, zAxis;
        void SetZero() { xAxis.SetZero(); yAxis.SetZero(); zAxis.SetZero(); }
        void SetIdentity()
        {
            const Vector3 lxAxis(1.0f, 0.0f, 0.0f); xAxis = lxAxis; const Vector3 lyAxis(0.0f, 1.0f, 0.0f); yAxis = lyAxis;
            const Vector3 lzAxis(0.0f, 0.0f, 1.0f); zAxis = lzAxis;
        }
    };

    // ADDITIVE GROW (class:rw::math::vpu::MaskScalar): the SDK's 128-bit per-lane
    // comparison-result wrapper -- the broadcast "mask" a VMX vcmpeqfp/vcmpgtfp leaves in a
    // vector register (each lane 0x00000000 = false / 0xFFFFFFFF = true). Same 16-byte,
    // 16-aligned register layout as the Vector* family, stored here as four named float
    // lanes so the float-domain compare semantics reduce exactly (a 0xFFFFFFFF lane reads
    // as NaN, which != 0.0; a 0x00000000 lane is +0.0; -0.0 also reads false, matching
    // vcmpeqfp). The SIMD producers (the vcmp operations themselves) live in the SDK
    // *_operation headers and are not modelled here; only the scalar reduction the SDK
    // exposes as a type method is provided.
    struct CGS_ALIGN(16) MaskScalar
    {
        float x, y, z, w;

        // X360 0x821F0D78  rw::math::vpu::MaskScalar::GetBool.
        //   vspltisw v0,0 ; lvx128 v13,this ; vcmpeqfp. v0,v13,v0  -> CR6
        //   mfocrf/not/extrwi r,r,1,24 reads (and inverts) CR6's all-lanes-equal bit.
        // vcmpeqfp. against zero sets CR6[0] ("all true") iff every lane == 0.0; the
        // trailing `not` then `extrwi ...,1,24` returns the COMPLEMENT of that bit -- i.e.
        // the mask is "true" iff at least one lane is non-zero. Lowered to portable scalar
        // float math; called by PolygonSoupTesterJob::{FillTriangleCache,LineTestNearestSS}.
        bool GetBool() const
        {
            return !(x == 0.0f && y == 0.0f && z == 0.0f && w == 0.0f);
        }
    };
}
}
}

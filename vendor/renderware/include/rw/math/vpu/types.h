#pragma once

// Portable PC reconstruction of the RenderWare rwmath VPU vector/matrix TYPES
// (EARenderWare rwmath 1.02.00, rw/math/vpu/*). The console SDK implements these
// over hardware SIMD (a 16-byte `vector float` register held in `VectorIntrinsic mV`,
// operated on with AltiVec/VMX intrinsics that do not exist on PC). We keep the
// memory *layout* exact — one 16-byte, 16-aligned register per vector (4 named lanes);
// matrices are 4 rows — so game structs that embed these match the console layout and
// compile on x64. The SIMD operations live in the SDK's *_operation headers and are
// not reproduced here (not needed for the type vocabulary).

namespace rw
{
namespace math
{
namespace vpu
{
    // One SIMD register: 4 lanes, 16-byte aligned. The SDK wraps this as a
    // `VectorIntrinsic mV` member; here the lanes are named directly for PC access.
    // The complex SIMD math (add/mul/transform) stays in the *_operation headers; only
    // the trivial initialisers the SDK exposes as type methods (SetZero/SetIdentity,
    // spelled `v.SetZero()` / `m.SetIdentity()` at the call sites) are provided here.
    struct CGS_ALIGN(16) Vector2 { float x, y, z, w; void SetZero() { x = y = z = w = 0.0f; } };
    struct CGS_ALIGN(16) Vector3 { float x, y, z, w; void SetZero() { x = y = z = w = 0.0f; }
                                 typedef const Vector3& InParam; };   // w: unused 4th lane
    struct CGS_ALIGN(16) Vector4 { float x, y, z, w; void SetZero() { x = y = z = w = 0.0f; }
                                 typedef const Vector4& InParam; };

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
        Vector3 GetVector3() const { const Vector3 lResult = { x, y, z, 0.0f }; return lResult; }
        void SetVector3(Vector3 lVector) { x = lVector.x; y = lVector.y; z = lVector.z; }
        float GetPlus() const { return w; }
        void SetPlus(float lfValue) { w = lfValue; }
    }; // w: the "plus" lane

    // Row-major; the last row of an affine is implicit (0,0,0,1) but still stored.
    struct CGS_ALIGN(16) Matrix44
    {
        typedef const Matrix44& InParam;
        Vector4 xAxis, yAxis, zAxis, wAxis;
        void SetZero() { xAxis.SetZero(); yAxis.SetZero(); zAxis.SetZero(); wAxis.SetZero(); }
        void SetIdentity()
        {
            const Vector4 lxAxis = { 1.0f, 0.0f, 0.0f, 0.0f }; xAxis = lxAxis; const Vector4 lyAxis = { 0.0f, 1.0f, 0.0f, 0.0f }; yAxis = lyAxis;
            const Vector4 lzAxis = { 0.0f, 0.0f, 1.0f, 0.0f }; zAxis = lzAxis; const Vector4 lwAxis = { 0.0f, 0.0f, 0.0f, 1.0f }; wAxis = lwAxis;
        }
    };
    struct CGS_ALIGN(16) Matrix44Affine
    {
        typedef const Matrix44Affine& InParam;
        Vector3 xAxis, yAxis, zAxis, wAxis;
        void SetZero() { xAxis.SetZero(); yAxis.SetZero(); zAxis.SetZero(); wAxis.SetZero(); }
        void SetIdentity()
        {
            const Vector3 lxAxis = { 1.0f, 0.0f, 0.0f, 0.0f }; xAxis = lxAxis; const Vector3 lyAxis = { 0.0f, 1.0f, 0.0f, 0.0f }; yAxis = lyAxis;
            const Vector3 lzAxis = { 0.0f, 0.0f, 1.0f, 0.0f }; zAxis = lzAxis; const Vector3 lwAxis = { 0.0f, 0.0f, 0.0f, 0.0f }; wAxis = lwAxis;
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
            const Vector3 lxAxis = { 1.0f, 0.0f, 0.0f, 0.0f }; xAxis = lxAxis; const Vector3 lyAxis = { 0.0f, 1.0f, 0.0f, 0.0f }; yAxis = lyAxis;
            const Vector3 lzAxis = { 0.0f, 0.0f, 1.0f, 0.0f }; zAxis = lzAxis;
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

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
    struct alignas(16) Vector2 { float x, y, z, w; void SetZero() { x = y = z = w = 0.0f; } };
    struct alignas(16) Vector3 { float x, y, z, w; void SetZero() { x = y = z = w = 0.0f; } };   // w: unused 4th lane
    struct alignas(16) Vector4 { float x, y, z, w; void SetZero() { x = y = z = w = 0.0f; } };
    struct alignas(16) Vector3Plus { float x, y, z, w; void SetZero() { x = y = z = w = 0.0f; } }; // w: the "plus" lane

    // Row-major; the last row of an affine is implicit (0,0,0,1) but still stored.
    struct alignas(16) Matrix44
    {
        Vector4 xAxis, yAxis, zAxis, wAxis;
        void SetZero() { xAxis.SetZero(); yAxis.SetZero(); zAxis.SetZero(); wAxis.SetZero(); }
        void SetIdentity()
        {
            xAxis = { 1.0f, 0.0f, 0.0f, 0.0f }; yAxis = { 0.0f, 1.0f, 0.0f, 0.0f };
            zAxis = { 0.0f, 0.0f, 1.0f, 0.0f }; wAxis = { 0.0f, 0.0f, 0.0f, 1.0f };
        }
    };
    struct alignas(16) Matrix44Affine
    {
        Vector3 xAxis, yAxis, zAxis, wAxis;
        void SetZero() { xAxis.SetZero(); yAxis.SetZero(); zAxis.SetZero(); wAxis.SetZero(); }
        void SetIdentity()
        {
            xAxis = { 1.0f, 0.0f, 0.0f, 0.0f }; yAxis = { 0.0f, 1.0f, 0.0f, 0.0f };
            zAxis = { 0.0f, 0.0f, 1.0f, 0.0f }; wAxis = { 0.0f, 0.0f, 0.0f, 0.0f };
        }
    };

    // ADDITIVE GROW (flagged by BrnPhysics-bodies group): a 3x3 rotation/inertia matrix,
    // stored as three 16-byte rows (each a Vector3 lane register), matching the console
    // `rw::math::vpu::Matrix33` layout (48 bytes, 16-aligned). Needed as the storage type
    // for BrnPhysics::ExternalPhysicsBody::m{Local,World}InverseInertia (DWARF spells those
    // members `Matrix33`). No existing user; purely a new type-vocabulary entry in the
    // canonical RW math home. SetIdentity sets the upper-3x3 to the identity basis.
    struct alignas(16) Matrix33
    {
        Vector3 xAxis, yAxis, zAxis;
        void SetZero() { xAxis.SetZero(); yAxis.SetZero(); zAxis.SetZero(); }
        void SetIdentity()
        {
            xAxis = { 1.0f, 0.0f, 0.0f, 0.0f }; yAxis = { 0.0f, 1.0f, 0.0f, 0.0f };
            zAxis = { 0.0f, 0.0f, 1.0f, 0.0f };
        }
    };
}
}
}

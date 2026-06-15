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
    struct alignas(16) Vector2 { float x, y, z, w; };
    struct alignas(16) Vector3 { float x, y, z, w; };   // w: unused 4th lane
    struct alignas(16) Vector4 { float x, y, z, w; };
    struct alignas(16) Vector3Plus { float x, y, z, w; }; // w: the "plus" lane

    // Row-major; the last row of an affine is implicit (0,0,0,1) but still stored.
    struct alignas(16) Matrix44       { Vector4 xAxis, yAxis, zAxis, wAxis; };
    struct alignas(16) Matrix44Affine { Vector3 xAxis, yAxis, zAxis, wAxis; };
}
}
}

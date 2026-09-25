#pragma once

// ============================================================================
// GameSource/Director/Camera/Utils/BrnConsoleVpu.h
//
// [FX-DIRECTOR2 2026-09-25] The RenderWare vpu inlines the director's rig camera executes, written the way the
// CONSOLE rounds them -- the campaign rounding rule (scratch/CRASHPARITY_0922/ROUNDING_RULE.md). None has a console
// symbol: each is an SDK inline, expanded at its call sites, and each banner below names the sites whose instructions
// it was read from.
//
// WHY THEY EXIST: the PC vendor SDK (vendor/renderware/include/rw/math/vpu) evaluates Mult / TransformVector /
// TransformPoint / InverseOfMatrixWithOrthonormal3x3 as separate f32 multiplies and adds, left to right, and
// Matrix44AffineFromAxisRotationAngle with std::sin / std::cos. The console executes vmaddfp cascades -- ONE rounding
// per multiply-add (rule 3) -- seeded in a fixed order, and the XDK's XMVectorSinCos polynomial
// (SDKs/XboxMath/XMVectorSinCos.h). The two differ by an ulp or so per operation; over CameraRig::Construct's 732
// instructions the vendor form lands up to 3.9e-6 from the console, and these helpers reproduce the emulator run of
// the console's words bit for bit (run_fxdirector2_camera_rig.py). They are used ONLY at the sites whose instructions
// were read; the vendor SDK is untouched (the rule: choose per site, from the instruction the console executes there).
//
// IDA prints the classic vmaddfp operands in field order D, A, B, C, which computes D = A * C + B; the 128 forms are
// vmaddfp128 D = A * B + D and vmaddcfp128 D = A * D + B.
//
// FLAG (PC-platform): the console's VMX flushes denormal operands and results to zero; the PC's scalar SSE does not
// (rule 6). Out of scope for camera-sized values.
// ============================================================================

#include <cmath>                                // std::fma
#include "types.hpp"                            // f32
#include "rw/math/vpu/types.h"                  // rw::math::vpu::Vector3 / Matrix44Affine
#include "SDKs/XboxMath/XMVectorSinCos.h"       // XboxMath::XMVectorSinCos (the XDK polynomial the console inlines)

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{
namespace ConsoleVpu
{
    using rw::math::vpu::Matrix44Affine;
    using rw::math::vpu::Vector3;

    // vmaddfp lane: a * b + c, ONE rounding (rule 3).
    inline f32 MultiplyAdd(f32 lfA, f32 lfB, f32 lfC)
    {
        return std::fma(lfA, lfB, lfC);
    }

    // The four lanes of a vmaddfp over a broadcast multiplier: a * s + c.
    inline Vector3 MultiplyAdd(const Vector3& lrA, f32 lfScalar, const Vector3& lrC)
    {
        Vector3 lResult;
        lResult.x = MultiplyAdd(lrA.x, lfScalar, lrC.x);
        lResult.y = MultiplyAdd(lrA.y, lfScalar, lrC.y);
        lResult.z = MultiplyAdd(lrA.z, lfScalar, lrC.z);
        lResult.w = MultiplyAdd(lrA.w, lfScalar, lrC.w);
        return lResult;
    }

    // ------------------------------------------------------------------------
    // rw::math::vpu::Matrix44AffineFromAxisRotationAngle (the SDK's Rodrigues builder), as CameraRig::Construct
    // inlines it three times -- the yaw's at 0x8220B3F4..0x8220B468 reads:
    //   t = 1 - c (vsubfp128) ; tx, ty, tz = t * axis (vmulfp128) ; sx, sy, sz = s * axis (vmulfp128)
    //   xAxis = { tx ax + c, tx ay + sz, tx az - sy }      the "+" terms one vmaddfp (fused), the "-" terms a
    //   yAxis = { ty ax - sz, ty ay + c, ty az + sx }      vmulfp then a vsubfp (two roundings) -- the compiler
    //   zAxis = { tz ax + sy, tz ay - sx, tz az + c }      has no fused a * b - c
    //   wAxis = 0
    // then packed into rows with vperm (control 0x82CDA350) / vrlimi128, which move lanes without arithmetic. The
    // sine and cosine are the inlined XMVectorSinCos.
    // ------------------------------------------------------------------------
    inline Matrix44Affine Matrix44AffineFromAxisRotationAngle(const Vector3& lrAxis, f32 lfAngle)
    {
        const f32 KF_ONE = 1.0f;   // vcsxwfp128 of a vspltisw 1

        f32 lfSin = 0.0f;
        f32 lfCos = 0.0f;
        XboxMath::XMVectorSinCos(&lfSin, &lfCos, lfAngle);

        const f32 lfT  = KF_ONE - lfCos;
        const f32 lfTX = lfT * lrAxis.x;
        const f32 lfTY = lfT * lrAxis.y;
        const f32 lfTZ = lfT * lrAxis.z;
        const f32 lfSX = lfSin * lrAxis.x;
        const f32 lfSY = lfSin * lrAxis.y;
        const f32 lfSZ = lfSin * lrAxis.z;

        Matrix44Affine lResult;
        lResult.xAxis.x = MultiplyAdd(lfTX, lrAxis.x, lfCos);
        lResult.xAxis.y = MultiplyAdd(lfTX, lrAxis.y, lfSZ);
        lResult.xAxis.z = lfTX * lrAxis.z - lfSY;
        lResult.xAxis.w = 0.0f;
        lResult.yAxis.x = lfTY * lrAxis.x - lfSZ;
        lResult.yAxis.y = MultiplyAdd(lfTY, lrAxis.y, lfCos);
        lResult.yAxis.z = MultiplyAdd(lfTY, lrAxis.z, lfSX);
        lResult.yAxis.w = 0.0f;
        lResult.zAxis.x = MultiplyAdd(lfTZ, lrAxis.x, lfSY);
        lResult.zAxis.y = lfTZ * lrAxis.y - lfSX;
        lResult.zAxis.z = MultiplyAdd(lfTZ, lrAxis.z, lfCos);
        lResult.zAxis.w = 0.0f;
        lResult.wAxis.x = 0.0f;
        lResult.wAxis.y = 0.0f;
        lResult.wAxis.z = 0.0f;
        lResult.wAxis.w = 0.0f;
        return lResult;
    }

    // ------------------------------------------------------------------------
    // The SDK's row transforms, as BehaviourRig::Update @0x822427C0 executes them:
    //   TransformVector(m, v) = v.x * m.xAxis, then + v.y * m.yAxis, then + v.z * m.zAxis
    //       (vmulfp128, then two vmaddfp128 / vmaddcfp128 -- the target velocity at 0x82242E58..0x82242E60)
    //   TransformPoint(m, p)  = m.wAxis + p.x * m.xAxis, then + p.y * m.yAxis, then + p.z * m.zAxis
    //       (three vmaddfp128 seeded with the translation -- the camera's position row at 0x82242FA8 /
    //       0x82242FDC / 0x82242FFC)
    // Every lane of every row (the w lanes included) runs the same fused cascade.
    // ------------------------------------------------------------------------
    inline Vector3 TransformVector(const Matrix44Affine& lrMatrix, const Vector3& lrVector)
    {
        Vector3 lResult;
        lResult.x = MultiplyAdd(lrVector.z, lrMatrix.zAxis.x, MultiplyAdd(lrVector.y, lrMatrix.yAxis.x, lrVector.x * lrMatrix.xAxis.x));
        lResult.y = MultiplyAdd(lrVector.z, lrMatrix.zAxis.y, MultiplyAdd(lrVector.y, lrMatrix.yAxis.y, lrVector.x * lrMatrix.xAxis.y));
        lResult.z = MultiplyAdd(lrVector.z, lrMatrix.zAxis.z, MultiplyAdd(lrVector.y, lrMatrix.yAxis.z, lrVector.x * lrMatrix.xAxis.z));
        lResult.w = MultiplyAdd(lrVector.z, lrMatrix.zAxis.w, MultiplyAdd(lrVector.y, lrMatrix.yAxis.w, lrVector.x * lrMatrix.xAxis.w));
        return lResult;
    }

    inline Vector3 TransformPoint(const Matrix44Affine& lrMatrix, const Vector3& lrPoint)
    {
        Vector3 lResult;
        lResult.x = MultiplyAdd(lrPoint.z, lrMatrix.zAxis.x, MultiplyAdd(lrPoint.y, lrMatrix.yAxis.x, MultiplyAdd(lrPoint.x, lrMatrix.xAxis.x, lrMatrix.wAxis.x)));
        lResult.y = MultiplyAdd(lrPoint.z, lrMatrix.zAxis.y, MultiplyAdd(lrPoint.y, lrMatrix.yAxis.y, MultiplyAdd(lrPoint.x, lrMatrix.xAxis.y, lrMatrix.wAxis.y)));
        lResult.z = MultiplyAdd(lrPoint.z, lrMatrix.zAxis.z, MultiplyAdd(lrPoint.y, lrMatrix.yAxis.z, MultiplyAdd(lrPoint.x, lrMatrix.xAxis.z, lrMatrix.wAxis.z)));
        lResult.w = MultiplyAdd(lrPoint.z, lrMatrix.zAxis.w, MultiplyAdd(lrPoint.y, lrMatrix.yAxis.w, MultiplyAdd(lrPoint.x, lrMatrix.xAxis.w, lrMatrix.wAxis.w)));
        return lResult;
    }

    // ------------------------------------------------------------------------
    // rw::math::vpu::Mult(a, b), the affine product a * b: each rotation row of a through TransformVector(b, .), its
    // translation row through TransformPoint(b, .). Read at BehaviourRig::Update 0x82242F90..0x8224300C (the camera =
    // rig x car) and 0x82242D6C..0x82242E34 (the looked-at car in the car's frame), and at the products inside
    // CameraRig::Construct (checked bit for bit against the emulator).
    // ------------------------------------------------------------------------
    inline Matrix44Affine Mult(const Matrix44Affine& lrLhs, const Matrix44Affine& lrRhs)
    {
        Matrix44Affine lResult;
        lResult.xAxis = ConsoleVpu::TransformVector(lrRhs, lrLhs.xAxis);   // qualified: ADL would also find
        lResult.yAxis = ConsoleVpu::TransformVector(lrRhs, lrLhs.yAxis);   // the vendor rw::math::vpu forms
        lResult.zAxis = ConsoleVpu::TransformVector(lrRhs, lrLhs.zAxis);
        lResult.wAxis = ConsoleVpu::TransformPoint(lrRhs, lrLhs.wAxis);
        return lResult;
    }

    // ------------------------------------------------------------------------
    // rw::math::vpu::InverseOfMatrixWithOrthonormal3x3, as BehaviourRig::Update executes it at 0x82242D20..
    // 0x82242D64: the 3x3 transposed with vmrghw / vmrglw (T0 = {x.x, y.x, z.x, 0}, T1 = the y column, T2 = the z
    // column), the position negated as 0 - pos (vsubfp128 against a vspltisw 0), and the inverse translation
    // accumulated Z FIRST:
    //   (-p.z) * T2 (vmulfp128) ; + (-p.y) * T1 (vmaddfp128) ; + (-p.x) * T0 (vmaddcfp128)
    // ------------------------------------------------------------------------
    inline Matrix44Affine InverseOfMatrixWithOrthonormal3x3(const Matrix44Affine& lrMatrix)
    {
        Matrix44Affine lResult;
        lResult.xAxis.x = lrMatrix.xAxis.x; lResult.xAxis.y = lrMatrix.yAxis.x; lResult.xAxis.z = lrMatrix.zAxis.x; lResult.xAxis.w = 0.0f;
        lResult.yAxis.x = lrMatrix.xAxis.y; lResult.yAxis.y = lrMatrix.yAxis.y; lResult.yAxis.z = lrMatrix.zAxis.y; lResult.yAxis.w = 0.0f;
        lResult.zAxis.x = lrMatrix.xAxis.z; lResult.zAxis.y = lrMatrix.yAxis.z; lResult.zAxis.z = lrMatrix.zAxis.z; lResult.zAxis.w = 0.0f;

        const f32 lfNegX = 0.0f - lrMatrix.wAxis.x;
        const f32 lfNegY = 0.0f - lrMatrix.wAxis.y;
        const f32 lfNegZ = 0.0f - lrMatrix.wAxis.z;
        lResult.wAxis.x = MultiplyAdd(lResult.xAxis.x, lfNegX, MultiplyAdd(lfNegY, lResult.yAxis.x, lfNegZ * lResult.zAxis.x));
        lResult.wAxis.y = MultiplyAdd(lResult.xAxis.y, lfNegX, MultiplyAdd(lfNegY, lResult.yAxis.y, lfNegZ * lResult.zAxis.y));
        lResult.wAxis.z = MultiplyAdd(lResult.xAxis.z, lfNegX, MultiplyAdd(lfNegY, lResult.yAxis.z, lfNegZ * lResult.zAxis.z));
        lResult.wAxis.w = MultiplyAdd(lResult.xAxis.w, lfNegX, MultiplyAdd(lfNegY, lResult.yAxis.w, lfNegZ * lResult.zAxis.w));
        return lResult;
    }
}
}
}
}

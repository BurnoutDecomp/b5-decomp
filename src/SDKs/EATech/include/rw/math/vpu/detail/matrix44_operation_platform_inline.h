#pragma once

// Portable PC reconstruction of EARenderWare rwmath (1.02.00)
// rw/math/vpu/detail/matrix44_operation_platform_inline.h -- the platform-level free
// matrix operations declared via matrix44.h.
//
// Per the project's vendor-SIMD rule, the AltiVec/VMX intrinsics each body uses are lowered
// to portable lane math, exactly as the committed VecFloat lane wrappers are. Each lowered
// intrinsic is named in a comment. The two reciprocal/permute building blocks here:
//   * vspltw(v, AXIS)      broadcast lane AXIS of v to all four lanes.
//   * vmaddfp(a, b, c)     per-lane  a*b + c   (fused multiply-add).
//
// FAITHFULLY RECONSTRUCTED (simple vec_float-op wrappers):
//   * Mult(Vector4, Matrix44)          row-combine via vspltw + vmaddfp chain.
//   * Mult(Matrix44, Matrix44)         four Mult(Vector4,Matrix44) row applications.
//   * Mult(Matrix44Affine, Matrix44)   affine row-combine (wAxis carries translation).
//
// LEFT DECLARATION-ONLY + FLAGGED (genuinely multi-stage hand-VMX that does NOT map onto
// the committed vec_float ops -- a scalar paraphrase would be a fabrication):
//   * Transpose(Matrix44)              vmrghw/vmrglw strided word-interleave transpose.
//   * Inverse(Matrix44, VecFloat&)     _asmInverse: an undecoded multi-stage VMX pipeline
//                                      (cross/permute/vrefp-Newton reciprocal block).
//   * Matrix44FromAxisRotationAngle    depends on SinCos + VecFloat arithmetic operators
//                                      (operator -,*,+) that are NOT committed in this
//                                      cluster, plus the 16-VecFloat Matrix44 ctor.

#include "SDKs/EATech/include/rw/math/vpu/matrix44.h"

namespace rw
{
namespace math
{
namespace vpu
{

// =====================================================================================
//  Mult -- FAITHFUL (vspltw broadcast + vmaddfp multiply-accumulate)
// =====================================================================================

inline Vector4 Mult(Vector4::InParam v, Matrix44::InParam m)
{
    // x/y/z/w = vspltw(v, X/Y/Z/W): broadcast each component of v.
    // result = vmaddfp(m.xAxis, x, 0);  += m.yAxis*y; += m.zAxis*z; += m.wAxis*w;
    // i.e. per output lane k: sum_i m.row_i.lane[k] * v.lane[i].
    VectorIntrinsic result;
    for (int k = 0; k < 4; ++k)
    {
        result.mafLane[k] =
              m.xAxis.mV.mafLane[k] * v.mV.mafLane[VectorAxisX]
            + m.yAxis.mV.mafLane[k] * v.mV.mafLane[VectorAxisY]
            + m.zAxis.mV.mafLane[k] * v.mV.mafLane[VectorAxisZ]
            + m.wAxis.mV.mafLane[k] * v.mV.mafLane[VectorAxisW];
    }
    return Vector4(result);
}

inline Matrix44 Mult(Matrix44::InParam m, Matrix44::InParam b)
{
    return Matrix44(
        Mult(m.xAxis, b),
        Mult(m.yAxis, b),
        Mult(m.zAxis, b),
        Mult(m.wAxis, b));
}

inline Matrix44 Mult(Matrix44Affine::InParam m, Matrix44::InParam b)
{
    // Affine: each row i of m has its X/Y/Z lanes broadcast (vspltw) against b's
    // xAxis/yAxis/zAxis rows; the wAxis combine seeds its accumulator with b.wAxis
    // (the homogeneous translation) instead of zero.
    //   temp_i = vspltw(m.row_i, X) * b.xAxis  (+ b.wAxis for row 3)
    //   temp_i += vspltw(m.row_i, Y) * b.yAxis
    //   result_i = temp_i + vspltw(m.row_i, Z) * b.zAxis
    VectorIntrinsic result[4];
    const VectorIntrinsic* mRows[4] = { &m.xAxis.mV, &m.yAxis.mV, &m.zAxis.mV, &m.wAxis.mV };

    for (int i = 0; i < 4; ++i)
    {
        const float mx = mRows[i]->mafLane[VectorAxisX];
        const float my = mRows[i]->mafLane[VectorAxisY];
        const float mz = mRows[i]->mafLane[VectorAxisZ];
        for (int k = 0; k < 4; ++k)
        {
            float acc = (i == 3) ? b.wAxis.mV.mafLane[k] : 0.0f;
            acc += mx * b.xAxis.mV.mafLane[k];
            acc += my * b.yAxis.mV.mafLane[k];
            acc += mz * b.zAxis.mV.mafLane[k];
            result[i].mafLane[k] = acc;
        }
    }
    return Matrix44(result[0], result[1], result[2], result[3]);
}

// Transpose -- FAITHFUL. The X360 vmrghw/vmrglw 4x4 word-interleave cascade is the standard
// matrix transpose: output row j, lane i = input row i, lane j (a deterministic lane shuffle,
// no undecoded permute constant). Rows are xAxis/yAxis/zAxis/wAxis.
inline Matrix44 Transpose(Matrix44::InParam m)
{
    const VectorIntrinsic* mRows[4] = { &m.xAxis.mV, &m.yAxis.mV, &m.zAxis.mV, &m.wAxis.mV };
    VectorIntrinsic result[4];
    for (int j = 0; j < 4; ++j)        // output row
    {
        for (int i = 0; i < 4; ++i)    // output lane
        {
            result[j].mafLane[i] = mRows[i]->mafLane[j];
        }
    }
    return Matrix44(result[0], result[1], result[2], result[3]);
}

// =====================================================================================
//  Inverse / Matrix44FromAxisRotationAngle -- DECLARATION-ONLY + FLAGGED
//
//  These are genuinely multi-stage hand-VMX (word-interleave transpose; the _asmInverse
//  cross-product + vrefp-Newton reciprocal pipeline; SinCos + uncommitted VecFloat
//  arithmetic). They do NOT collapse onto the committed vec_float ops, so per the
//  no-fabrication rule they are declared without bodies here. A reviewer/conductor that
//  homes the vec_float cross/transpose/reciprocal helpers can then supply faithful bodies.
// =====================================================================================

Matrix44 Inverse(Matrix44::InParam m, VecFloat& determinant);  // FLAGGED: _asmInverse VMX pipeline
Matrix44 Matrix44FromAxisRotationAngle(Vector3::InParam axis, VecFloat::InParam angle); // FLAGGED: SinCos + VecFloat ops

}
}
}

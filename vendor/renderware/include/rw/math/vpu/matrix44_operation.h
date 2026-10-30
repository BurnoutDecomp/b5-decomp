#pragma once

// Canonical RenderWare SDK home for the rw::math::vpu **Matrix44** (full 4x4, non-affine)
// operation vocabulary (EARenderWare rwmath 1.02.00, rw/math/vpu/matrix44_operation.h --
// sibling to types.h, vector4_operation.h and matrix44affine_operation.h).
//
// gen-tool note: tools/renderware/generate_headers.py only writes rwcore_enums.h /
// rwcore_structs.h / rwcore.h under include/rw/; it never touches rw/math/vpu/, so this
// hand-maintained header (like its siblings) is immune to regeneration.
//
// Bodies live out-of-line in vendor/renderware/src/rw/math/vpu/Matrix44Operation.cpp --
// Production BrnCommonTypes VecFloat is an alias of this vocabulary's Vector4.
// The separate SDKs/EATech mV-based Matrix44/VecFloat headers are not included by
// this canonical wrapper; their declaration-only Inverse is a separate recovery.

#include "rw/math/vpu/types.h"   // rw::math::vpu::Matrix44 / Vector4

namespace rw
{
namespace math
{
namespace vpu
{
    // Inverse(m, det) @ X360 0x825B2628 -- the GENERAL 4x4 inverse (no affine/orthonormal
    // fast path; every one of the 16 elements participates). Returns adj(m)/det and writes
    // the determinant, broadcast to all four lanes, through `lrDeterminant`.
    // The reciprocal has two unconditional Newton refinements: singular matrices
    // retain the original NaN result, without an identity/affine/zero fallback.
    //
    // For an affine matrix with an orthonormal 3x3 prefer
    // InverseOfMatrixWithOrthonormal3x3 (matrix44affine_operation.h) -- that is the cheap
    // transpose-based path the SDK offers alongside this one.
    Matrix44 Inverse(const Matrix44& lrMatrix, Vector4& lrDeterminant);

    // Mult(view, projection) / operator* -- the AFFINE x FULL-4x4 product, i.e. the
    // world-view-projection build. Row-major, row-vector: the three rotation rows of the affine
    // are transformed by the projection's first three rows alone, and the translation row
    // additionally picks up the projection's fourth row, because an affine's implicit fourth
    // COLUMN puts a 1 in that row's w lane.
    //
    // ADDITIVE GROW 2026-08-15 (post-fx step-6 producers wave). ATTESTED: MotionBlurState::Update
    // @0x823F8490 emits it twice, inlined, once per arm (asm 0x823F8A40-0x823F8ACC for
    // `mCurrentWVP = lView * lProjection`, 0x823F893C-0x823F898C for the constructed previous
    // pair); the DWARF hint listing for that function names `rw::math::vpu::Mult` three times --
    // twice in the lfTimeStep == 0 arm, once in the other. The X360 shape is the standard
    // vspltw128 broadcast of each source lane plus a vmaddfp cascade over the projection's four
    // rows, with the translation row seeded from projection.wAxis (`vmaddfp v5, v4, v2, v0` at
    // 0x823F8A84 == view.wAxis.x * projection.xAxis + projection.wAxis).
    inline Matrix44 Mult(const Matrix44Affine& lrAffine, const Matrix44& lrMatrix)
    {
        Matrix44 lResult;
        lResult.xAxis.mV.x = lrAffine.xAxis.mV.x * lrMatrix.xAxis.mV.x + lrAffine.xAxis.mV.y * lrMatrix.yAxis.mV.x + lrAffine.xAxis.mV.z * lrMatrix.zAxis.mV.x;
        lResult.xAxis.mV.y = lrAffine.xAxis.mV.x * lrMatrix.xAxis.mV.y + lrAffine.xAxis.mV.y * lrMatrix.yAxis.mV.y + lrAffine.xAxis.mV.z * lrMatrix.zAxis.mV.y;
        lResult.xAxis.mV.z = lrAffine.xAxis.mV.x * lrMatrix.xAxis.mV.z + lrAffine.xAxis.mV.y * lrMatrix.yAxis.mV.z + lrAffine.xAxis.mV.z * lrMatrix.zAxis.mV.z;
        lResult.xAxis.mV.w = lrAffine.xAxis.mV.x * lrMatrix.xAxis.mV.w + lrAffine.xAxis.mV.y * lrMatrix.yAxis.mV.w + lrAffine.xAxis.mV.z * lrMatrix.zAxis.mV.w;

        lResult.yAxis.mV.x = lrAffine.yAxis.mV.x * lrMatrix.xAxis.mV.x + lrAffine.yAxis.mV.y * lrMatrix.yAxis.mV.x + lrAffine.yAxis.mV.z * lrMatrix.zAxis.mV.x;
        lResult.yAxis.mV.y = lrAffine.yAxis.mV.x * lrMatrix.xAxis.mV.y + lrAffine.yAxis.mV.y * lrMatrix.yAxis.mV.y + lrAffine.yAxis.mV.z * lrMatrix.zAxis.mV.y;
        lResult.yAxis.mV.z = lrAffine.yAxis.mV.x * lrMatrix.xAxis.mV.z + lrAffine.yAxis.mV.y * lrMatrix.yAxis.mV.z + lrAffine.yAxis.mV.z * lrMatrix.zAxis.mV.z;
        lResult.yAxis.mV.w = lrAffine.yAxis.mV.x * lrMatrix.xAxis.mV.w + lrAffine.yAxis.mV.y * lrMatrix.yAxis.mV.w + lrAffine.yAxis.mV.z * lrMatrix.zAxis.mV.w;

        lResult.zAxis.mV.x = lrAffine.zAxis.mV.x * lrMatrix.xAxis.mV.x + lrAffine.zAxis.mV.y * lrMatrix.yAxis.mV.x + lrAffine.zAxis.mV.z * lrMatrix.zAxis.mV.x;
        lResult.zAxis.mV.y = lrAffine.zAxis.mV.x * lrMatrix.xAxis.mV.y + lrAffine.zAxis.mV.y * lrMatrix.yAxis.mV.y + lrAffine.zAxis.mV.z * lrMatrix.zAxis.mV.y;
        lResult.zAxis.mV.z = lrAffine.zAxis.mV.x * lrMatrix.xAxis.mV.z + lrAffine.zAxis.mV.y * lrMatrix.yAxis.mV.z + lrAffine.zAxis.mV.z * lrMatrix.zAxis.mV.z;
        lResult.zAxis.mV.w = lrAffine.zAxis.mV.x * lrMatrix.xAxis.mV.w + lrAffine.zAxis.mV.y * lrMatrix.yAxis.mV.w + lrAffine.zAxis.mV.z * lrMatrix.zAxis.mV.w;

        lResult.wAxis.mV.x = lrAffine.wAxis.mV.x * lrMatrix.xAxis.mV.x + lrAffine.wAxis.mV.y * lrMatrix.yAxis.mV.x + lrAffine.wAxis.mV.z * lrMatrix.zAxis.mV.x + lrMatrix.wAxis.mV.x;
        lResult.wAxis.mV.y = lrAffine.wAxis.mV.x * lrMatrix.xAxis.mV.y + lrAffine.wAxis.mV.y * lrMatrix.yAxis.mV.y + lrAffine.wAxis.mV.z * lrMatrix.zAxis.mV.y + lrMatrix.wAxis.mV.y;
        lResult.wAxis.mV.z = lrAffine.wAxis.mV.x * lrMatrix.xAxis.mV.z + lrAffine.wAxis.mV.y * lrMatrix.yAxis.mV.z + lrAffine.wAxis.mV.z * lrMatrix.zAxis.mV.z + lrMatrix.wAxis.mV.z;
        lResult.wAxis.mV.w = lrAffine.wAxis.mV.x * lrMatrix.xAxis.mV.w + lrAffine.wAxis.mV.y * lrMatrix.yAxis.mV.w + lrAffine.wAxis.mV.z * lrMatrix.zAxis.mV.w + lrMatrix.wAxis.mV.w;
        return lResult;
    }
    inline Matrix44 operator*(const Matrix44Affine& lrAffine, const Matrix44& lrMatrix)
    {
        return Mult(lrAffine, lrMatrix);
    }

} // namespace vpu
} // namespace math
} // namespace rw

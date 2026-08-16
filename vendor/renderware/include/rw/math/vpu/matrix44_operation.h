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
// the existing call sites (BrnShadowMap.cpp, BrnEffectsGlassManager.cpp) declare
// `Inverse` TU-locally against exactly this signature and link against that TU.

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
        lResult.xAxis.x = lrAffine.xAxis.x * lrMatrix.xAxis.x + lrAffine.xAxis.y * lrMatrix.yAxis.x + lrAffine.xAxis.z * lrMatrix.zAxis.x;
        lResult.xAxis.y = lrAffine.xAxis.x * lrMatrix.xAxis.y + lrAffine.xAxis.y * lrMatrix.yAxis.y + lrAffine.xAxis.z * lrMatrix.zAxis.y;
        lResult.xAxis.z = lrAffine.xAxis.x * lrMatrix.xAxis.z + lrAffine.xAxis.y * lrMatrix.yAxis.z + lrAffine.xAxis.z * lrMatrix.zAxis.z;
        lResult.xAxis.w = lrAffine.xAxis.x * lrMatrix.xAxis.w + lrAffine.xAxis.y * lrMatrix.yAxis.w + lrAffine.xAxis.z * lrMatrix.zAxis.w;

        lResult.yAxis.x = lrAffine.yAxis.x * lrMatrix.xAxis.x + lrAffine.yAxis.y * lrMatrix.yAxis.x + lrAffine.yAxis.z * lrMatrix.zAxis.x;
        lResult.yAxis.y = lrAffine.yAxis.x * lrMatrix.xAxis.y + lrAffine.yAxis.y * lrMatrix.yAxis.y + lrAffine.yAxis.z * lrMatrix.zAxis.y;
        lResult.yAxis.z = lrAffine.yAxis.x * lrMatrix.xAxis.z + lrAffine.yAxis.y * lrMatrix.yAxis.z + lrAffine.yAxis.z * lrMatrix.zAxis.z;
        lResult.yAxis.w = lrAffine.yAxis.x * lrMatrix.xAxis.w + lrAffine.yAxis.y * lrMatrix.yAxis.w + lrAffine.yAxis.z * lrMatrix.zAxis.w;

        lResult.zAxis.x = lrAffine.zAxis.x * lrMatrix.xAxis.x + lrAffine.zAxis.y * lrMatrix.yAxis.x + lrAffine.zAxis.z * lrMatrix.zAxis.x;
        lResult.zAxis.y = lrAffine.zAxis.x * lrMatrix.xAxis.y + lrAffine.zAxis.y * lrMatrix.yAxis.y + lrAffine.zAxis.z * lrMatrix.zAxis.y;
        lResult.zAxis.z = lrAffine.zAxis.x * lrMatrix.xAxis.z + lrAffine.zAxis.y * lrMatrix.yAxis.z + lrAffine.zAxis.z * lrMatrix.zAxis.z;
        lResult.zAxis.w = lrAffine.zAxis.x * lrMatrix.xAxis.w + lrAffine.zAxis.y * lrMatrix.yAxis.w + lrAffine.zAxis.z * lrMatrix.zAxis.w;

        lResult.wAxis.x = lrAffine.wAxis.x * lrMatrix.xAxis.x + lrAffine.wAxis.y * lrMatrix.yAxis.x + lrAffine.wAxis.z * lrMatrix.zAxis.x + lrMatrix.wAxis.x;
        lResult.wAxis.y = lrAffine.wAxis.x * lrMatrix.xAxis.y + lrAffine.wAxis.y * lrMatrix.yAxis.y + lrAffine.wAxis.z * lrMatrix.zAxis.y + lrMatrix.wAxis.y;
        lResult.wAxis.z = lrAffine.wAxis.x * lrMatrix.xAxis.z + lrAffine.wAxis.y * lrMatrix.yAxis.z + lrAffine.wAxis.z * lrMatrix.zAxis.z + lrMatrix.wAxis.z;
        lResult.wAxis.w = lrAffine.wAxis.x * lrMatrix.xAxis.w + lrAffine.wAxis.y * lrMatrix.yAxis.w + lrAffine.wAxis.z * lrMatrix.zAxis.w + lrMatrix.wAxis.w;
        return lResult;
    }
    inline Matrix44 operator*(const Matrix44Affine& lrAffine, const Matrix44& lrMatrix)
    {
        return Mult(lrAffine, lrMatrix);
    }

} // namespace vpu
} // namespace math
} // namespace rw

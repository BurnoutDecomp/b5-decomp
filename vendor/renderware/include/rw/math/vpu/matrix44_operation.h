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
        const __vector4 lvRow0 = lrMatrix.xAxis.mV;
        const __vector4 lvRow1 = lrMatrix.yAxis.mV;
        const __vector4 lvRow2 = lrMatrix.zAxis.mV;
        const __vector4 lvRow3 = lrMatrix.wAxis.mV;
        __vector4 lvResult = __vmulfp(__vspltw(lrAffine.xAxis.mV, 0), lvRow0);
        lvResult = __vmaddfp(__vspltw(lrAffine.xAxis.mV, 1), lvRow1, lvResult);
        lResult.xAxis.mV = __vmaddfp(__vspltw(lrAffine.xAxis.mV, 2), lvRow2, lvResult);
        lvResult = __vmulfp(__vspltw(lrAffine.yAxis.mV, 0), lvRow0);
        lvResult = __vmaddfp(__vspltw(lrAffine.yAxis.mV, 1), lvRow1, lvResult);
        lResult.yAxis.mV = __vmaddfp(__vspltw(lrAffine.yAxis.mV, 2), lvRow2, lvResult);
        lvResult = __vmulfp(__vspltw(lrAffine.zAxis.mV, 0), lvRow0);
        lvResult = __vmaddfp(__vspltw(lrAffine.zAxis.mV, 1), lvRow1, lvResult);
        lResult.zAxis.mV = __vmaddfp(__vspltw(lrAffine.zAxis.mV, 2), lvRow2, lvResult);
        lvResult = __vmaddfp(__vspltw(lrAffine.wAxis.mV, 0), lvRow0, lvRow3);
        lvResult = __vmaddfp(__vspltw(lrAffine.wAxis.mV, 1), lvRow1, lvResult);
        lResult.wAxis.mV = __vmaddfp(__vspltw(lrAffine.wAxis.mV, 2), lvRow2, lvResult);
        return lResult;
    }
    inline Matrix44 operator*(const Matrix44Affine& lrAffine, const Matrix44& lrMatrix)
    {
        return Mult(lrAffine, lrMatrix);
    }

} // namespace vpu
} // namespace math
} // namespace rw

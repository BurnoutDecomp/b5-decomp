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

} // namespace vpu
} // namespace math
} // namespace rw

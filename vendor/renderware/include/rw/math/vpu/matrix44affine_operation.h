#pragma once

// Canonical RenderWare SDK home for the rw::math::vpu Matrix44Affine operation
// vocabulary (EARenderWare rwmath 1.02.00, rw/math/vpu/matrix44affine_operation.h --
// sibling to types.h and vector3_operation.h). The free-function names and the
// member spellings (xAxis/yAxis/zAxis/wAxis, Pos()) are locked from the public
// rwmath headers and the DecFIGS DWARF call lists (BrnPositionLag.cpp attests live
// instances of InverseOfMatrixWithOrthonormal3x3 / TransformPoint / TransformVector
// adjacent to the Matrix44Affine type).
//
// The console SDK implements these over AltiVec/VMX SIMD on the 16-byte
// `VectorIntrinsic` rows: the affine point/vector transforms are a lane-broadcast
// (vspltw128 of each source component) FMA cascade (vmaddfp) over the four 16-byte
// matrix rows; InverseOfMatrixWithOrthonormal3x3 builds the rotation transpose with
// vmrglw/vmrghw lane-merge permutes and computes the inverse translation as the
// transpose applied to the negated position (a vsubfp against zero then the same
// FMA cascade). The PC reconstruction operates on the named float lanes of the flat
// Matrix44Affine / Vector3 aggregates in types.h; per-lane f32 math gives exact
// semantic parity with the vspltw128 + vmaddfp accumulation.
//
// gen-tool note: tools/renderware/generate_headers.py only writes rwcore_*.h under
// include/rw/; it never touches rw/math/vpu/, so this hand-maintained header (like
// its siblings types.h / vector3_operation.h) is immune to regeneration.

#include "rw/math/vpu/types.h"             // rw::math::vpu::Matrix44Affine / Vector3
#include "rw/math/vpu/vector3_operation.h" // Vector3 operator+/-, Mult, Lerp

namespace rw
{
namespace math
{
namespace vpu
{
    // -- affine transforms --------------------------------------------------------------

    // TransformVector(m, v): rotate the vector by the affine's upper 3x3 (rows
    // xAxis/yAxis/zAxis); the translation row (wAxis) is NOT added -- a direction
    // transform. X360 shape: vspltw128(v.x/y/z) broadcast + vmaddfp cascade over the
    // three rotation rows (xAxis*v.x + yAxis*v.y + zAxis*v.z).
    inline Vector3 TransformVector(const Matrix44Affine& lrMatrix, Vector3 lvVector)
    {
        const f32 lfX = lvVector.x;
        const f32 lfY = lvVector.y;
        const f32 lfZ = lvVector.z;

        Vector3 lvResult;
        lvResult.x = lrMatrix.xAxis.x * lfX + lrMatrix.yAxis.x * lfY + lrMatrix.zAxis.x * lfZ;
        lvResult.y = lrMatrix.xAxis.y * lfX + lrMatrix.yAxis.y * lfY + lrMatrix.zAxis.y * lfZ;
        lvResult.z = lrMatrix.xAxis.z * lfX + lrMatrix.yAxis.z * lfY + lrMatrix.zAxis.z * lfZ;
        lvResult.w = 0.0f;
        return lvResult;
    }

    // TransformPoint(m, p): transform the point by the full affine -- rotate by the
    // 3x3 (rows xAxis/yAxis/zAxis) and add the translation row (wAxis). X360 shape:
    // vspltw128(p.x/y/z) broadcast + vmaddfp cascade over all four rows
    // (xAxis*p.x + yAxis*p.y + zAxis*p.z + wAxis).
    inline Vector3 TransformPoint(const Matrix44Affine& lrMatrix, Vector3 lvPoint)
    {
        const f32 lfX = lvPoint.x;
        const f32 lfY = lvPoint.y;
        const f32 lfZ = lvPoint.z;

        Vector3 lvResult;
        lvResult.x = lrMatrix.xAxis.x * lfX + lrMatrix.yAxis.x * lfY + lrMatrix.zAxis.x * lfZ + lrMatrix.wAxis.x;
        lvResult.y = lrMatrix.xAxis.y * lfX + lrMatrix.yAxis.y * lfY + lrMatrix.zAxis.y * lfZ + lrMatrix.wAxis.y;
        lvResult.z = lrMatrix.xAxis.z * lfX + lrMatrix.yAxis.z * lfY + lrMatrix.zAxis.z * lfZ + lrMatrix.wAxis.z;
        lvResult.w = 0.0f;
        return lvResult;
    }

    // -- composition --------------------------------------------------------------------

    // Mult(m, b) / operator*(m, b): the affine matrix product `m * b` (row-major). Each
    // output row i is m.row_i transformed by b: the rotation rows are
    //   out.row_i = m.row_i.x * b.xAxis + m.row_i.y * b.yAxis + m.row_i.z * b.zAxis
    // and the translation row additionally adds b.wAxis (m.wAxis is a point, so its w==1
    // contribution is the +b.wAxis term). X360 shape (matrix44affine_operation_platform_inline.h
    // :84-90): the source rows are vspltw128-broadcast component-by-component (sp0..sp3) and
    // accumulated with a vmaddfp cascade over the loaded b rows (ma0..ma3 / bx/by/bz), the
    // translation row seeded with b.wAxis (the `zero`/w handling). Per-lane f32 math gives exact
    // semantic parity with that broadcast+FMA accumulation.
    inline Matrix44Affine Mult(const Matrix44Affine& lrLhs, const Matrix44Affine& lrRhs)
    {
        Matrix44Affine lResult;
        lResult.xAxis = TransformVector(lrRhs, lrLhs.xAxis);
        lResult.yAxis = TransformVector(lrRhs, lrLhs.yAxis);
        lResult.zAxis = TransformVector(lrRhs, lrLhs.zAxis);
        lResult.wAxis = TransformPoint(lrRhs, lrLhs.wAxis);
        return lResult;
    }
    inline Matrix44Affine operator*(const Matrix44Affine& lrLhs, const Matrix44Affine& lrRhs)
    {
        return Mult(lrLhs, lrRhs);
    }

    // -- inverse ------------------------------------------------------------------------

    // InverseOfMatrixWithOrthonormal3x3(m): the affine inverse when the upper 3x3 is
    // orthonormal (a pure rotation). The inverse rotation is the transpose of the 3x3,
    // and the inverse translation is the transpose applied to the negated position:
    //   invR = transpose(R)            (col k of R becomes row k of invR)
    //   invT = invR * (-Pos)  i.e. invT.k = -(Pos . R.row_k)  (lane k = -Pos dot axis k)
    // X360 shape: the transpose is built by vmrglw/vmrghw lane merges; the inverse
    // translation is `vsubfp v,0,Pos` then a vmaddfp cascade of the transposed rows
    // against the negated-position broadcasts. The translation lanes computed there
    // are exactly -(Pos . xAxis), -(Pos . yAxis), -(Pos . zAxis).
    inline Matrix44Affine InverseOfMatrixWithOrthonormal3x3(const Matrix44Affine& lrMatrix)
    {
        Matrix44Affine lInverse;

        // Inverse rotation = transpose of the upper 3x3 (rows become columns).
        lInverse.xAxis.x = lrMatrix.xAxis.x; lInverse.xAxis.y = lrMatrix.yAxis.x; lInverse.xAxis.z = lrMatrix.zAxis.x; lInverse.xAxis.w = 0.0f;
        lInverse.yAxis.x = lrMatrix.xAxis.y; lInverse.yAxis.y = lrMatrix.yAxis.y; lInverse.yAxis.z = lrMatrix.zAxis.y; lInverse.yAxis.w = 0.0f;
        lInverse.zAxis.x = lrMatrix.xAxis.z; lInverse.zAxis.y = lrMatrix.yAxis.z; lInverse.zAxis.z = lrMatrix.zAxis.z; lInverse.zAxis.w = 0.0f;

        // Inverse translation = transpose applied to the negated position, i.e. each
        // lane is the negative dot of the original position with the matching axis row.
        const Vector3& lrPos = lrMatrix.wAxis;
        lInverse.wAxis.x = -(lrPos.x * lrMatrix.xAxis.x + lrPos.y * lrMatrix.xAxis.y + lrPos.z * lrMatrix.xAxis.z);
        lInverse.wAxis.y = -(lrPos.x * lrMatrix.yAxis.x + lrPos.y * lrMatrix.yAxis.y + lrPos.z * lrMatrix.yAxis.z);
        lInverse.wAxis.z = -(lrPos.x * lrMatrix.zAxis.x + lrPos.y * lrMatrix.zAxis.y + lrPos.z * lrMatrix.zAxis.z);
        lInverse.wAxis.w = 0.0f;
        return lInverse;
    }
}
}
}

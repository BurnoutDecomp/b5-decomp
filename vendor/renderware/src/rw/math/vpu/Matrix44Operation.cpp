// ===========================================================================
// rw/math/vpu/Matrix44Operation.cpp -- the rw::math::vpu **Matrix44** (full 4x4)
// operation bodies. Declared in rw/math/vpu/matrix44_operation.h.
//
// Reconstructed from X360 ARTIST:
//   rw::math::vpu::Inverse @ 0x825B2628
//
// X360 shape (0x825B2628, straight-line VMX, no branches):
//   r3 = &result (sret), r4 = &source matrix, r5 = &determinant out.
//   The four 16-byte rows are loaded with lvx128 at +0/+16/+32/+48, then each row is
//   fed through the SAME lane-rotate permute (vperm against the constant at
//   unk_82CDA440) one, two and three times -- giving row^1/row^2/row^3, where
//   row^k[i] == row[(i+k) & 3].
//
//   Each cofactor row is then the lane-parallel 3x3 determinant ("triple product")
//   of the OTHER three rows taken in cyclic order, accumulated with vmaddfp/vnmsubfp:
//
//     A[i] = M1^1*(M2^2*M3^3 - M2^3*M3^2)
//          + M1^2*(M2^3*M3^1 - M2^1*M3^3)
//          + M1^3*(M2^1*M3^2 - M2^2*M3^1)          (cofactor row 0, from rows 1,2,3)
//
//   ...and likewise B from rows (2,3,0), C from (3,0,1), D from (0,1,2). The
//   checkerboard cofactor sign is applied as a vxor against a lane mask built with
//   `vslw v,v,v` (0xFFFFFFFF << 31 == 0x80000000, i.e. the float sign bit) punched
//   out per lane by vrlimi128: rows 0/2 get (+,-,+,-) and rows 1/3 get (-,+,-,+),
//   which is exactly (-1)^(row+col).
//
//   The determinant is `vmsum4fp128 v13, v31, v30` -- the 4-lane dot product of the
//   UNPERMUTED row 0 with cofactor row 0, broadcast to all four lanes -- and is
//   stored whole through r5 (stvx128 v13, r0, r5).
//
//   The adjugate is the TRANSPOSE of the cofactor matrix, formed by the classic
//   vmrghw/vmrglw 4x4 lane-merge transpose, and each transposed row is scaled by the
//   reciprocal of the determinant (vrefp estimate + two Newton-Raphson refinements:
//   e' = e + e*(1 - d*e), spelled vnmsubfp/vmaddfp against the vcfsx-materialised
//   1.0f) before the four stvx128 stores at r3 +0/+16/+32/+48.
//
// => This is the GENERAL 4x4 inverse. There is no affine / orthonormal fast path in
//    this function: all 16 source elements feed the cofactors, the w column is never
//    assumed to be (0,0,0,1), and all four result rows are written. (The cheap
//    transpose-based path is the separate InverseOfMatrixWithOrthonormal3x3, in
//    matrix44affine_operation.h.)
//
// PC lowering: the permutes exist only to get the lane rotations into VMX registers,
// so the reconstruction indexes the lanes directly. The two Newton-Raphson passes
// over a 12-bit vrefp seed converge to full f32 precision, so a plain `1.0f / det`
// is the same value -- emulating the estimate would ADD hardware behaviour, not
// preserve it (and it degenerates identically for det == 0: +/-inf, then NaN).
// ===========================================================================

#include "rw/math/vpu/matrix44_operation.h"

#include <cstddef>   // offsetof

namespace rw
{
namespace math
{
namespace vpu
{

namespace
{
    // The four lanes of a row, in the order the VMX lane-rotate permute walks them.
    // (Vector4 is four named f32 lanes; copying them into an indexable local is what
    // lets the cyclic row^k rotation be written as arithmetic instead of a permute.)
    struct Lanes
    {
        float l[4];
    };

    inline Lanes GetLanes(const Vector4& lrRow)
    {
        Lanes lLanes;
        lLanes.l[0] = lrRow.x;
        lLanes.l[1] = lrRow.y;
        lLanes.l[2] = lrRow.z;
        lLanes.l[3] = lrRow.w;
        return lLanes;
    }

    // One cofactor row: the lane-parallel 3x3 determinant of rows (A, B, C) -- the
    // three rows OTHER than the one being expanded, in cyclic order. For output lane
    // `i` the three participating columns are the cyclic rotations (i+1, i+2, i+3),
    // which is exactly what the repeated vperm produced on the console.
    //
    // `lbOddRow` selects the checkerboard sign: (-1)^(row+col), i.e. lanes 1 and 3
    // are negated for an even row and lanes 0 and 2 for an odd one (the vxor against
    // the vslw/vrlimi128 sign-bit mask).
    Vector4 CofactorRow(const Vector4& lrRowA, const Vector4& lrRowB, const Vector4& lrRowC,
                        bool lbOddRow)
    {
        const Lanes lA = GetLanes(lrRowA);
        const Lanes lB = GetLanes(lrRowB);
        const Lanes lC = GetLanes(lrRowC);

        Lanes lCofactor;
        for (int liLane = 0; liLane < 4; ++liLane)
        {
            const int liP = (liLane + 1) & 3;
            const int liQ = (liLane + 2) & 3;
            const int liS = (liLane + 3) & 3;

            lCofactor.l[liLane] = lA.l[liP] * (lB.l[liQ] * lC.l[liS] - lB.l[liS] * lC.l[liQ])
                                + lA.l[liQ] * (lB.l[liS] * lC.l[liP] - lB.l[liP] * lC.l[liS])
                                + lA.l[liS] * (lB.l[liP] * lC.l[liQ] - lB.l[liQ] * lC.l[liP]);

            const bool lbNegate = (((liLane + (lbOddRow ? 1 : 0)) & 1) != 0);
            if (lbNegate)
            {
                lCofactor.l[liLane] = -lCofactor.l[liLane];
            }
        }

        return Vector4{ lCofactor.l[0], lCofactor.l[1], lCofactor.l[2], lCofactor.l[3] };
    }
} // namespace

Matrix44 Inverse(const Matrix44& lrMatrix, Vector4& lrDeterminant)
{
    // -- the four cofactor rows (each from the other three source rows, cyclically) --
    const Vector4 lvCofactor0 = CofactorRow(lrMatrix.yAxis, lrMatrix.zAxis, lrMatrix.wAxis, false);
    const Vector4 lvCofactor1 = CofactorRow(lrMatrix.zAxis, lrMatrix.wAxis, lrMatrix.xAxis, true);
    const Vector4 lvCofactor2 = CofactorRow(lrMatrix.wAxis, lrMatrix.xAxis, lrMatrix.yAxis, false);
    const Vector4 lvCofactor3 = CofactorRow(lrMatrix.xAxis, lrMatrix.yAxis, lrMatrix.zAxis, true);

    // -- determinant: vmsum4fp128 of the unpermuted row 0 with cofactor row 0, in every
    //    lane (the console stores the whole broadcast register through r5) -----------
    const float lfDeterminant = lrMatrix.xAxis.x * lvCofactor0.x
                              + lrMatrix.xAxis.y * lvCofactor0.y
                              + lrMatrix.xAxis.z * lvCofactor0.z
                              + lrMatrix.xAxis.w * lvCofactor0.w;

    lrDeterminant.x = lfDeterminant;
    lrDeterminant.y = lfDeterminant;
    lrDeterminant.z = lfDeterminant;
    lrDeterminant.w = lfDeterminant;

    // -- adjugate = transpose of the cofactor matrix (the vmrghw/vmrglw transpose),
    //    scaled by 1/det (vrefp + 2x Newton-Raphson) ----------------------------------
    const float lfInvDeterminant = 1.0f / lfDeterminant;

    Matrix44 lInverse;
    lInverse.xAxis = Vector4{ lvCofactor0.x * lfInvDeterminant, lvCofactor1.x * lfInvDeterminant,
                              lvCofactor2.x * lfInvDeterminant, lvCofactor3.x * lfInvDeterminant };
    lInverse.yAxis = Vector4{ lvCofactor0.y * lfInvDeterminant, lvCofactor1.y * lfInvDeterminant,
                              lvCofactor2.y * lfInvDeterminant, lvCofactor3.y * lfInvDeterminant };
    lInverse.zAxis = Vector4{ lvCofactor0.z * lfInvDeterminant, lvCofactor1.z * lfInvDeterminant,
                              lvCofactor2.z * lfInvDeterminant, lvCofactor3.z * lfInvDeterminant };
    lInverse.wAxis = Vector4{ lvCofactor0.w * lfInvDeterminant, lvCofactor1.w * lfInvDeterminant,
                              lvCofactor2.w * lfInvDeterminant, lvCofactor3.w * lfInvDeterminant };

    return lInverse;
}

// Layout pins (_AssertLayout equivalent -- every member here is public, so the
// asserts need no member-function scope). The console lvx128/stvx128 offsets
// (+0/+16/+32/+48) are only correct here because Matrix44 is four 16-byte,
// 16-aligned float4 rows on the host too -- no pointer widens inside these
// aggregates, so this is one of the rare console strides that survives verbatim.
static_assert(sizeof(Vector4) == 16, "rw::math::vpu::Vector4 must be one 16-byte lane register");
static_assert(sizeof(Matrix44) == 64, "rw::math::vpu::Matrix44 must be four 16-byte rows");
static_assert(offsetof(Matrix44, xAxis) == 0,  "Matrix44::xAxis at +0  (lvx128 r4+0)");
static_assert(offsetof(Matrix44, yAxis) == 16, "Matrix44::yAxis at +16 (lvx128 r4+0x10)");
static_assert(offsetof(Matrix44, zAxis) == 32, "Matrix44::zAxis at +32 (lvx128 r4+0x20)");
static_assert(offsetof(Matrix44, wAxis) == 48, "Matrix44::wAxis at +48 (lvx128 r4+0x30)");

} // namespace vpu
} // namespace math
} // namespace rw

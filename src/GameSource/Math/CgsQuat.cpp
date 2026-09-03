#include "GameSource/Math/CgsQuat.h"

#include <cmath>   // std::sqrt

// cQuat::FromMatrix @ 0x82908450 and cQuat::ToMatrix @ 0x8290A9D8.
//
// Both are pure scalar FPU routines on the X360 (lfs/stfs/fadds/fsubs/fmuls/fsqrts/
// fdivs) over a 4-float quaternion (lanes x,y,z,w) and a 16-float Matrix44 (the four
// 4-float lane rows xAxis/yAxis/zAxis/wAxis). The rodata constants resolve as:
//   flt_82001C98 = 1.0   flt_82001CC0 = 0.0   flt_82001D9C = 2.0
//   flt_82004EF4 = 4.0   flt_82003F40 = 0.25
//
// Matrix float-index -> Matrix44 lane map (idx = byte_offset/4):
//   0..3   = xAxis.{x,y,z,w}   (row 0)
//   4..7   = yAxis.{x,y,z,w}   (row 1)
//   8..11  = zAxis.{x,y,z,w}   (row 2)
//   12..15 = wAxis.{x,y,z,w}   (row 3)

// ---------------------------------------------------------------------------------------
// FromMatrix (X360 @ 0x82908450) -- rotation matrix -> quaternion (Shepperd's method).
//
// ASM: builds the four trace candidates
//     t0 = m00 - m11 - m22 + 1     (x largest)
//     t1 = 1 - m00 + m11 - m22     (y largest)
//     t2 = 1 - m00 - m11 + m22     (z largest)
//     t3 = m00 + m11 + m22 + 1     (w largest)
// (m00=xAxis.x=idx0, m11=yAxis.y=idx5, m22=zAxis.z=idx10), picks the max via the
// fcmpu/fmr cascade, then for the winning branch computes s = 1/sqrt(cand*4), sets the
// dominant component to sqrt(cand*4)*0.25 and the other three from the off-diagonal
// sums/differences * s. The off-diagonal terms are read by their exact byte offsets:
//   idx1=xAxis.y idx2=xAxis.z idx4=yAxis.x idx6=yAxis.z idx8=zAxis.x idx9=zAxis.y.
cQuat& cQuat::FromMatrix(cQuat& lResult, const Matrix44& lMatrix)
{
    const f32 lfM00 = lMatrix.xAxis.x;   // idx0
    const f32 lfM01 = lMatrix.xAxis.y;   // idx1
    const f32 lfM02 = lMatrix.xAxis.z;   // idx2
    const f32 lfM10 = lMatrix.yAxis.x;   // idx4
    const f32 lfM11 = lMatrix.yAxis.y;   // idx5
    const f32 lfM12 = lMatrix.yAxis.z;   // idx6
    const f32 lfM20 = lMatrix.zAxis.x;   // idx8
    const f32 lfM21 = lMatrix.zAxis.y;   // idx9
    const f32 lfM22 = lMatrix.zAxis.z;   // idx10

    const f32 lfXCandidate = (lfM00 + 1.0f) - lfM11 - lfM22;
    const f32 lfYCandidate = (1.0f - lfM00) + lfM11 - lfM22;
    const f32 lfZCandidate = (1.0f - lfM00) - lfM11 + lfM22;
    const f32 lfWCandidate = lfM00 + lfM11 + lfM22 + 1.0f;

    // max(xCand, yCand, zCand, wCand) -- the fmr cascade at 0x82908490..0x829084B8.
    f32 lfLargest = lfXCandidate;
    if (lfYCandidate > lfLargest) lfLargest = lfYCandidate;
    if (lfZCandidate > lfLargest) lfLargest = lfZCandidate;
    if (lfWCandidate > lfLargest) lfLargest = lfWCandidate;

    if (lfLargest == lfWCandidate)
    {
        const f32 lfFour = lfWCandidate * 4.0f;
        const f32 lfScale = 1.0f / std::sqrt(lfFour);
        lResult.x = (lfM21 - lfM12) * lfScale;
        lResult.y = (lfM02 - lfM20) * lfScale;
        lResult.z = (lfM10 - lfM01) * lfScale;
        lResult.w = std::sqrt(lfFour) * 0.25f;
    }
    else if (lfLargest == lfXCandidate)
    {
        const f32 lfFour = lfXCandidate * 4.0f;
        const f32 lfScale = 1.0f / std::sqrt(lfFour);
        lResult.x = std::sqrt(lfFour) * 0.25f;
        lResult.y = (lfM10 + lfM01) * lfScale;
        lResult.z = (lfM20 + lfM02) * lfScale;
        lResult.w = (lfM21 - lfM12) * lfScale;
    }
    else if (lfLargest == lfYCandidate)
    {
        const f32 lfFour = lfYCandidate * 4.0f;
        const f32 lfScale = 1.0f / std::sqrt(lfFour);
        lResult.x = (lfM10 + lfM01) * lfScale;
        lResult.y = std::sqrt(lfFour) * 0.25f;
        lResult.z = (lfM21 + lfM12) * lfScale;
        lResult.w = (lfM02 - lfM20) * lfScale;
    }
    else
    {
        const f32 lfFour = lfZCandidate * 4.0f;
        const f32 lfScale = 1.0f / std::sqrt(lfFour);
        lResult.x = (lfM20 + lfM02) * lfScale;
        lResult.y = (lfM21 + lfM12) * lfScale;
        lResult.z = std::sqrt(lfFour) * 0.25f;
        lResult.w = (lfM10 - lfM01) * lfScale;
    }

    return lResult;
}

// ---------------------------------------------------------------------------------------
// ToMatrix (X360 @ 0x8290A9D8) -- quaternion -> 4x4 rotation matrix (row-major).
//
// ASM: loads the quaternion lanes (qx=idx0, qy=idx1, qz=idx2, qw=idx3), forms the
// doubled products 2x*,2y*,2z*,2w* with the 2.0 constant, then writes the standard
// quaternion rotation matrix. The two UNAMBIGUOUS early stores pin the convention:
//   xAxis.y (idx1) <- 2xy - 2wz      xAxis.z (idx2) <- 2xz + 2wy
// which is the row-major R (v' = R*v); the remaining diagonal/off-diagonal terms follow.
//
// FLAG (optimizer de-obfuscation): the X360 store stream (0x8290AA78..0x8290AB94) is
// heavily register-shuffled -- the same matrix offsets are written several times with
// fmr-renamed temporaries, and the matrix memory is round-tripped as scratch. Those are
// strength-reduced compiler artifacts; the dataflow resolves to exactly the canonical
// quaternion->rotation-matrix assignment below (each computed term -- 1-2y2-2z2, 2xy-2wz,
// 2xz+2wy, 2xy+2wz, 1-2x2-2z2, 2yz-2wx, 2xz-2wy, 2yz+2wx, 1-2x2-2y2 -- is present in the
// asm and matched to its destination lane). The translation row/column are cleared to the
// identity affine (idx3/7/11/12/13/14 = 0, idx15 = 1).
const cQuat& cQuat::ToMatrix(const cQuat& lQuat, Matrix44& lMatrix)
{
    const f32 lfX = lQuat.x;
    const f32 lfY = lQuat.y;
    const f32 lfZ = lQuat.z;
    const f32 lfW = lQuat.w;

    const f32 lf2X = lfX * 2.0f;
    const f32 lf2Y = lfY * 2.0f;
    const f32 lf2Z = lfZ * 2.0f;

    const f32 lf2XX = lfX * lf2X;
    const f32 lf2YY = lfY * lf2Y;
    const f32 lf2ZZ = lfZ * lf2Z;
    const f32 lf2XY = lfY * lf2X;
    const f32 lf2XZ = lfZ * lf2X;
    const f32 lf2YZ = lfZ * lf2Y;
    const f32 lf2WX = lfW * lf2X;
    const f32 lf2WY = lfW * lf2Y;
    const f32 lf2WZ = lfW * lf2Z;

    lMatrix.xAxis.x = 1.0f - (lf2YY + lf2ZZ);   // idx0  = 1 - 2y^2 - 2z^2
    lMatrix.xAxis.y = lf2XY - lf2WZ;            // idx1  = 2xy - 2wz
    lMatrix.xAxis.z = lf2XZ + lf2WY;            // idx2  = 2xz + 2wy
    lMatrix.xAxis.w = 0.0f;                     // idx3

    lMatrix.yAxis.x = lf2XY + lf2WZ;            // idx4  = 2xy + 2wz
    lMatrix.yAxis.y = 1.0f - (lf2XX + lf2ZZ);   // idx5  = 1 - 2x^2 - 2z^2
    lMatrix.yAxis.z = lf2YZ - lf2WX;            // idx6  = 2yz - 2wx
    lMatrix.yAxis.w = 0.0f;                     // idx7

    lMatrix.zAxis.x = lf2XZ - lf2WY;            // idx8  = 2xz - 2wy
    lMatrix.zAxis.y = lf2YZ + lf2WX;            // idx9  = 2yz + 2wx
    lMatrix.zAxis.z = 1.0f - (lf2XX + lf2YY);   // idx10 = 1 - 2x^2 - 2y^2
    lMatrix.zAxis.w = 0.0f;                     // idx11

    lMatrix.wAxis.x = 0.0f;                     // idx12
    lMatrix.wAxis.y = 0.0f;                     // idx13
    lMatrix.wAxis.z = 0.0f;                     // idx14
    lMatrix.wAxis.w = 1.0f;                     // idx15

    return lQuat;
}

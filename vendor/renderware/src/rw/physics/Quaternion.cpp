// =====================================================================================
// rw::physics::Quaternion::UnitQuaternionToMatrix @ 0x82BC3EC0
//
// TRANSCRIPTION BASIS
//   * 0x82BC3EC0 is an EXPORT HOLE (no listing). The body is recovered from the copy the
//     compiler inlined into rw::physics::RigidBody::DynamicUpdate @0x82BC2B78, X360
//     0x82BC2C58..0x82BC2D38, which IS in the export set and was read instruction by
//     instruction.
//   * Corroborated against the two out-of-line witnesses that do exist:
//     BurnoutPR 0x59972D0 (x86/SSE, Hex-Rays `_mm_*`) and
//     Burnout_External_Xbox_One.exe 0x1409B5BE0 (x64/AVX).
//
// ⭐ THE ONE CONSTANT NEEDS NO .rdata READ. X360 loads rw::math::vpu::detail::gSqrt2s and
// forms s = k*q, then builds the diagonal as pairwise sums of (0.5 - s_i^2):
//     (0.5 - s_x^2) + (0.5 - s_y^2) == 1 - 2x^2 - 2y^2
// which holds only for k == sqrt(2). The algebra pins it; no data read is required.
// (It was subsequently read anyway -- the X360 image word at flt_821815B0+0x30 is
// 0x3FB504F3 = 1.41421354 -- and BurnoutPR's xmmword_F0C890 is the same value. Three ways.)
//
// The three output rows are the three COLUMNS of the standard body->world rotation matrix,
// i.e. the right / up / at basis vectors -- exactly what rigidbody.h calls mRi / mUp / mAt.
// Lane-by-lane confirmed against BurnoutPR's explicit row construction.
//
// ⚠️ DE-OPTIMISATION, DELIBERATE: X360 normalises with `vrsqrtefp` plus TWO Newton-Raphson
// refinements (0x82BC2C68..0x82BC2C8C). BurnoutPR and Xbox One both use a true square root.
// Same value; the PC leaf uses the exact form, per the committed precedent for the identical
// idiom in rw::math::vpu::QuaternionFromMatrix33.
// =====================================================================================

#include "rw/physics/quaternion.h"

#include <cmath>   // std::sqrt

namespace rw
{
namespace physics
{

void Quaternion::UnitQuaternionToMatrix(rw::math::vpu::Matrix33* lpDst,
                                        rw::math::vpu::Quaternion* lpQuat)
{
    // Normalise IN PLACE. All three builds do this -- the inlined X360 copy writes the
    // normalised quaternion back over the source at `stvx128 v12, r0, r3`.
    const float lfNormSq = lpQuat->x * lpQuat->x + lpQuat->y * lpQuat->y
                         + lpQuat->z * lpQuat->z + lpQuat->w * lpQuat->w;
    const float lfInvNorm = 1.0f / std::sqrt(lfNormSq);

    lpQuat->x *= lfInvNorm; lpQuat->y *= lfInvNorm;
    lpQuat->z *= lfInvNorm; lpQuat->w *= lfInvNorm;

    const float x = lpQuat->x, y = lpQuat->y, z = lpQuat->z, w = lpQuat->w;

    // X360: v9 = s * s.yzx  -> [2xy, 2yz, 2zx]   (`vpermwi128` imm 0x60 == .yzxx)
    //       v8 = s.w * s.zxy -> [2wz, 2wx, 2wy]  (`vpermwi128` imm 0x84 == .zxyx)
    // BurnoutPR forms exactly [2xy, 2yz, 2zx] and [2wx, 2wy, 2wz].
    const float lfXY = 2.0f * x * y, lfYZ = 2.0f * y * z, lfZX = 2.0f * z * x;
    const float lfWX = 2.0f * w * x, lfWY = 2.0f * w * y, lfWZ = 2.0f * w * z;
    const float lfXX = 2.0f * x * x, lfYY = 2.0f * y * y, lfZZ = 2.0f * z * z;

    // Rows = columns of R. Each of the nine terms below is the literal expression BurnoutPR
    // builds, and the X360 lane routing (`vperm` + `vrlimi128 mask=2` with rotates 0/3/2)
    // selects exactly these.
    lpDst->xAxis.x = 1.0f - lfYY - lfZZ;
    lpDst->xAxis.y = lfXY + lfWZ;
    lpDst->xAxis.z = lfZX - lfWY;

    lpDst->yAxis.x = lfXY - lfWZ;
    lpDst->yAxis.y = 1.0f - lfXX - lfZZ;
    lpDst->yAxis.z = lfYZ + lfWX;

    lpDst->zAxis.x = lfZX + lfWY;
    lpDst->zAxis.y = lfYZ - lfWX;
    lpDst->zAxis.z = 1.0f - lfXX - lfYY;
}

} // namespace physics
} // namespace rw

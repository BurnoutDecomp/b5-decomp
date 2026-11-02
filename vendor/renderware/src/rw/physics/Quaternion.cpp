// =====================================================================================
// rw::physics::Quaternion::UnitQuaternionToMatrix @ 0x82BC3EC0   (116 instructions)
//
// TRANSCRIPTION BASIS -- THE FUNCTION'S OWN WORDS. 0x82BC3EC0 is an export hole (no JSON), so
// it is read straight out of the image: `python tools/re/ppcdis.py 82BC3EC0 116`. The three
// words capstone prints as `.long 0x7C00FC0E / 0x7DA0240E / 0x7D80F40E` are `lvlx` (primary
// 31, XO 519): `lvlx v0,0,r31` / `lvlx v13,0,r4` / `lvlx v12,0,r30`, each followed by
// `vspltw vX,vX,0`. It is SCALAR FPU code with three VMX multiplies, not the VMX builder
// RigidBody::DynamicUpdate inlines. Constants, read from the image (tools/re/x360rd.py):
//     flt_82001D9C = 0x40000000 = 2.0f   stored to -0x60/-0x5C/-0x58(r1), then lvlx + vspltw 0
//     flt_82001C98 = 0x3F800000 = 1.0f
//
//   0x82BC3ECC..3EE4  lfs x, y, z, w <- 0/4/8/0xC(r4)  -- the ONLY accesses through r4, which
//                     is overwritten by `addi r4,r1,-0x5C` at 0x82BC3F3C before any store.
//   0x82BC3F0C..3F1C  fmuls  xx = x*x, yy = y*y, zz = z*z
//   0x82BC3F2C..3F54  fmuls  wx = w*x, wy = y*w, wz = z*w
//   0x82BC3F68..3F78  fmuls  xy = y*x, yz = z*y, zx = z*x
//   0x82BC3F84 / 3FB4 / 3FB8  vmulfp128 of each triple by splat(2.0f):
//                     (2xx, 2yy, 2zz)  (2wx, 2wy, 2wz)  (2xy, 2yz, 2zx)
//   0x82BC3FD0..3FD8  fadds  2zz+2xx, 2yy+2zz, 2yy+2xx          (the three diagonal SUMS)
//   0x82BC4020 / 4030 / 4048  fsubs  1 - (2zz+2xx), 1 - (2yy+2zz), 1 - (2yy+2xx)
//   0x82BC3FF4..405C  fadds/fsubs  the six off-diagonals 2xy -/+ 2wz, 2yz +/- 2wx, 2zx -/+ 2wy
//   0x82BC4074 / 4078 / 4080  stvx128 row1 -> r3+0x10, row0 -> r3+0x00, row2 -> r3+0x20.
//                     Each row is assembled on the stack and its w lane is the `stw r11(=0)` at
//                     0x82BC4040 (row0) / 0x82BC4028 (row1) / 0x82BC4064 (row2).
//
// The three rows are the three COLUMNS of the body->world rotation matrix, i.e. the
// right / up / at basis vectors (rigidbody.h's mRi / mUp / mAt); both jacobian builders use
// them as the constraint frame (JointJacobian_Build.cpp / DriveJacobian_Build.cpp).
//
// ⛔ CORRECTED 2026-09-24 (H2-D1). This body used to NORMALISE the quaternion IN PLACE
// (1/sqrt(|q|^2), written back through the caller's pointer) and built each diagonal as
// (1 - a) - b. Neither is on the console: 0x82BC3EC0 has no vrsqrtefp / fsqrt / fdiv, never
// stores through r4, and forms every diagonal as 1 - (a + b) (the fadds come first). The old
// banner had transcribed this body from the block RigidBody::DynamicUpdate INLINES at
// 0x82BC2C58..0x82BC2D38 -- but that block is a different routine (rwmath's
// Normalize(Quaternion) + Matrix33FromQuaternion, the gSqrt2s form), and DynamicUpdate never
// calls 0x82BC3EC0. DynamicUpdate now carries that normalise and builder itself.
// BurnoutPR 0x59972D0 does normalise (it scales by 2/|q|^2 and writes q/|q| back) -- BPR and
// X360 genuinely differ here; X360 is the target.
//
// No out-of-line caller depended on the old write-back: the PC builders never read their
// quaternion after the call (JointJacobian_Build.cpp:103-106 pass lqA/lqB/lqL/lqRel, last used
// by Jacobian_RQD::Create BEFORE the calls; DriveJacobian_Build.cpp:136 passes a dedicated copy),
// and on the console nothing is written back to read (the drive's var_200 is even fully
// overwritten, 0x82BC588C..0x82BC58A4, before its next load). So the only thing this correction
// changes for them is the matrix itself: built from the composed quaternion as it is (|q| = 1 to
// rounding), in the console's grouping, with determinate zero w lanes.
// =====================================================================================

#include "rw/physics/quaternion.h"

namespace rw
{
namespace physics
{

void Quaternion::UnitQuaternionToMatrix(rw::math::vpu::Matrix33* lpDst,
                                        const rw::math::vpu::Quaternion* lpQuat)
{
    // ARTIST 0x82BC3ECC..3F78 loads the four components as scalar floats
    // and rounds the nine products before the three vector doubles.
    const __vector4 lQuat = __lvx(lpQuat, 0);
    const float x = lQuat.v[0], y = lQuat.v[1], z = lQuat.v[2], w = lQuat.v[3];
    __vector4 squared, spin, cross;
    squared.v[0] = x * x; squared.v[1] = y * y; squared.v[2] = z * z; squared.u[3] = 0;
    spin.v[0] = w * x; spin.v[1] = y * w; spin.v[2] = z * w; spin.u[3] = 0;
    cross.v[0] = y * x; cross.v[1] = z * y; cross.v[2] = z * x; cross.u[3] = 0;
    const __vector4 two = __vcfsx(__vspltisw(2), 0);
    squared = __vmulfp(squared, two);
    spin = __vmulfp(spin, two);
    cross = __vmulfp(cross, two);
    const float xx = squared.v[0], yy = squared.v[1], zz = squared.v[2];
    const float wx = spin.v[0], wy = spin.v[1], wz = spin.v[2];
    const float xy = cross.v[0], yz = cross.v[1], zx = cross.v[2];
    lpDst->xAxis = rw::math::vpu::Vector3(1.0f - (yy + zz), xy + wz, zx - wy);
    lpDst->yAxis = rw::math::vpu::Vector3(xy - wz, 1.0f - (zz + xx), yz + wx);
    lpDst->zAxis = rw::math::vpu::Vector3(zx + wy, yz - wx, 1.0f - (yy + xx));

}

} // namespace physics
} // namespace rw

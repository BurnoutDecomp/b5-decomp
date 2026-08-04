#pragma once

// =====================================================================================
// rw::physics::Quaternion -- the physics module's quaternion helper scope.
//
// The X360 binary carries exactly ONE symbol under this scope:
//     rw::physics::Quaternion::UnitQuaternionToMatrix @ 0x82BC3EC0
// and that name is not a guess: it is the name IDA resolves for the call target in the
// `xrefs_from` of both jacobian builders (0x82BC42E8 JointJacobian::Build, which calls it
// four times, and 0x82BC5590 DriveJacobian::Build, which calls it once).
//
// ⚠️ THE FUNCTION IS AN EXPORT HOLE -- 0x82BC3EC0 has no JSON in the export set, so its own
// listing was never available. The body is recovered from the copy the compiler INLINED into
// RigidBody::DynamicUpdate (X360 0x82BC2C58..0x82BC2D38) and cross-read against the two
// out-of-line witnesses that do exist, BurnoutPR 0x59972D0 and Xbox One 0x1409B5BE0.
// See Quaternion.cpp for the per-term derivation.
//
// SIGNATURE, recovered at the call sites: r3 is the destination matrix and r4 is the
// quaternion -- DriveJacobian::Build @0x82BC56DC passes `r4 = var_200 = qB'`. The quaternion
// is NORMALISED IN PLACE (the inlined copy writes all four components back through the rigid
// body at `stvx128 v12,r0,r3`), so it is taken by pointer, not by const reference.
// =====================================================================================

#include "rw/math/vpu/types.h"   // rw::math::vpu::{Quaternion, Matrix33}

namespace rw
{
namespace physics
{

class Quaternion
{
public:
    // @ 0x82BC3EC0 -- normalise lpQuat in place, then write the body->world rotation basis
    // (right / up / at) into the three rows of lpDst.
    static void UnitQuaternionToMatrix(rw::math::vpu::Matrix33* lpDst,
                                       rw::math::vpu::Quaternion* lpQuat);
};

} // namespace physics
} // namespace rw

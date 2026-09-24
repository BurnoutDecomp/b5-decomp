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
// ⚠️ THE FUNCTION IS AN EXPORT HOLE -- 0x82BC3EC0 has no JSON in the export set. Its own 116
// instructions (.pdata 0x821E8688: start 0x82BC3EC0, info 0x40007402 = 0x74 instructions,
// ending in the `blr` at 0x82BC408C) are read straight from the image with
// tools/re/ppcdis.py. See Quaternion.cpp for the per-instruction transcription.
//
// SIGNATURE, from the words and the five call sites: r3 is the destination matrix and r4 is
// the quaternion. r4 is only READ (`lfs` 0/4/8/0xC) and is repurposed as a stack pointer
// (`addi r4,r1,-0x5C` @0x82BC3F3C) before the body's first store; the only stores that leave
// the frame are the three matrix rows (`stvx128` r3+0x10 / r3 / r3+0x20). So the quaternion
// is taken CONST: this function neither normalises it nor writes it back.
//
// CALLERS (full-image scan of every I-form branch, 2026-09-24): exactly five `bl` --
// JointJacobian::Build 0x82BC44B0 / 0x82BC44BC / 0x82BC44C8 / 0x82BC44D4 and
// DriveJacobian::Build 0x82BC56DC. No address constant builds 0x82BC3EC0 and the only other
// occurrence of the word is its .pdata entry. rw::physics::RigidBody::DynamicUpdate does NOT
// call it: the console inlines rwmath's Normalize(Quaternion) + Matrix33FromQuaternion there
// (RigidBody.cpp), a different routine.
// =====================================================================================

#include "rw/math/vpu/types.h"   // rw::math::vpu::{Quaternion, Matrix33}

namespace rw
{
namespace physics
{

class Quaternion
{
public:
    // @ 0x82BC3EC0 -- write the rotation basis of the (assumed unit) quaternion *lpQuat into the
    // three rows of *lpDst: right / up / at, each row's w lane 0. *lpQuat is only read.
    static void UnitQuaternionToMatrix(rw::math::vpu::Matrix33* lpDst,
                                       const rw::math::vpu::Quaternion* lpQuat);
};

} // namespace physics
} // namespace rw

#pragma once

#include "rw/math/vpu/types.h"   // rw::math::vpu::{Quaternion, Vector3}

// ===========================================================================
// rw::physics::DriveFrames -- the per-drive frame block: the two body-relative orientations
// and the two anchor positions. 64 bytes. The JOINT twin (JointFrames) carries a FIFTH
// quaternion, mQuatL, and lives in JointFrames.hpp.
//
// PROVENANCE
//   NAMES / TYPES / ORDER : DecFIGS DWARF driveframes.h:169..172.
//   ATTESTED ON X360      : all four slots are loaded by DriveJacobian::Build @0x82BC5590 --
//     `lvx128 v0,r0,r27` = +0x00, `lvx128 v12,r27,r25` with r25 = 0x20 = +0x20, and the two
//     position vectors through r11/r8 at +0x10 / +0x30.
// ===========================================================================

namespace rw
{
namespace physics
{
    class DriveFrames
    {
    public:
        const rw::math::vpu::Quaternion& GetChildOrientation() const  { return mQuatA; }
        const rw::math::vpu::Quaternion& GetParentOrientation() const { return mQuatB; }
        const rw::math::vpu::Vector3&    GetChildPosition() const     { return mPosA; }
        const rw::math::vpu::Vector3&    GetParentPosition() const    { return mPosB; }

    private:
        rw::math::vpu::Quaternion mQuatA;   // :169  +0x00  left operand of qA' = bodyA.mQuat (x) mQuatA
        rw::math::vpu::Vector3    mPosA;    // :170  +0x10
        rw::math::vpu::Quaternion mQuatB;   // :171  +0x20  left operand of qB'
        rw::math::vpu::Vector3    mPosB;    // :172  +0x30
    };
}
}

#pragma once

#include "rw/math/vpu/types.h"   // rw::math::vpu::{Quaternion, Vector3}
#include <cstddef>            // offsetof (the layout pins below)

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
        friend void _rw_physics_DriveFrames_AssertLayout();

        rw::math::vpu::Quaternion mQuatA;   // :169  +0x00  left operand of qA' = bodyA.mQuat (x) mQuatA
        rw::math::vpu::Vector3    mPosA;    // :170  +0x10
        rw::math::vpu::Quaternion mQuatB;   // :171  +0x20  left operand of qB'
        rw::math::vpu::Vector3    mPosB;    // :172  +0x30
    };

    // ⭐ SOLE OWNER OF THIS RECORD SINCE 2026-08-04 (task #143). `CgsPhysics::DriveFrames`
    // was a SECOND definition of this record -- a bare `u8 macOpaque[64]` -- and it was the
    // type DriveData's slot table actually held while DriveJacobian::Build read the very same
    // bytes through THIS class. ⛔ Do not re-introduce a local copy: this type reaches the
    // solver as a bare `void*` through Simulation::AddDrive, so the seam is invisible to both
    // the compiler and the linker. See the block at CgsPhysicsSimulationModule.h.
    inline void _rw_physics_DriveFrames_AssertLayout()
    {
        static_assert(offsetof(DriveFrames, mQuatA) == 0x00, "mQuatA @+0x00 (DriveJacobian::Build lvx128 v0,r0,r27)");
        static_assert(offsetof(DriveFrames, mPosA)  == 0x10, "mPosA  @+0x10 (Build reads the position through r11)");
        static_assert(offsetof(DriveFrames, mQuatB) == 0x20, "mQuatB @+0x20 (Build lvx128 v12,r27,r25 with r25 = 0x20)");
        static_assert(offsetof(DriveFrames, mPosB)  == 0x30, "mPosB  @+0x30 (Build reads the position through r8)");

        // Adjacency form -- survives a member WIDENING, which the absolute offsets would too
        // but which the stride check below would not.
        static_assert(offsetof(DriveFrames, mPosA)  == offsetof(DriveFrames, mQuatA) + sizeof(DriveFrames::mQuatA), "mPosA follows mQuatA");
        static_assert(offsetof(DriveFrames, mQuatB) == offsetof(DriveFrames, mPosA)  + sizeof(DriveFrames::mPosA),  "mQuatB follows mPosA");
        static_assert(offsetof(DriveFrames, mPosB)  == offsetof(DriveFrames, mQuatB) + sizeof(DriveFrames::mQuatB), "mPosB follows mQuatB");

        // The X360-attested ARRAY STRIDE -- ProcessUpdateDriveFramesQueue @0x8289FD18 steps
        // DriveData::maFrames by `liIndex << 6`, and copies exactly four 16-byte lanes.
        static_assert(sizeof(DriveFrames) == 64, "DriveFrames array stride 64 (drain slwi r10,r31,6 + 4 lvx128 lanes)");
    }
}
}

#pragma once

#include "types.hpp"             // f32
#include "rw/math/vpu/types.h"   // rw::math::vpu::Vector3

// ===========================================================================
// rw::physics::JointLimits -- the limit block a RenderWare physics JOINT points at:
// the prismatic (linear) position/velocity box, the swing/twist angular limits, and the
// two enums that pick which of the five swing and three twist behaviours the solver runs.
//
// PROVENANCE
//   NAMES / TYPES / ORDER : DecFIGS DWARF jointlimits.h:74 / :98 / :244..253.
//   ATTESTED ON X360      : rw::physics::JointJacobian::Build @0x82BC42E8 reads
//     `lfs 0x20(r28)` mVtwist, `lfs 0x24(r28)` mVswing, `lfs 0x30(r28)` mSwingc,
//     `lfs 0x34(r28)` mTwistc, `lwz 0x38(r28)` -> a 5-way switch = SwingType, and
//     `lwz 0x3C(r28)` -> a 3-way switch = TwistType. mPprism/mVprism are read as whole
//     vectors at +0x00/+0x10 by the prismatic box (0x82BC4690..0x82BC46D8).
//   mSwinga / mTwista are DWARF-declared but NOT touched by Build -- name/type trusted,
//   use unattested.
//
// ⭐ THE THREE ANGULAR LANES ARE NAMED BY THE DATA, NOT BY GUESSWORK. Build assembles the
// angular velocity limit as the vector (mVtwist, mVswing, mVswing) -- X360 `lfs 0x20/0x24`
// into three stack slots, BurnoutPR an `unpcklps` chain over [ecx+20h]/[ecx+24h].
// ⇒ lane x = TWIST, lanes y and z = the two SWING rows. Everything downstream follows.
// ===========================================================================

namespace rw
{
namespace physics
{
    // jointlimits.h:74. The 5-way dispatch at X360 0x82BC470C..0x82BC4734 IS this enum.
    enum SwingType
    {
        SWING_LOCKED = 0,
        SWING_CONE   = 1,
        SWING_HINGE  = 2,
        SWING_AXLE   = 3,
        SWING_FREE   = 4
    };

    // jointlimits.h:98. The 3-way dispatch on `lwz r26,0x3C(r28)`.
    enum TwistType
    {
        TWIST_LOCKED = 0,
        TWIST_ARC    = 1,
        TWIST_FREE   = 2
    };

    class JointLimits
    {
    public:
        const rw::math::vpu::Vector3& GetPositionLimit() const       { return mPprism; }
        const rw::math::vpu::Vector3& GetLinearVelocityLimit() const { return mVprism; }
        SwingType  GetSwingType() const   { return mSwingf; }
        TwistType  GetTwistType() const   { return mTwistf; }
        const f32& GetSwingAngle() const  { return mSwinga; }
        const f32& GetTwistAngle() const  { return mTwista; }
        const f32& GetSwingLimit() const  { return mSwingc; }
        const f32& GetTwistLimit() const  { return mTwistc; }

        // jointlimits.h:194 -- the SDK's own accessor, and the one that names the lanes.
        rw::math::vpu::Vector3 GetAngularVelocityLimit() const
        { rw::math::vpu::Vector3 r = { mVtwist, mVswing, mVswing, 0.0f }; return r; }

    private:
        rw::math::vpu::Vector3 mPprism;   // :244  +0x00  position limit
        rw::math::vpu::Vector3 mVprism;   // :245  +0x10  linear velocity limit
        f32       mVtwist;                // :246  +0x20
        f32       mVswing;                // :247  +0x24
        f32       mSwinga;                // :248  +0x28  current swing angle -- not read by Build
        f32       mTwista;                // :249  +0x2C  current twist angle -- not read by Build
        f32       mSwingc;                // :250  +0x30  swing limit
        f32       mTwistc;                // :251  +0x34  twist limit
        SwingType mSwingf;                // :252  +0x38
        TwistType mTwistf;                // :253  +0x3C
    };
}
}

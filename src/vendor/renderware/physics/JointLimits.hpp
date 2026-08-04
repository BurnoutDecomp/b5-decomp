#pragma once

#include <cstddef>               // offsetof (the layout pins below)

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
        // jointlimits.h:138. X360-ATTESTED BODY, and the attribution is a DEDUCTION rather than
        // a guess. CgsPhysics::JointData::JointData @0x827DB798 runs a 36-pass loop over
        // &maLimits[0] that, per 64-byte record, zeroes mPprism/mVprism (two `stvx128` of a
        // zeroed vector), zeroes mVtwist/mVswing/mSwinga/mTwista (`stfs f0` at 0x20/0x24/0x28/
        // 0x2C), stores **1.0f** into mSwingc/mTwistc (`stfs f13` at 0x30/0x34) and zeroes both
        // enum slots (`stw r10` at 0x38/0x3C).
        // ⭐ WHY THOSE STORES BELONG HERE AND NOT TO JointData: the DWARF declares setters for
        // every other member -- SetPositionLimit, SetLinearVelocityLimit, SetSwingAngle,
        // SetTwistAngle, SetTwistVelocityLimit, SetSwingVelocityLimit, SetSwingType,
        // SetTwistType -- but declares **no SetSwingLimit and no SetTwistLimit**, and
        // GetSwingLimit/GetTwistLimit return `const float32_t&`. mSwingc/mTwistc are therefore
        // unreachable from outside the class, so the only DWARF-declared function that can
        // write the console's 1.0f is this constructor. ⇒ the loop is this body, inlined.
        // Same footing as DriveDynamics::Params::Params() (task #143).
        JointLimits()
            : mPprism{ 0.0f, 0.0f, 0.0f, 0.0f }
            , mVprism{ 0.0f, 0.0f, 0.0f, 0.0f }
            , mVtwist(0.0f)
            , mVswing(0.0f)
            , mSwinga(0.0f)
            , mTwista(0.0f)
            , mSwingc(1.0f)
            , mTwistc(1.0f)
            , mSwingf(SWING_LOCKED)
            , mTwistf(TWIST_LOCKED)
        {}

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
        friend void _rw_physics_JointLimits_AssertLayout();

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

    // ⭐ SOLE OWNER OF THIS RECORD SINCE 2026-08-04 (task #144). `CgsPhysics::JointLimits` was a
    // SECOND definition of it, with its own re-spelled fields (`mafPprism[4]`, `mfVtwist`,
    // `meSwingf`...) and its own `E_SwingType`/`E_TwistType` enums -- byte-identical, so correct
    // BY LUCK, exactly as the DriveDynamics fork task #143 retired was. JointData's slot table
    // held the fork while JointJacobian::Build read the same bytes through THIS class
    // (`const JointLimits& lrLim = *lrJoint.m_limit;`, JointJacobian_Build.cpp:75).
    // ⛔ Do not re-introduce a local copy -- Simulation::AddJoint carries this as a bare `void*`.
    //
    // ⭐⭐ THE SHIPPED BINARY ITSELF NAMES THIS CLASS'S API. ProcessAddJointQueue @0x828A40F0
    // carries SEVEN validation asserts spelled `RwMathVPU::IsValid( lpLimits->GetXxx() )` --
    // GetAngularVelocityLimit (.cpp:1535), GetLinearVelocityLimit (:1536), GetPositionLimit
    // (:1537), GetTwistLimit (:1538), GetTwistAngle (:1539), GetSwingLimit (:1540),
    // GetSwingAngle (:1541). Not one of those accessors exists on the fork.
    // ⭐ AND THE :1535 BLOCK RE-PROVES `GetAngularVelocityLimit`'s BODY: its inlined check is
    // THREE lanes fed by exactly TWO `lfs` loads -- i.e. a 3-lane vector built from two
    // scalars, which is precisely the committed `{ mVtwist, mVswing, mVswing }`.
    inline void _rw_physics_JointLimits_AssertLayout()
    {
        static_assert(offsetof(JointLimits, mPprism) == 0x00, "mPprism @+0x00 (Build prismatic box 0x82BC4690)");
        static_assert(offsetof(JointLimits, mVprism) == 0x10, "mVprism @+0x10");
        static_assert(offsetof(JointLimits, mVtwist) == 0x20, "mVtwist @+0x20 (Build `lfs 0x20(r28)`)");
        static_assert(offsetof(JointLimits, mVswing) == 0x24, "mVswing @+0x24 (Build `lfs 0x24(r28)`)");
        static_assert(offsetof(JointLimits, mSwinga) == 0x28, "mSwinga @+0x28");
        static_assert(offsetof(JointLimits, mTwista) == 0x2C, "mTwista @+0x2C");
        static_assert(offsetof(JointLimits, mSwingc) == 0x30, "mSwingc @+0x30 (Build `lfs 0x30(r28)`)");
        static_assert(offsetof(JointLimits, mTwistc) == 0x34, "mTwistc @+0x34 (Build `lfs 0x34(r28)`)");
        static_assert(offsetof(JointLimits, mSwingf) == 0x38, "mSwingf @+0x38 (Build `lwz 0x38(r28)` -> 5-way switch)");
        static_assert(offsetof(JointLimits, mTwistf) == 0x3C, "mTwistf @+0x3C (Build `lwz 0x3C(r28)` -> 3-way switch)");

        // Adjacency form -- survives a member WIDENING.
        static_assert(offsetof(JointLimits, mVprism) == offsetof(JointLimits, mPprism) + sizeof(JointLimits::mPprism), "mVprism follows mPprism");
        static_assert(offsetof(JointLimits, mVtwist) == offsetof(JointLimits, mVprism) + sizeof(JointLimits::mVprism), "mVtwist follows mVprism");
        static_assert(offsetof(JointLimits, mVswing) == offsetof(JointLimits, mVtwist) + sizeof(JointLimits::mVtwist), "mVswing follows mVtwist");
        static_assert(offsetof(JointLimits, mSwinga) == offsetof(JointLimits, mVswing) + sizeof(JointLimits::mVswing), "mSwinga follows mVswing");
        static_assert(offsetof(JointLimits, mTwista) == offsetof(JointLimits, mSwinga) + sizeof(JointLimits::mSwinga), "mTwista follows mSwinga");
        static_assert(offsetof(JointLimits, mSwingc) == offsetof(JointLimits, mTwista) + sizeof(JointLimits::mTwista), "mSwingc follows mTwista");
        static_assert(offsetof(JointLimits, mTwistc) == offsetof(JointLimits, mSwingc) + sizeof(JointLimits::mSwingc), "mTwistc follows mSwingc");
        static_assert(offsetof(JointLimits, mSwingf) == offsetof(JointLimits, mTwistc) + sizeof(JointLimits::mTwistc), "mSwingf follows mTwistc");
        static_assert(offsetof(JointLimits, mTwistf) == offsetof(JointLimits, mSwingf) + sizeof(JointLimits::mSwingf), "mTwistf follows mSwingf");

        // The X360-attested ARRAY STRIDE. ProcessUpdateJointLimitsQueue @0x8289F724 steps
        // JointData::maLimits by `addi r10,r31,0x2D / slwi r10,r10,6` == (i+45)*64, i.e. base
        // +0x0B40 with a 64-byte stride, and copies exactly EIGHT 8-byte `ld/std` pairs.
        static_assert(sizeof(JointLimits) == 64, "JointLimits array stride 64 (drain (i+45)*64 + EIGHT ld/std)");
    }
}
}

#pragma once

#include <cmath>   // std::sqrt (QuaternionFromMatrix33)
#include <cstddef> // offsetof (the layout pins below)

#include "rw/math/vpu/types.h"   // rw::math::vpu::{Vector3, Matrix33}

// ===========================================================================
// rw::physics::JointFrames -- the per-joint frame block a RenderWare physics
// constraint carries: the child/parent angular frames and the parent linear
// frame as quaternions, plus the two anchor positions.
//
// OWNING HOME (JointFrames.cpp) for the three functions the X360 binary
// defines in this TU (all callers:
// BrnPhysics::Vehicle::ArticulatedJointPool::ConstructArticulatedJoint):
//     rw::physics::JointFrames::SetChildAngularFrame(Matrix33)   @ 0x8259B580
//     rw::physics::JointFrames::SetParentAngularFrame(Matrix33)  @ 0x8259B728
//     rw::physics::JointFrames::SetParentLinearFrame(Matrix33)   @ 0x8259B8D0
//
// LAYOUT + SIGNATURES: DWARF-authoritative (references/DecFIGS/dwarfdump/
// SDKs/EATech/include/cmn/rw/physics/jointframes.h -- setters :136/:144/:152,
// members :166-170: mQuatA +0x00, mPosA +0x10, mQuatB +0x20, mPosB +0x30,
// mQuatL +0x40; 0x50 bytes, 16-aligned). The Quaternion overloads / getters /
// position setters were fully inlined away by the console build -- declared
// and defined inline from the Feb-2007 rwpcore.h:504-623 originals for
// vocabulary.
//
// NAMESPACE-GROW NOTE: rw::math::vpu::Quaternion USED to be defined here, with
// a note saying "move it to the vendor rw/math/vpu/types.h on the next grow of
// that header". ⭐ 2026-08-04: that move HAS HAPPENED (the rw physics solver
// landing needed the same type for RigidBody::mQuat, DriveFrames and the two
// jacobian builders), so the definition now lives in rw/math/vpu/types.h and is
// picked up through the include above. QuaternionFromMatrix33 stays here until
// rw/math/vpu/quaternion_operation.h exists; it has no second user yet.
// ===========================================================================

namespace rw
{
namespace math
{
namespace vpu
{
    // rw::math::vpu::QuaternionFromMatrix33 -- inlined on the X360 into all
    // three JointFrames setters (e.g. 0x8259B580). GROUND TRUTH: the EXACT
    // original VMX source survives in the Feb-2007 tree (rwmath 1.02.00
    // rw/math/vpu/detail/quaternion_operation_inline.h:282-382 -- matches the
    // X360 body instruction-for-instruction: vspltw x/y/z diagonals, the
    // three gQuatFromMat_{x,y,z}Signs vxor masks unk_8327F120/F100/F0F0,
    // vaddfp fold, vrsqrtefp + two Newton-Raphson refines, s = t*rsqrt /
    // s2 = 0.5*rsqrt, the two vperm+vsldoi(8) off-diagonal gathers
    // (unk_82CDA360..430 lane controls), N = sub / S = add, four scaled
    // candidates, three vsel selects). The body below is the SDK's own
    // scalar branch twin (fpu/quaternion_operation.h:802-853, T = float),
    // whose branch tree is exactly the console's vsel network:
    //     mask3 = (trace > epsilon)              -> Q0 (w-largest)
    //     mask2 = (xx > yy) & (xx > zz)          -> Q1 (x-largest)
    //     mask1 = (yy > zz)                      -> Q2 else Q3
    // The vrsqrtefp + 2x Newton-Raphson is de-optimised to the exact
    // std::sqrt per the committed precedent. epsilon (unk_8208C24C, not
    // byte-dumped in the extract) is the DEFAULTED argument
    // `VecFloat(ZERO_FLOAT)` (vpu quaternion_operation.h:133; the fpu twin
    // defaults `epsilon = (T)(0.0f)`), and the JointFrames setters pass no
    // epsilon -- so 0.0f. Canonical home: rw/math/vpu/quaternion_operation.h.
    inline Quaternion QuaternionFromMatrix33(const Matrix33& m, float epsilon = 0.0f)
    {
        const float tr = m.xAxis.x + m.yAxis.y + m.zAxis.z;

        if (tr > epsilon)
        {
            // Q0 (w-largest): t = 1 + trace = 4w^2
            const float s  = std::sqrt(tr + 1.0f);
            const float s2 = 0.5f / s;
            return Quaternion{ (m.yAxis.z - m.zAxis.y) * s2,
                               (m.zAxis.x - m.xAxis.z) * s2,
                               (m.xAxis.y - m.yAxis.x) * s2,
                               0.5f * s };
        }
        if ((m.xAxis.x > m.yAxis.y) && (m.xAxis.x > m.zAxis.z))
        {
            // Q1 (x-largest): t = 1 + xx - yy - zz = 4x^2
            const float s  = std::sqrt((m.xAxis.x - (m.yAxis.y + m.zAxis.z)) + 1.0f);
            const float s2 = 0.5f / s;
            return Quaternion{ 0.5f * s,
                               (m.yAxis.x + m.xAxis.y) * s2,
                               (m.xAxis.z + m.zAxis.x) * s2,
                               (m.yAxis.z - m.zAxis.y) * s2 };
        }
        if (m.yAxis.y > m.zAxis.z)
        {
            // Q2 (y-largest): t = 1 - xx + yy - zz = 4y^2
            const float s  = std::sqrt((m.yAxis.y - (m.zAxis.z + m.xAxis.x)) + 1.0f);
            const float s2 = 0.5f / s;
            return Quaternion{ (m.yAxis.x + m.xAxis.y) * s2,
                               0.5f * s,
                               (m.zAxis.y + m.yAxis.z) * s2,
                               (m.zAxis.x - m.xAxis.z) * s2 };
        }
        // Q3 (z-largest): t = 1 - xx - yy + zz = 4z^2
        const float s  = std::sqrt((m.zAxis.z - (m.xAxis.x + m.yAxis.y)) + 1.0f);
        const float s2 = 0.5f / s;
        return Quaternion{ (m.xAxis.z + m.zAxis.x) * s2,
                           (m.zAxis.y + m.yAxis.z) * s2,
                           0.5f * s,
                           (m.xAxis.y - m.yAxis.x) * s2 };
    }
}
}

namespace physics
{
    class JointFrames
    {
    public:
        JointFrames() {}
        ~JointFrames() {}

        // The three out-of-line X360 bodies of this TU (JointFrames.cpp):
        void SetChildAngularFrame(const rw::math::vpu::Matrix33& m);   // @ 0x8259B580 -> mQuatA
        void SetParentAngularFrame(const rw::math::vpu::Matrix33& m);  // @ 0x8259B728 -> mQuatB
        void SetParentLinearFrame(const rw::math::vpu::Matrix33& m);   // @ 0x8259B8D0 -> mQuatL

        // Inline-only siblings (Feb-2007 rwpcore.h:504-623 originals; no X360
        // standalone symbol -- fully inlined away by the console build):
        void SetChildAngularFrame(const rw::math::vpu::Quaternion& q)  { mQuatA = q; }
        void SetParentAngularFrame(const rw::math::vpu::Quaternion& q) { mQuatB = q; }
        void SetParentLinearFrame(const rw::math::vpu::Quaternion& q)  { mQuatL = q; }
        void SetChildPosition(const rw::math::vpu::Vector3& p)  { mPosA = p; }
        void SetParentPosition(const rw::math::vpu::Vector3& p) { mPosB = p; }
        rw::math::vpu::Quaternion GetChildAngularFrame() const  { return mQuatA; }
        rw::math::vpu::Quaternion GetParentAngularFrame() const { return mQuatB; }
        rw::math::vpu::Quaternion GetParentLinearFrame() const  { return mQuatL; }
        rw::math::vpu::Vector3 GetChildPosition() const  { return mPosA; }
        rw::math::vpu::Vector3 GetParentPosition() const { return mPosB; }

    private:
        friend void _rw_physics_JointFrames_AssertLayout();

        // DWARF member sequence (jointframes.h:166-170):
        rw::math::vpu::Quaternion mQuatA;   // +0x00  child angular frame
        rw::math::vpu::Vector3    mPosA;    // +0x10  child position
        rw::math::vpu::Quaternion mQuatB;   // +0x20  parent angular frame
        rw::math::vpu::Vector3    mPosB;    // +0x30  parent position
        rw::math::vpu::Quaternion mQuatL;   // +0x40  parent linear frame
    };

    // ⭐ SOLE OWNER OF THIS RECORD SINCE 2026-08-04 (task #144). `CgsPhysics::JointFrames` was a
    // SECOND definition of it -- a bare `u8 macOpaque[80]` -- and it was the type JointData's
    // slot table actually held while JointJacobian::Build read the very same bytes through THIS
    // class (`const JointFrames& lrF = *lrJoint.m_skel;`, JointJacobian_Build.cpp:74). Exactly
    // the DriveFrames fork task #143 retired, one group over.
    //
    // ⛔ Do not re-introduce a local copy: this type reaches the solver as a bare `void*`
    // through Simulation::AddJoint, so that seam is invisible to BOTH the compiler and the
    // linker -- a `void*` in a reconstructed signature is a fork detector switched off. See
    // [[odr-forks-link-silently]] and the block at CgsPhysicsSimulationModule.h.
    //
    // ⭐⭐ THE SHIPPED BINARY ITSELF NAMES THIS CLASS'S API. PhysicsSimulationModule::
    // ProcessAddJointQueue @0x828A40F0 carries four validation asserts whose literals are
    // `RwMathVPU::IsValid( lpFrames->GetChildAngularFrame() )` (.cpp:1529),
    // `...GetParentAngularFrame() )` (:1530), `...GetParentLinearFrame() )` (:1531) and
    // `...GetParentPosition() )` (:1532) -- accessors that exist HERE and nowhere on the fork.
    // Their inlined lane counts type each slot outright: 4x `vspltw`+`vcmpeqfp.` for the three
    // quaternions, 3x for the position. ⚠️ `GetChildPosition()` is the one slot the console
    // does NOT validate -- transcribed as shipped, not "completed".
    inline void _rw_physics_JointFrames_AssertLayout()
    {
        static_assert(offsetof(JointFrames, mQuatA) == 0x00, "mQuatA @+0x00 (JointJacobian::Build lvx128 over [r26])");
        static_assert(offsetof(JointFrames, mPosA)  == 0x10, "mPosA  @+0x10");
        static_assert(offsetof(JointFrames, mQuatB) == 0x20, "mQuatB @+0x20");
        static_assert(offsetof(JointFrames, mPosB)  == 0x30, "mPosB  @+0x30");
        static_assert(offsetof(JointFrames, mQuatL) == 0x40, "mQuatL @+0x40 (Build r25 = 0x40 over body B; BurnoutPR [esi+40h])");

        // Adjacency form -- survives a member WIDENING, which the stride check below would not.
        static_assert(offsetof(JointFrames, mPosA)  == offsetof(JointFrames, mQuatA) + sizeof(JointFrames::mQuatA), "mPosA follows mQuatA");
        static_assert(offsetof(JointFrames, mQuatB) == offsetof(JointFrames, mPosA)  + sizeof(JointFrames::mPosA),  "mQuatB follows mPosA");
        static_assert(offsetof(JointFrames, mPosB)  == offsetof(JointFrames, mQuatB) + sizeof(JointFrames::mQuatB), "mPosB follows mQuatB");
        static_assert(offsetof(JointFrames, mQuatL) == offsetof(JointFrames, mPosB)  + sizeof(JointFrames::mPosB),  "mQuatL follows mPosB");

        // The X360-attested ARRAY STRIDE. ProcessUpdateJointFramesQueue @0x8289F4E8 steps
        // JointData::maFrames by `slwi r10,r31,2 / add r10,r31,r10 / slwi r10,r10,4` == i*5*16
        // == i*80, and copies exactly FIVE 16-byte lanes at 0/0x10/0x20/0x30/0x40. The drive
        // sibling does four at a stride of 64 -- this record is one quaternion larger.
        static_assert(sizeof(JointFrames) == 80, "JointFrames array stride 80 (drain i*80 + FIVE lvx128 lanes)");
    }
}
}

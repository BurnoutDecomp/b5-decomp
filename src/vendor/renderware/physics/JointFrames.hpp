#pragma once

#include <vectorintrinsics.h>
#include "types.hpp" // original sign XOR and bit-select
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
    // ARTIST 8259B580/8259B728/8259B8D0. Keep all four candidates, both
    // unguarded reciprocal-square-root refinements and the original strict selects.
    inline Quaternion QuaternionFromMatrix33(const Matrix33& m, float epsilon = 0.0f)
    {
        // Sign masks are initialized by ARTIST 82C74000/30/60; the permute
        // controls are the original words at 82CDA360..82CDA430.
        static const CGS_ALIGN(16) u32 kaSignX[4] = { 0, 0, 0x80000000u, 0x80000000u };
        static const CGS_ALIGN(16) u32 kaSignY[4] = { 0, 0x80000000u, 0, 0x80000000u };
        static const CGS_ALIGN(16) u32 kaSignZ[4] = { 0, 0x80000000u, 0x80000000u, 0 };
        static const CGS_ALIGN(16) u32 kaGatherX[4] = { 0x04050607, 0x10111213, 0x00010203, 0x00010203 };
        static const CGS_ALIGN(16) u32 kaGatherYZ[4] = { 0x00010203, 0x00010203, 0x08090A0B, 0x10111213 };
        static const CGS_ALIGN(16) u32 kaGatherZX[4] = { 0x00010203, 0x00010203, 0x04050607, 0x18191A1B };
        static const CGS_ALIGN(16) u32 kaGatherYZero[4] = { 0x00010203, 0x10111213, 0x00010203, 0x00010203 };
        static const CGS_ALIGN(16) u32 kaWeightTrace[4] = { 0x10111213, 0x10111213, 0x10111213, 0x00010203 };
        static const CGS_ALIGN(16) u32 kaWeightX[4] = { 0x04050607, 0x14151617, 0x14151617, 0x14151617 };
        static const CGS_ALIGN(16) u32 kaWeightY[4] = { 0x18191A1B, 0x08090A0B, 0x18191A1B, 0x18191A1B };
        static const CGS_ALIGN(16) u32 kaWeightZ[4] = { 0x1C1D1E1F, 0x1C1D1E1F, 0x0C0D0E0F, 0x1C1D1E1F };
        static const CGS_ALIGN(16) u32 kaCandidateX[4] = { 0x0C0D0E0F, 0x18191A1B, 0x14151617, 0x00010203 };
        static const CGS_ALIGN(16) u32 kaCandidateY[4] = { 0x18191A1B, 0x0C0D0E0F, 0x10111213, 0x04050607 };
        static const CGS_ALIGN(16) u32 kaCandidateZ[4] = { 0x14151617, 0x10111213, 0x0C0D0E0F, 0x08090A0B };

        const __vector4 rowX = __lvx(&m.xAxis, 0);
        const __vector4 rowY = __lvx(&m.yAxis, 0);
        const __vector4 rowZ = __lvx(&m.zAxis, 0);
        const __vector4 diagonalX = __vspltw(rowX, 0);
        const __vector4 diagonalY = __vspltw(rowY, 1);
        const __vector4 diagonalZ = __vspltw(rowZ, 2);
        const __vector4 one = __vcfsx(__vspltisw(1), 0);
        const __vector4 half = __vcfsx(__vspltisw(1), 1);
        const __vector4 zero = __vspltisw(0);
        const __vector4 signedXY = __vaddfp(__vxor(diagonalX, __lvx(kaSignX, 0)),
                                           __vxor(diagonalY, __lvx(kaSignY, 0)));
        const __vector4 terms = __vaddfp(signedXY,
            __vaddfp(__vxor(diagonalZ, __lvx(kaSignZ, 0)), one));
        __vector4 inverseRoots = __vrsqrtefp(terms);
        for (int step = 0; step < 2; ++step)
        {
            const __vector4 squared = __vmulfp(inverseRoots, inverseRoots);
            const __vector4 halfEstimate = __vmulfp(inverseRoots, half);
            const __vector4 residual = __vnmsubfp(terms, squared, one);
            inverseRoots = __vmaddfp(halfEstimate, residual, inverseRoots);
        }
        const __vector4 roots = __vmulfp(terms, inverseRoots);
        const __vector4 scales = __vmulfp(half, inverseRoots);
        const __vector4 gatherX = __vperm(rowX, half, __lvx(kaGatherX, 0));
        const __vector4 gatherYZ = __vperm(rowY, rowZ, __lvx(kaGatherYZ, 0));
        const __vector4 gatherZX = __vperm(rowZ, rowX, __lvx(kaGatherZX, 0));
        const __vector4 gatherYZero = __vperm(rowY, zero, __lvx(kaGatherYZero, 0));
        const __vector4 upper = __vsldoi(gatherYZ, gatherX, 8);
        const __vector4 lower = __vsldoi(gatherZX, gatherYZero, 8);
        const __vector4 difference = __vsubfp(upper, lower);
        const __vector4 sum = __vaddfp(upper, lower);
        const __vector4 candidateTrace = __vmulfp(difference,
            __vperm(roots, scales, __lvx(kaWeightTrace, 0)));
        const __vector4 candidateX = __vmulfp(__vperm(difference, sum, __lvx(kaCandidateX, 0)),
            __vperm(roots, scales, __lvx(kaWeightX, 0)));
        const __vector4 candidateY = __vmulfp(__vperm(difference, sum, __lvx(kaCandidateY, 0)),
            __vperm(roots, scales, __lvx(kaWeightY, 0)));
        const __vector4 candidateZ = __vmulfp(__vperm(difference, sum, __lvx(kaCandidateZ, 0)),
            __vperm(roots, scales, __lvx(kaWeightZ, 0)));
        __vector4 selected = __vsel(candidateZ, candidateY, __vcmpgtfp(diagonalY, diagonalZ));
        selected = __vsel(selected, candidateX,
            __vand(__vcmpgtfp(diagonalX, diagonalY), __vcmpgtfp(diagonalX, diagonalZ)));
        const __vector4 trace = __vspltw(__vaddfp(signedXY, diagonalZ), 0);
        const __vector4 threshold = { epsilon, epsilon, epsilon, epsilon };
        selected = __vsel(selected, candidateTrace, __vcmpgtfp(trace, threshold));
        Quaternion result;
        __stvx(selected, &result, 0);
        return result;
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
        CGS_STATIC_ASSERT((offsetof(JointFrames, mQuatA) == 0x00), "mQuatA @+0x00 (JointJacobian::Build lvx128 over [r26])");
        CGS_STATIC_ASSERT((offsetof(JointFrames, mPosA)  == 0x10), "mPosA  @+0x10");
        CGS_STATIC_ASSERT((offsetof(JointFrames, mQuatB) == 0x20), "mQuatB @+0x20");
        CGS_STATIC_ASSERT((offsetof(JointFrames, mPosB)  == 0x30), "mPosB  @+0x30");
        CGS_STATIC_ASSERT((offsetof(JointFrames, mQuatL) == 0x40), "mQuatL @+0x40 (Build r25 = 0x40 over body B; BurnoutPR [esi+40h])");

        // Adjacency form -- survives a member WIDENING, which the stride check below would not.
        CGS_STATIC_ASSERT((offsetof(JointFrames, mPosA)  == offsetof(JointFrames, mQuatA) + sizeof(((JointFrames*)0)->mQuatA)), "mPosA follows mQuatA");
        CGS_STATIC_ASSERT((offsetof(JointFrames, mQuatB) == offsetof(JointFrames, mPosA)  + sizeof(((JointFrames*)0)->mPosA)),  "mQuatB follows mPosA");
        CGS_STATIC_ASSERT((offsetof(JointFrames, mPosB)  == offsetof(JointFrames, mQuatB) + sizeof(((JointFrames*)0)->mQuatB)), "mPosB follows mQuatB");
        CGS_STATIC_ASSERT((offsetof(JointFrames, mQuatL) == offsetof(JointFrames, mPosB)  + sizeof(((JointFrames*)0)->mPosB)),  "mQuatL follows mPosB");

        // The X360-attested ARRAY STRIDE. ProcessUpdateJointFramesQueue @0x8289F4E8 steps
        // JointData::maFrames by `slwi r10,r31,2 / add r10,r31,r10 / slwi r10,r10,4` == i*5*16
        // == i*80, and copies exactly FIVE 16-byte lanes at 0/0x10/0x20/0x30/0x40. The drive
        // sibling does four at a stride of 64 -- this record is one quaternion larger.
        CGS_STATIC_ASSERT((sizeof(JointFrames) == 80), "JointFrames array stride 80 (drain i*80 + FIVE lvx128 lanes)");
    }
}
}

#pragma once

#include <cmath>   // VMX arithmetic and reciprocal-square-root estimate model
#include "types.hpp" // original sign XOR and bit-select
#include <cstring> // float-word transfers
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
    namespace QuaternionFromMatrix33Detail
    {
        inline float Flush(float lfValue)
        {
            return std::fpclassify(lfValue) == FP_SUBNORMAL
                ? std::copysign(0.0f, lfValue) : lfValue;
        }

        inline float Add(float lfA, float lfB) { return Flush(Flush(lfA) + Flush(lfB)); }
        inline float Subtract(float lfA, float lfB) { return Flush(Flush(lfA) - Flush(lfB)); }
        inline float Multiply(float lfA, float lfB) { return Flush(Flush(lfA) * Flush(lfB)); }
        inline float MultiplyAdd(float lfA, float lfC, float lfB)
        {
            return Flush(std::fmaf(Flush(lfA), Flush(lfC), Flush(lfB)));
        }
        inline float NegativeMultiplySubtract(float lfA, float lfC, float lfB)
        {
            const float lfDifference = Flush(std::fmaf(Flush(lfA), Flush(lfC), -Flush(lfB)));
            return std::isnan(lfDifference) ? lfDifference : -lfDifference;
        }
        inline float FlipSign(float lfValue)
        {
            uint32_t luWord;
            std::memcpy(&luWord, &lfValue, sizeof(luWord));
            luWord ^= 0x80000000u;
            std::memcpy(&lfValue, &luWord, sizeof(lfValue));
            return lfValue;
        }
        inline float Select(float lfA, float lfB, bool lbSelectB)
        {
            uint32_t luA, luB;
            std::memcpy(&luA, &lfA, sizeof(luA));
            std::memcpy(&luB, &lfB, sizeof(luB));
            const uint32_t luMask = 0u - static_cast<uint32_t>(lbSelectB);
            const uint32_t luResult = (luA & ~luMask) | (luB & luMask);
            float lfResult;
            std::memcpy(&lfResult, &luResult, sizeof(lfResult));
            return lfResult;
        }
        inline Quaternion Select(Quaternion lA, Quaternion lB, bool lbSelectB)
        {
            { const Quaternion lResult = {Select(lA.x, lB.x, lbSelectB), Select(lA.y, lB.y, lbSelectB),
                              Select(lA.z, lB.z, lbSelectB), Select(lA.w, lB.w, lbSelectB)}; return lResult; }
        }
        inline float RefinedReciprocalSqrt(float lfValue)
        {
            // FLAG (model): correctly rounded vrsqrtefp seed, as in SLerpDetail.
            // Both unconditional VMX Newton passes are retained, including 0*Inf.
            float lfEstimate = Flush(static_cast<float>(1.0 / std::sqrt(static_cast<double>(Flush(lfValue)))));
            for (int liStep = 0; liStep < 2; ++liStep)
            {
                const float lfSquared = Multiply(lfEstimate, lfEstimate);
                const float lfHalf = Multiply(lfEstimate, 0.5f);
                const float lfResidual = NegativeMultiplySubtract(lfValue, lfSquared, 1.0f);
                lfEstimate = MultiplyAdd(lfHalf, lfResidual, lfEstimate);
            }
            return lfEstimate;
        }
    }

    // ARTIST JointFrames setters 8259B580/8259B728/8259B8D0 inline this
    // conversion. Four candidates are evaluated before the final three vsels.
    // Runtime sign masks are written at 82C74000/30/60 (their BSS image is zero).
    // Keep (signedX+signedY)+(signedZ+1), the two unguarded rsqrt refinements,
    // t*rsqrt / 0.5*rsqrt, off-diagonal gathers and strict comparison order.
    inline Quaternion QuaternionFromMatrix33(const Matrix33& m, float epsilon = 0.0f)
    {
        using namespace QuaternionFromMatrix33Detail;
        const float lfX = m.xAxis.x, lfY = m.yAxis.y, lfZ = m.zAxis.z;
        const float laSignedX[4] = {lfX, lfX, FlipSign(lfX), FlipSign(lfX)};
        const float laSignedY[4] = {lfY, FlipSign(lfY), lfY, FlipSign(lfY)};
        const float laSignedZ[4] = {lfZ, FlipSign(lfZ), FlipSign(lfZ), lfZ};
        float laSumXY[4], laS[4], laS2[4];
        for (int liLane = 0; liLane < 4; ++liLane)
        {
            laSumXY[liLane] = Add(laSignedX[liLane], laSignedY[liLane]);
            const float lfT = Add(laSumXY[liLane], Add(laSignedZ[liLane], 1.0f));
            const float lfInverseRoot = RefinedReciprocalSqrt(lfT);
            laS[liLane] = Multiply(lfT, lfInverseRoot);
            laS2[liLane] = Multiply(0.5f, lfInverseRoot);
        }

        // The original two vperm/vsldoi gathers end in half and zero, giving
        // N.w = S.w = 0.5. Upper row W lanes never enter the conversion.
        const Vector4 lN{Subtract(m.yAxis.z, m.zAxis.y), Subtract(m.zAxis.x, m.xAxis.z),
                         Subtract(m.xAxis.y, m.yAxis.x), 0.5f};
        const Vector4 lS{Add(m.yAxis.z, m.zAxis.y), Add(m.zAxis.x, m.xAxis.z),
                         Add(m.xAxis.y, m.yAxis.x), 0.5f};
        const Quaternion lQ0{Multiply(lN.x, laS2[0]), Multiply(lN.y, laS2[0]),
                             Multiply(lN.z, laS2[0]), Multiply(lN.w, laS[0])};
        const Quaternion lQ1{Multiply(lN.w, laS[1]), Multiply(lS.z, laS2[1]),
                             Multiply(lS.y, laS2[1]), Multiply(lN.x, laS2[1])};
        const Quaternion lQ2{Multiply(lS.z, laS2[2]), Multiply(lN.w, laS[2]),
                             Multiply(lS.x, laS2[2]), Multiply(lN.y, laS2[2])};
        const Quaternion lQ3{Multiply(lS.y, laS2[3]), Multiply(lS.x, laS2[3]),
                             Multiply(lN.w, laS[3]), Multiply(lN.z, laS2[3])};
        Quaternion lResult = Select(lQ3, lQ2, Flush(lfY) > Flush(lfZ));
        lResult = Select(lResult, lQ1, (Flush(lfX) > Flush(lfY)) & (Flush(lfX) > Flush(lfZ)));
        const float lfTrace = Add(laSumXY[0], lfZ);
        return Select(lResult, lQ0, lfTrace > Flush(epsilon));
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
        CGS_STATIC_ASSERT((offsetof(JointFrames, mPosA)  == offsetof(JointFrames, mQuatA) + sizeof(JointFrames::mQuatA)), "mPosA follows mQuatA");
        CGS_STATIC_ASSERT((offsetof(JointFrames, mQuatB) == offsetof(JointFrames, mPosA)  + sizeof(JointFrames::mPosA)),  "mQuatB follows mPosA");
        CGS_STATIC_ASSERT((offsetof(JointFrames, mPosB)  == offsetof(JointFrames, mQuatB) + sizeof(JointFrames::mQuatB)), "mPosB follows mQuatB");
        CGS_STATIC_ASSERT((offsetof(JointFrames, mQuatL) == offsetof(JointFrames, mPosB)  + sizeof(JointFrames::mPosB)),  "mQuatL follows mPosB");

        // The X360-attested ARRAY STRIDE. ProcessUpdateJointFramesQueue @0x8289F4E8 steps
        // JointData::maFrames by `slwi r10,r31,2 / add r10,r31,r10 / slwi r10,r10,4` == i*5*16
        // == i*80, and copies exactly FIVE 16-byte lanes at 0/0x10/0x20/0x30/0x40. The drive
        // sibling does four at a stride of 64 -- this record is one quaternion larger.
        CGS_STATIC_ASSERT((sizeof(JointFrames) == 80), "JointFrames array stride 80 (drain i*80 + FIVE lvx128 lanes)");
    }
}
}

#include "vendor/renderware/physics/JointFrames.hpp"

// ===========================================================================
// rw::physics::JointFrames -- the three Matrix33 setter bodies the X360
// binary carries as their own TU. Reconstructed from BURNOUT_X360_ARTIST.XEX
// (dedicated VMX pass wave 2).
//
//   JointFrames::SetChildAngularFrame(Matrix33)   @ 0x8259B580
//   JointFrames::SetParentAngularFrame(Matrix33)  @ 0x8259B728
//   JointFrames::SetParentLinearFrame(Matrix33)   @ 0x8259B8D0
//
// All three bodies are instruction-for-instruction IDENTICAL except the
// final store target (stvx128 v0 at this+0x00 / +0x20 / +0x40): each is the
// fully-inlined rw::math::vpu::QuaternionFromMatrix33(m, epsilon = 0.0f)
// whose exact VMX source and rodata mapping are documented on the inline in
// JointFrames.hpp. r3 = this, r4 = &m (three 16-byte rows read at
// +0x00/+0x10/+0x20). Caller of all three:
// BrnPhysics::Vehicle::ArticulatedJointPool::ConstructArticulatedJoint.
// ===========================================================================

namespace rw
{
namespace physics
{

// X360 0x8259B580: single store stvx128 v0, r0, r3 => mQuatA (+0x00).
void JointFrames::SetChildAngularFrame(const rw::math::vpu::Matrix33& m)
{
    mQuatA = rw::math::vpu::QuaternionFromMatrix33(m);
}

// X360 0x8259B728: single store stvx128 v0, r3, r9(0x20) => mQuatB (+0x20).
void JointFrames::SetParentAngularFrame(const rw::math::vpu::Matrix33& m)
{
    mQuatB = rw::math::vpu::QuaternionFromMatrix33(m);
}

// X360 0x8259B8D0: single store stvx128 v0, r3, r11(0x40) => mQuatL (+0x40).
void JointFrames::SetParentLinearFrame(const rw::math::vpu::Matrix33& m)
{
    mQuatL = rw::math::vpu::QuaternionFromMatrix33(m);
}

} // namespace physics
} // namespace rw

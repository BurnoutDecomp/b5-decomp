#include "rw/physics/rigidbody.h"

// ===========================================================================
// rw::physics::RigidBody -- definition home for the two RigidBody methods the
// X360 binary carries as their own TU. Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   RigidBody::operator=      @ 0x825E3410   (BODIED below)
//   RigidBody::DynamicUpdate  @ 0x82BC2B78   (KEYSTONE -- not bodied; see below)
//
// The RigidBody type itself lives in its canonical home, rw/physics/rigidbody.h.
// These two methods were grown into that header additively (declarations only).
// ===========================================================================

namespace rw
{
namespace physics
{

// ---------------------------------------------------------------------------
// RigidBody::operator= @ 0x825E3410
//
// A full field copy of the 176-byte (0xB0) body. The asm copies the first
// 16-byte register at +0x00 (lvx128/stvx128) then every 4-byte field from
// +0x10 to +0xAC (lfs/stfs for the float lanes, lwz/stw for the integer
// housekeeping lanes at +0x1C/+0x2C/+0x3C/.../+0xAC). That is exactly a
// member-wise copy of all eleven Vector4 lane-registers, so the reconstruction
// copies the members by name. (RigidBody's members are private; this method is
// a member, so the named copy is well-formed.)
// ---------------------------------------------------------------------------
RigidBody& RigidBody::operator=(const RigidBody& rOther)
{
    mQuat   = rOther.mQuat;     // +0x00  (lvx128/stvx128 register copy)
    mCom    = rOther.mCom;      // +0x10
    mVel    = rOther.mVel;      // +0x20
    mOmega  = rOther.mOmega;    // +0x30
    mRi     = rOther.mRi;       // +0x40
    mUp     = rOther.mUp;       // +0x50
    mAt     = rOther.mAt;       // +0x60
    mIfull  = rOther.mIfull;    // +0x70
    mIsplt  = rOther.mIsplt;    // +0x80
    mForce  = rOther.mForce;    // +0x90
    mTorque = rOther.mTorque;   // +0xA0  (ends at +0xAC)
    return *this;
}

// ---------------------------------------------------------------------------
// RigidBody::DynamicUpdate @ 0x82BC2B78 -- KEYSTONE, intentionally NOT defined.
//
// See rigidbody.h and the group's still_unbodied report for the full reason.
// In short: the integrator's quaternion-renormalise and quaternion->basis
// conversion read three .rdata vperm control vectors (unk_82CDA3D0,
// unk_82CDA410, unk_82CDA450) and a normalise constant
// (rw::math::vpu::detail::gSqrt2s) whose values are NOT present in the X360
// export, so the permute lane-routing cannot be reproduced faithfully; and the
// body reads this+0x1C and this+0x4C as POINTERS (the inertia/constraint links)
// while the committed rigidbody.h layout names those offsets as the float lanes
// mVel.w / mUp.w -- bodying it faithfully would require retyping the committed
// home. Left declared-only (the compile gate is compile-only, so the
// declaration is enough for callers to resolve).
// ---------------------------------------------------------------------------

} // namespace physics
} // namespace rw

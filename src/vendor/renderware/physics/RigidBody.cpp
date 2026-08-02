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
// The three read accessors ExternallySimulatedBody::ReadFromRenderware @0x825A2E88
// consumes. ADDITIVE 2026-08-02 (physics wave 3): they were declared-only in
// rigidbody.h, which made ExternallySimulatedBody.cpp unlinkable (three of the 17
// LNK2019s measured for the vehicle-dynamics core).
//
// They have no X360 symbol of their own -- the console inlines them into the
// caller, and the caller's asm IS the specification. ExternallySimulatedBody.cpp's
// own committed banner records that asm:
//     mTransform       = GetTransform()        "copies the body's mRi/mUp/mAt
//                                               orientation rows + mCom position"
//     mLinearVelocity  = GetLinearVelocity()   "lvx r4+0x20 -> this+0x40"
//     mAngularVelocity = GetAngularVelocity()  "lvx r4+0x30 -> this+0x50"
// and rigidbody.h's committed member sequence puts mCom at +0x10, mVel at +0x20,
// mOmega at +0x30, mRi at +0x40, mUp at +0x50, mAt at +0x60 -- so the two velocity
// loads land on mVel / mOmega EXACTLY, and the three orientation rows the transform
// is assembled from are mRi / mUp / mAt with mCom as the translation row. Nothing
// here is inferred beyond that already-committed offset table.
//
// The console packs housekeeping scalars in each register's w lane (mCom.w == mId,
// mVel.w == mRight, ...). Matrix44Affine / Vector3 are Vector3-shaped (x,y,z,w) so a
// lane-for-lane copy would carry those scalars into the caller's w lanes, exactly as
// the console's lvx/stvx register copies do. The w lanes are reproduced verbatim
// rather than zeroed, because the console copy is a whole-register move.
// ---------------------------------------------------------------------------
rw::math::vpu::Matrix44Affine RigidBody::GetTransform() const
{
    rw::math::vpu::Matrix44Affine lResult;
    lResult.xAxis = rw::math::vpu::Vector3{ mRi.x,  mRi.y,  mRi.z,  mRi.w  };   // +0x40
    lResult.yAxis = rw::math::vpu::Vector3{ mUp.x,  mUp.y,  mUp.z,  mUp.w  };   // +0x50
    lResult.zAxis = rw::math::vpu::Vector3{ mAt.x,  mAt.y,  mAt.z,  mAt.w  };   // +0x60
    lResult.wAxis = rw::math::vpu::Vector3{ mCom.x, mCom.y, mCom.z, mCom.w };   // +0x10
    return lResult;
}

rw::math::vpu::Vector3 RigidBody::GetLinearVelocity() const
{
    return rw::math::vpu::Vector3{ mVel.x, mVel.y, mVel.z, mVel.w };            // +0x20
}

rw::math::vpu::Vector3 RigidBody::GetAngularVelocity() const
{
    return rw::math::vpu::Vector3{ mOmega.x, mOmega.y, mOmega.z, mOmega.w };    // +0x30
}

// ---------------------------------------------------------------------------
// RigidBody::GetLocalInvInertiaDiagonal @ -- intentionally NOT defined.
//
// Its console storage is a POINTER packed into the mUp.w float lane (the caller
// loads it as `lwz +0x5C`), which the committed 64-bit PC layout cannot represent:
// mUp.w is a float, and widening it would retype a vendor home that eleven other
// registers' lane packing depends on. The same packed-pointer problem keeps
// DynamicUpdate declared-only (below). Its ONE caller,
// ExternalPhysicsBody::ReadPropertiesFromRenderware @0x825A2280, was split out of
// ExternalPhysicsBody.cpp into its own (unmounted) TU in physics wave 3 so that the
// rest of ExternalPhysicsBody -- the whole integrator -- could be mounted.
// ---------------------------------------------------------------------------

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

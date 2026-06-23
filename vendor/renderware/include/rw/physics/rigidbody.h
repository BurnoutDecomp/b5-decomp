#pragma once

// Portable PC reconstruction of the EATech RenderWare physics RigidBody TYPE
// (rw/physics/rigidbody.h, EATech SDK cmn). The console body stores its pose as an
// orientation BASIS (right/up/at column/row vectors) plus the centre-of-mass position, with
// the per-vector w-lanes packing scalar housekeeping fields (id/tag/cooldown/...). We keep the
// memory LAYOUT faithful -- one 16-byte SIMD register per vector field, in the DWARF member
// sequence (rigidbody.h:419..) -- so game code that reads a RigidBody by its accessors links,
// and the few accessors the externally-simulated body needs are declared here.
//
// SCOPE: only the read accessors BrnPhysics::ExternallySimulatedBody::ReadFromRenderware uses
// (GetTransform / GetLinearVelocity / GetAngularVelocity) are declared. The full method set
// (Set*, Add*, DynamicUpdate, inertia, simulation wiring) belongs to the SDK's own
// reconstruction and is intentionally NOT declared here. The SIMD math machinery is not
// modelled; the named lanes of the flat vectors are accessed directly by callers.

#include "rw/math/vpu/types.h"   // rw::math::vpu::{Vector3, Vector4, Matrix44Affine}

namespace rw
{
namespace physics
{
    enum BodyState
    {
        STATE_DISABLED = 0,
        STATE_FROZEN   = 1,
        STATE_ENABLED  = 2
    };

    struct RigidBody
    {
        // Read the world transform: orientation basis (mRi/mUp/mAt) as the upper 3x3 plus the
        // centre-of-mass position as the translation row. Declared-only (owned by the SDK
        // reconstruction); ExternallySimulatedBody::ReadFromRenderware calls it BY NAME.
        rw::math::vpu::Matrix44Affine GetTransform() const;
        rw::math::vpu::Vector3        GetLinearVelocity() const;   // mVel
        rw::math::vpu::Vector3        GetAngularVelocity() const;  // mOmega

        // ADDITIVE GROW (rw-collision-physics group): the two RigidBody methods
        // the X360 binary defines as their own TU. Declared here so the canonical
        // home owns them; defined in the SDK reconstruction
        // (src/vendor/renderware/physics/RigidBody.cpp).
        //
        //   operator=      @ 0x825E3410 -- full 176-byte field copy. BODIED.
        //   DynamicUpdate  @ 0x82BC2B78 -- the per-body integrator. NOT BODIED:
        //     it is a keystone (see RigidBody.cpp / the group's still_unbodied
        //     note) -- it reaches unrecovered .rdata permutation masks
        //     (unk_82CDA3D0/410/450, rw::math::vpu::detail::gSqrt2s) and reads
        //     this+0x1C / this+0x4C as POINTERS, which conflicts with the
        //     committed float-lane members mVel.w / mUp.w. Declared-only so
        //     callers (Simulation::BatchIntegrator) can resolve the name.
        RigidBody& operator=(const RigidBody& rOther);   // @ 0x825E3410
        RigidBody* DynamicUpdate();                      // @ 0x82BC2B78 (declared-only)

    private:
        // Member SEQUENCE per the DWARF (rigidbody.h:419..459). Each Vector field is a 16-byte
        // SIMD register whose w-lane carries the adjacent scalar in the console packing; here
        // those scalars are kept as the named lane neighbours so the layout/stride matches.
        rw::math::vpu::Vector4 mQuat;     // :419  orientation quaternion
        rw::math::vpu::Vector4 mCom;      // :421  centre of mass (mId packed in w)
        rw::math::vpu::Vector4 mVel;      // :425  linear velocity (mRight packed in w)
        rw::math::vpu::Vector4 mOmega;    // :429  angular velocity (mLeft packed in w)
        rw::math::vpu::Vector4 mRi;       // :433  right basis vector (mPad0 in w)
        rw::math::vpu::Vector4 mUp;       // :437  up basis vector    (mPad1 in w)
        rw::math::vpu::Vector4 mAt;       // :441  at basis vector    (mTag in w)
        rw::math::vpu::Vector4 mIfull;    // :445  inertia (full)     (mInvm in w)
        rw::math::vpu::Vector4 mIsplt;    // :449  inertia (split)    (mState in w)
        rw::math::vpu::Vector4 mForce;    // :453  accumulated force  (mKine in w)
        rw::math::vpu::Vector4 mTorque;   // :457  accumulated torque (mCool in w)
    };
}
}

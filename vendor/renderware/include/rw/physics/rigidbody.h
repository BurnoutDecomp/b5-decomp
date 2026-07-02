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

    // The frame a force/impulse/position argument is expressed in (rigidbody.h:98 DWARF).
    // ADDITIVE GROW (Deformation car-car-impulse group): ImpulseParams::mePositionSpace embeds
    // this enum BY VALUE, so it needs the real type here (not an opaque blob). Values are
    // DWARF-authoritative (rw::physics::InputSpace { WORLD_SPACE=0, BODY_SPACE=1 }).
    enum InputSpace
    {
        WORLD_SPACE = 0,
        BODY_SPACE  = 1
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

        // ADDITIVE GROW (BrnPhysics::ExternalPhysicsBody::ReadPropertiesFromRenderware
        // @0x825A2280, which reads these four properties back out of the body):
        //   mIfull  = {Ixx, Ixy, Ixz | mInvm in w}   (the tensor's first row + inv mass)
        //   mIsplt  = {Izz, Iyy, Iyz | mState in w}  (the remaining unique tensor terms)
        // The full symmetric world inertia tensor reassembles as rows
        //   {Ixx,Ixy,Ixz} / {Ixy,Iyy,Iyz} / {Ixz,Iyz,Izz} -- exactly the two vperm masks
        // (.rdata 0x8208C2A0/B0: {A.z,B.z,B.x,A.w} / {A.y,B.y,B.z,A.w}) the caller applies.
        const rw::math::vpu::Vector4& GetInertiaFull() const  { return mIfull; }
        const rw::math::vpu::Vector4& GetInertiaSplit() const { return mIsplt; }
        float GetInverseMass() const { return mIfull.w; }   // mInvm (packed in mIfull.w)

        // The body's LOCAL inverse-inertia diagonal vector. On the console this pointer is
        // packed into the mUp.w scalar lane (the caller loads it as `lwz +0x5C`) -- the same
        // pointer-in-a-float-lane console packing the DynamicUpdate note above describes, so
        // it cannot be an inline lane read on the 64-bit PC. DECLARED-ONLY: the PC body lands
        // with the SDK reconstruction, where the packed lane gets its PC-side storage answer.
        const rw::math::vpu::Vector4* GetLocalInvInertiaDiagonal() const;

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

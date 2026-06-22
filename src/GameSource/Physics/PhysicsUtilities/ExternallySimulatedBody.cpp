#include "GameSource/Physics/PhysicsUtilities/ExternallySimulatedBody.h"

// BrnPhysics::ExternallySimulatedBody -- the single ledger func owned by the Physics-IO-util
// group: ReadFromRenderware @0x825A2E88.
//
// The X360 body is VMX128 register code (lvx128/stvx128) with no PC equivalent; the body below
// is the de-SIMD'd equivalent written against the members BY NAME (cf. ExternalPhysicsBody.cpp
// / BrnTagPoint.cpp -- no __asm).
//
// RECOVERED DATA FLOW (the `dword_82FB7518==0 && dword_82FB7514==0` branch of the asm -- the
// normal, ship path):
//   if (mbFrozen)            -> do nothing (asm: `lbz r11,0x60(r3)` / `bne` skips the whole body)
//   else:
//     mTransform        = lpRigidBody->GetTransform()         (asm copies the body's mRi/mUp/mAt
//                                                              orientation rows + mCom position
//                                                              into this->mTransform's rows)
//     mLinearVelocity   = lpRigidBody->GetLinearVelocity()    (asm: lvx r4+0x20 -> this+0x40)
//     mAngularVelocity  = lpRigidBody->GetAngularVelocity()   (asm: lvx r4+0x30 -> this+0x50)
//
// FLAG (debug-toggle paths NOT reconstructed -- honest placeholder): the asm has two extra
// branches gated on the file-static debug globals `dword_82FB7518` and `dword_82FB7514`, which
// substitute a forced spin (using the un-homed spin-speed constants KF_SPINNING_SPEED_X/Y/Z and
// the rodata floats flt_82FB751C/7520/7524 scaled by 2*pi) and a forced hold-in-place. Those
// globals + constants are debug-only rodata with NO project home and NO recoverable runtime
// value (they default OFF in a ship build), so they are NOT fabricated here. The reconstructed
// body is the both-flags-OFF path, which is the live behaviour; if a future TU homes the debug
// toggles it must GROW this body additively (re-introduce the two branches), not retype it.

namespace BrnPhysics
{
    void ExternallySimulatedBody::ReadFromRenderware(const rw::physics::RigidBody* lpRigidBody)
    {
        if (mbFrozen)
            return;

        // Normal path: snapshot the simulated body's pose and velocities into this mirror.
        mTransform       = lpRigidBody->GetTransform();
        mLinearVelocity  = lpRigidBody->GetLinearVelocity();
        mAngularVelocity = lpRigidBody->GetAngularVelocity();
    }
}

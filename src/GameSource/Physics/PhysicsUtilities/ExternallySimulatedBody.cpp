#include "GameSource/Physics/PhysicsUtilities/ExternallySimulatedBody.h"
#include "rw/math/vpu/vector3_operation.h"   // rw::math::vpu::Add (Translate)

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
    // -------------------------------------------------------------------------------------
    // Construct @0x8259CF28 / Destruct @0x8259CFF8 / Prepare @0x8259D0C0 / Release @0x8259D190
    //
    // FIDELITY: CLEAN, and recovered store-for-store. All four X360 bodies are the same 50-51
    // instructions with the same stores; Prepare alone also sets r3 = 1 (its return value).
    // Each builds the identity affine on the stack from flt_82001C98 (== 1.0f, already
    // rodata-verified by ReadPropertiesFromRenderware) and flt_82001CC0 (== 0.0f) --
    //     row0 {1,0,0,0}  row1 {0,1,0,0}  row2 {0,0,1,0}  row3 {0,0,0,0}
    // note row3.w is ZERO, which is exactly what Matrix44Affine::SetIdentity() writes -- then:
    //     stvx128 zero, this+0x40      -> mLinearVelocity  = 0
    //     stvx128 zero, this+0x50      -> mAngularVelocity = 0
    //     stb     0,    this+0x60      -> mbFrozen         = false
    //     the four identity rows       -> mTransform (this+0x00/0x10/0x20/0x30)
    // (the byte store to +0x60 and the two register stores to +0x40/+0x50 are what pin this
    // class's own frame -- see the LAYOUT note in the header).
    //
    // There is NO destructor-like teardown in Destruct and no allocation in Construct: on the
    // console these four are literally interchangeable resets. They are written out separately
    // rather than delegated so each keeps its own X360 address in the ledger.
    // -------------------------------------------------------------------------------------
    void ExternallySimulatedBody::Construct()
    {
        mTransform.SetIdentity();
        mLinearVelocity.SetZero();
        mAngularVelocity.SetZero();
        mbFrozen = false;
    }

    void ExternallySimulatedBody::Destruct()
    {
        mTransform.SetIdentity();
        mLinearVelocity.SetZero();
        mAngularVelocity.SetZero();
        mbFrozen = false;
    }

    bool ExternallySimulatedBody::Prepare()
    {
        mTransform.SetIdentity();
        mLinearVelocity.SetZero();
        mAngularVelocity.SetZero();
        mbFrozen = false;
        return true;   // asm: `li r3, 1` (the X360 always returns 1)
    }

    void ExternallySimulatedBody::Release()
    {
        mTransform.SetIdentity();
        mLinearVelocity.SetZero();
        mAngularVelocity.SetZero();
        mbFrozen = false;
    }

    // DWARF ExternallySimulatedBody.h:105 -- console-INLINED (no X360 symbol). Its two callers,
    // PropManager::HandleContactWithLeanProp @0x8260FB60 / TiltProp @0x826108B8, expand it as
    // `lvx128 v13, lpBody+0x40 ; vmaddfp128 v13 = pos + n*d ; stvx128 v13, lpBody+0x40`
    // (0x8260FCFC..0x8260FD10 / 0x82610A70..0x82610A7C): body +0x10 mTransform + 0x30 wAxis, i.e.
    // advance the pose's translation row by a world-space offset; the rotation rows and both
    // velocities are untouched. LANDED 2026-08-19 (wave Q6 / lean integration; measured body
    // in scratchpad/waveQ6/lean.owner.md section 5).
    void ExternallySimulatedBody::Translate(Vector3 lvTranslation)
    {
        mTransform.wAxis = rw::math::vpu::Add(mTransform.wAxis, lvTranslation);
    }

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

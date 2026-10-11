#include "GameSource/Physics/PhysicsUtilities/ExternallySimulatedBody.h"
#include "rw/math/vpu/vector3_operation.h"   // rw::math::vpu::Add (Translate)

// BrnPhysics::ExternallySimulatedBody -- the single ledger func owned by the Physics-IO-util
// group: ReadFromRenderware @0x825A2E88.
//
// The X360 body is VMX128 register code (lvx128/stvx128) with no PC equivalent; the body below
// is the de-SIMD'd equivalent written against the members BY NAME (cf. ExternalPhysicsBody.cpp
// / BrnTagPoint.cpp -- no __asm).
//
// RECOVERED DATA FLOW (ARTIST 0x825A2E88):
//   if mbFrozen: return without touching the mirror.
//   if KI_HOLD_CAR_SPINING_IN_AIR: takes priority over hold-in-place; copy the RenderWare
//       orientation, retain the prior mirror position X/Z/W, force Y=10, clear linear velocity,
//       and set angular velocity to the three menu speeds times 2*pi.
//   else: copy the RenderWare transform; if KI_HOLD_CAR_IN_PLACE, retain prior X/Z/W while
//       keeping the current physics Y; then copy both velocities normally.
// The five controls are the original mutable Sound debug cells at dword_82FB7514/18 and
// flt_82FB751C/20/24 (all image-initialized to zero).

namespace BrnPhysics
{
    // The Sound page's original shared debug controls. Each has one definition here.
    s32 KI_HOLD_CAR_IN_PLACE       = 0;     // dword_82FB7514
    s32 KI_HOLD_CAR_SPINING_IN_AIR = 0;     // dword_82FB7518
    f32 KF_SPINNING_SPEED_X        = 0.0f;  // flt_82FB751C
    f32 KF_SPINNING_SPEED_Y        = 0.0f;  // flt_82FB7520
    f32 KF_SPINNING_SPEED_Z        = 0.0f;  // flt_82FB7524

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

        if (KI_HOLD_CAR_SPINING_IN_AIR)
        {
            const Vector3 lPreviousPosition = mTransform.wAxis;
            mTransform = lpRigidBody->GetTransform();
            mTransform.wAxis = { lPreviousPosition.x, 10.0f,
                                 lPreviousPosition.z, lPreviousPosition.w };
            mLinearVelocity.SetZero();
            const f32 lfTwoPi = 6.2831855f; // flt_8208C240
            mAngularVelocity = { KF_SPINNING_SPEED_X * lfTwoPi,
                                 KF_SPINNING_SPEED_Y * lfTwoPi,
                                 KF_SPINNING_SPEED_Z * lfTwoPi,
                                 0.0f };
            return;
        }

        if (KI_HOLD_CAR_IN_PLACE)
        {
            const Vector3 lPreviousPosition = mTransform.wAxis;
            mTransform = lpRigidBody->GetTransform();
            // The original VMX insert mask replaces only position Y from physics; X/Z/W
            // remain the previous externally simulated position.
            mTransform.wAxis.x = lPreviousPosition.x;
            mTransform.wAxis.z = lPreviousPosition.z;
            mTransform.wAxis.w = lPreviousPosition.w;
        }
        else
        {
            mTransform = lpRigidBody->GetTransform();
        }

        mLinearVelocity  = lpRigidBody->GetLinearVelocity();
        mAngularVelocity = lpRigidBody->GetAngularVelocity();
    }
}

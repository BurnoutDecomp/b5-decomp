#pragma once

// X360 RigidBody uses the SDK's packed FPU triples interleaved with the
// 32-bit scalar/pointer slots read by ARTIST. DecFIGS rigidbody.h names the
// members; ARTIST DynamicUpdate and operator= establish the X360 layout.

#include "types.hpp"             // u32 / f32
#include "GameShared/GameClasses/RenderWare/Math/RwMathVectorTemplates.h" // fpu::Vector3Template
#include "rw/math/vpu/types.h"   // rw::math::vpu::{Vector3, Vector4, Matrix44Affine}
#include "rw/physics/inertia.h"  // rw::physics::Inertia

namespace rw
{
namespace physics
{
    class Simulation;
    struct JointJacobian;
    struct DriveJacobian;

    // rigidbody.h:73 (DWARF). ⚠️ A BITMASK, NOT AN ORDINAL -- the committed enum used to
    // read { DISABLED = 0, FROZEN = 1, ENABLED = 2 }, which is wrong in both the names and
    // the values. Every X360 witness agrees with the DWARF:
    //   ActivateRigidBody @0x82BC2A28  `rlwimi r11,r9(=1),2,29,27`  => state = 4 | (old & 8)
    //   FreezeRigidBody   @0x82BC2A98  `rlwimi r11,r9(=1),1,29,27`  => state = 2 | (old & 8)
    //   RemoveRigidBody   @0x82BC298C  `clrlwi r11,r11,29`          => the `& 7` filter
    //   Joint/DriveBatchBuild @0x82BC6A78 `or r11,r11,r10` then
    //                                     `rlwinm. r11,r11,0,29,29` => ACTIVE is bit 2
    // SPY_BODY (bit 3) is a separate flag that both setters preserve.
    enum BodyState
    {
        FREE_BODY    = 0,
        STATIC_BODY  = 1,
        FROZEN_BODY  = 2,
        ACTIVE_BODY  = 4,
        STATE_FILTER = 7,   // the `& 7` in RemoveRigidBody
        SPY_BODY     = 8    // preserved across Activate/Freeze
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
        // centre-of-mass position as the translation row. ExternallySimulatedBody::
        // ReadFromRenderware calls these BY NAME; bodied in the SDK reconstruction
        // (src/vendor/renderware/physics/RigidBody.cpp).
        rw::math::vpu::Matrix44Affine GetTransform() const;
        rw::math::vpu::Vector3        GetLinearVelocity() const;   // mVel
        rw::math::vpu::Vector3        GetAngularVelocity() const;  // mOmega

        //   operator=      @ 0x825E3410 -- full field copy. BODIED.
        //   DynamicUpdate  @ 0x82BC2B78 -- the per-body integrator. BODIED 2026-08-04; the
        //     two blockers this header used to record (unrecovered .rdata permute masks, and
        //     the packed pointer lanes) are both retired -- see the banner above and the
        //     body's own transcription notes.
        RigidBody& operator=(const RigidBody& rOther);   // @ 0x825E3410
        RigidBody* DynamicUpdate();                      // @ 0x82BC2B78

        // ARTIST AddRigidBody @0x82BC35A8..365C computes the tensor and
        // refreshes mInvm. DynamicUpdate's corresponding block preserves mInvm.
        void InertiaUpdate(Inertia* lpInertia);

        // ADDITIVE GROW (BrnPhysics::ExternalPhysicsBody::ReadPropertiesFromRenderware
        // @0x825A2280, which reads these four properties back out of the body):
        //   mIfull  = {Ixx, Ixy, Ixz}   (the tensor's first row)
        //   mIsplt  = {Izz, Iyy, Iyz}   (the remaining unique tensor terms -- Izz FIRST)
        // The full symmetric world inverse inertia reassembles as rows
        //   {Ixx,Ixy,Ixz} / {Ixy,Iyy,Iyz} / {Ixz,Iyz,Izz}.
        // ⭐ That split is now confirmed by THREE independent derivations: this header's
        // original ReadPropertiesFromRenderware + DWARF reading; DynamicUpdate's own VMX
        // (`vpermwi128` immediates 0x97/.zyyw and 0x9B/.zyzw select exactly the (2,2)/(1,1)/
        // (1,2) products); and the X360 .rdata expansion tables 0x821816A0 = (A.z,B.z,B.x,A.w)
        // and 0x821816B0 = (A.y,B.y,B.z,A.w), which are byte-read and match the Xbox One
        // build's explicit `vinsertps` chain at 0x1409B1DC5.
        // The packed triples are followed by mInvm and mState respectively.
        const rw::math::fpu::Vector3Template<f32>& GetInertiaFull() const  { return mIfull; }
        const rw::math::fpu::Vector3Template<f32>& GetInertiaSplit() const { return mIsplt; }

        // DWARF rigidbody.h:174 / :226 / :242 / :331 / :335 / :339 / :351. All inlined away
        // on the console -- no standalone X360 symbol for any of them.
        const f32& GetInverseMass() const     { return mInvm; }
        Inertia*   GetInertia() const         { return mInertia; }
        BodyState  GetState() const           { return mState; }
        Simulation* GetSimulation() const     { return mStasis; }
        RigidBody* GetRight() const           { return mRight; }
        RigidBody* GetLeft() const            { return mLeft; }
        void       SetRight(RigidBody* lpB)   { mRight = lpB; }
        void       SetLeft(RigidBody* lpB)    { mLeft = lpB; }
        u32        GetReactionForcesId() const { return mId; }
        u32        GetCoolDown() const        { return mCool; }
        f32        GetKineticEnergy() const   { return mKine; }
        u32        GetTag() const             { return mTag; }

        // ⭐ 2026-08-04 (task #140). The mutator set CgsPhysics::PhysicsSimulationModule::
        // ProcessAddRigidBodyQueue @0x828A2708 drives a freshly-added body with. Every NAME
        // and SIGNATURE below is DWARF-attested (rw/physics/rigidbody.h :89 SetLinearVelocity,
        // :92 SetAngularVelocity, :113 SetTag, :162 SetState, :215 SetSpy, :273 ResetForces);
        // none has its own X360 symbol -- the console inlines all six into that drain, which
        // is why the drain is 306 instructions.
        //
        // ⭐ ResetForces IS THE PAIR, NOT A SINGLE STORE. The drain emits, together,
        // `lwz r7,0x4C(r3)` + `lvx128 v13,r7,r6(=0x90)` + `vrlimi128`/`stvx128` into +0x90 AND
        // a zeroing `vrlimi128`/`stvx128` into +0xA0 -- i.e. force := the argument, torque := 0.
        // Reading it as "set the force" and dropping the torque zero would leave whatever the
        // previous owner of this pool node left behind.
        //
        // ⚠️ SetSpy(false) IS `mState & STATE_FILTER`, NOT `mState & ~SPY_BODY`. The console
        // uses `clrlwi r11,r11,29`, which clears bits 3..31. Identical for any legal state,
        // and this is the spelling the binary chose.
        //
        // These xyz writes preserve the following scalar in each native row.
        void SetLinearVelocity(const rw::math::vpu::Vector3& lrV)
        { __stvx(__vrlimi(lrV.mV, __lvx(&mVel, 0), 1, 0), &mVel, 0); }

        void SetAngularVelocity(const rw::math::vpu::Vector3& lrV)
        { __stvx(__vrlimi(lrV.mV, __lvx(&mOmega, 0), 1, 0), &mOmega, 0); }

        void ResetForces(const rw::math::vpu::Vector3& lrForce)
        {
            __stvx(__vrlimi(lrForce.mV, __lvx(&mForce, 0), 1, 0), &mForce, 0);
            __stvx(__vrlimi(__vspltisw(0), __lvx(&mTorque, 0), 1, 0), &mTorque, 0);
        }

        void SetState(BodyState leState) { mState = leState; }        // +0x8C
        void SetTag(u32 luTag)           { mTag   = luTag;   }        // +0x6C

        // DWARF rigidbody.h:209. ⭐ ADDITIVE GROW 2026-08-05 (the rigid-body drain group):
        // witnessed as the bare `stw 0, 0xAC(body)` all four update-side drains emit
        // (ProcessUpdateExternalBodyQueue @0x828A40B4, ProcessUpdateRigidBodyQueue
        // @0x828A3AFC, ProcessApplyForceQueue @0x828A6C84/0x828A6C94, and the committed
        // Simulation::ActivateRigidBody's `mCool = m_CoolDown - 1u` is the same slot).
        void SetCoolDown(u32 luCool)     { mCool  = luCool;  }        // +0xAC

        // DWARF rigidbody.h:221 `void AddForce(const Vector3&, InputSpace)`. NO OWN X360
        // SYMBOL; sole witness is the inline in ProcessApplyForceQueue @0x828A6C1C..0x828A6C7C,
        // whose call site passes WORLD_SPACE and where the console folded the space branch
        // away. So ONLY the world-space leg is attested:
        //     mForce.xyz += lrForce * mInvm
        // (`vmulfp128 v0, v0, {mInvm splat}` then three scalar `fadds` into +0x90/+0x94/+0x98).
        // ⚠️ The BODY_SPACE leg (a basis rotation before the accumulate) has NO witness in this
        // image and is deliberately NOT fabricated; the parameter is kept for the DWARF
        // signature and every in-tree caller passes WORLD_SPACE.
        // ⚠️ Yes, the accumulate is PRE-SCALED by the inverse mass -- mForce holds an
        // ACCELERATION-dimensioned sum here (consistent with ResetForces seeding it with the
        // simulation's gravity vector, -600 y, which DynamicUpdate integrates as-is).
        void AddForce(const rw::math::vpu::Vector3& lrForce, InputSpace leSpace)
        {
            (void)leSpace;   // only WORLD_SPACE witnessed -- see the banner above
            const __vector4 scale = __vspltw(__lvlx(&mInvm, 0), 0);
            const __vector4 impulse = __vmulfp(lrForce.mV, scale);
            mForce.X() += impulse.v[0];
            mForce.Y() += impulse.v[1];
            mForce.Z() += impulse.v[2];
        }

        // DWARF rigidbody.h:200 (the Matrix44Affine overload; the :203 QuatPos overload has
        // no witness in this build and is not declared). NO OWN X360 SYMBOL -- inlined into
        // ProcessUpdateExternalBodyQueue @0x828A3E28..0x828A3FAC, which is the transcription
        // basis. Bodied out-of-line in src/vendor/renderware/physics/RigidBody.cpp (it needs
        // rw::math::vpu::QuaternionFromMatrix33, whose current home is JointFrames.hpp).
        void SetTransform(const rw::math::vpu::Matrix44Affine& lrTransform);

        void SetSpy(bool lbSpy)
        {
            mState = lbSpy ? static_cast<BodyState>(mState | SPY_BODY)        // `ori r11,r11,8`
                           : static_cast<BodyState>(mState & STATE_FILTER);   // `clrlwi r11,r11,29`
        }

        // The body's LOCAL inverse-inertia diagonal -- the console reaches it through the
        // Inertia block whose pointer is packed in the mUp.w lane (`lwz +0x5C`). Now a plain
        // member read; ExternalPhysicsBody::ReadPropertiesFromRenderware's one caller works.
        const rw::math::vpu::Vector3& GetLocalInvInertiaDiagonal() const
        { return mInertia->GetInverseInertia(); }

        // DWARF rigidbody.h:137 / :185. ⭐ ADDITIVE GROW 2026-08-06 (the game-side spy wave):
        // witnessed as the inline in PhysicsSimulationModule::AddActiveBodiesToOutputQueue
        // @0x828A6D5C..0x828A6D7C -- the world-bounds clamp of the mCom row, whose
        // `vrlimi128 vD,vOld,1,0` before the store-back is exactly "xyz only, keep w":
        // mId follows the triple and is preserved by the xyz-only setter.
        rw::math::vpu::Vector3 GetPosition() const
        { return rw::math::vpu::Vector3(__lvx(&mCom, 0)); }

        void SetPosition(const rw::math::vpu::Vector3& lrPosition)
        { __stvx(__vrlimi(lrPosition.mV, __lvx(&mCom, 0), 1, 0), &mCom, 0); }

    private:
        // The SDK keeps everything private and reaches it through Simulation, the two
        // jacobian builders, and (since 2026-08-05) Contact -- whose GenerateFromCollision
        // is the console's inline in ProcessAddContactQueue @0x828A3458 and snapshots the
        // body's mCom/mIfull/mIsplt/mForce/mTorque rows plus their packed scalars into the
        // contact record's pad region for ContactBatchBuild to consume.
        friend class Simulation;
        friend struct JointJacobian;
        friend struct DriveJacobian;
        friend struct Contact;

        typedef rw::math::fpu::Vector3Template<f32> Vector3U_32;
        rw::math::vpu::Quaternion mQuat; // +0x00
        Vector3U_32 mCom;     u32 mId;             // +0x10 / +0x1C
        Vector3U_32 mVel;     RigidBody* mRight;   // +0x20 / +0x2C
        Vector3U_32 mOmega;   RigidBody* mLeft;    // +0x30 / +0x3C
        Vector3U_32 mRi;      Simulation* mStasis; // +0x40 / +0x4C
        Vector3U_32 mUp;      Inertia* mInertia;   // +0x50 / +0x5C
        Vector3U_32 mAt;      u32 mTag;            // +0x60 / +0x6C
        Vector3U_32 mIfull;   f32 mInvm;           // +0x70 / +0x7C
        Vector3U_32 mIsplt;   BodyState mState;    // +0x80 / +0x8C
        Vector3U_32 mForce;   f32 mKine;           // +0x90 / +0x9C
        Vector3U_32 mTorque;  u32 mCool;           // +0xA0 / +0xAC
    };
    inline void RigidBody::InertiaUpdate(Inertia* lpInertia)
    {
        const __vector4 lRi = __lvx(&mRi, 0);
        const __vector4 lUp = __lvx(&mUp, 0);
        const __vector4 lAt = __lvx(&mAt, 0);
        const __vector4 lDiagonal = lpInertia->GetInverseInertia().mV;
        const __vector4 lRiScaled = __vmulfp(lRi, __vspltw(lDiagonal, 0));
        const __vector4 lUpScaled = __vmulfp(lUp, __vspltw(lDiagonal, 1));
        const __vector4 lAtScaled = __vmulfp(lAt, __vspltw(lDiagonal, 2));

        __vector4 lFull = __vmulfp(lUpScaled, __vspltw(lUp, 0));
        lFull = __vmaddfp(lRiScaled, __vspltw(lRi, 0), lFull);
        lFull = __vmaddfp(lAtScaled, __vspltw(lAt, 0), lFull);
        lFull = __vrlimi(lFull, __lvx(&mIfull, 0), 1, 0);
        __stvx(lFull, &mIfull, 0);

        __vector4 lSplit = __vmulfp(__vpermwi(lUp, 0x97), __vpermwi(lUpScaled, 0x9B));
        lSplit = __vmaddfp(__vpermwi(lRi, 0x97), __vpermwi(lRiScaled, 0x9B), lSplit);
        lSplit = __vmaddfp(__vpermwi(lAt, 0x97), __vpermwi(lAtScaled, 0x9B), lSplit);
        lSplit = __vrlimi(lSplit, __lvx(&mIsplt, 0), 1, 0);
        __stvx(lSplit, &mIsplt, 0);
        const __vector4 lMass = __vspltw(__lvlx(&lpInertia->GetInverseMass(), 0), 0);
        __stvx(__vrlimi(__lvx(&mIfull, 0), lMass, 1, 0), &mIfull, 0);
    }
    CGS_STATIC_ASSERT((sizeof(RigidBody) == 0xB0), "ARTIST RigidBody size");
}
}

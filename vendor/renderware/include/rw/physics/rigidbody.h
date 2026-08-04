#pragma once

// Portable PC reconstruction of the EATech RenderWare physics RigidBody TYPE
// (rw/physics/rigidbody.h, EATech SDK cmn). The console body stores its pose as an
// orientation BASIS (right/up/at column/row vectors) plus the centre-of-mass position, with
// the per-vector w-lanes packing scalar housekeeping fields (id/tag/cooldown/...). We keep the
// memory LAYOUT faithful -- one 16-byte SIMD register per vector field, in the DWARF member
// sequence (rigidbody.h:419..) -- so game code that reads a RigidBody by its accessors links.
//
// =====================================================================================
// ⭐ 2026-08-04 -- THE PACKED SCALAR LANES ARE NOW REAL, NAMED MEMBERS.
//
// The console packs one 4-byte housekeeping scalar into the w lane of each pose vector.
// FIVE OF THOSE TEN SCALARS ARE POINTERS, and a 64-bit pointer does not fit in a float
// lane -- which is why this header used to carry a "DynamicUpdate is NOT BODIED" note and
// why rw/physics/Simulation.cpp used to carry a matching "the list splice functions are
// BLOCKED" note. Both notes named the right problem and both are now RETIRED: the ten
// scalars are declared as their own members, in the DWARF's own names, AFTER the eleven
// registers. The eleven registers keep their 16-byte stride and DWARF order; their w lanes
// are now unused padding on the PC (documented at each member).
//
// The X360 facts behind that, read out of BURNOUT_X360_ARTIST.XEX this wave:
//   0x82BC2B80  lwz r10,0x4C(r3) ; lfs f0,0xA0(r10)   -> +0x4C is the owning Simulation*
//   0x82BC2D3C  lwz r30,0x5C(r3) ; lfs f0,0x1C(r30)   -> +0x5C is the Inertia*
//   0x82BC2B8C  lwz r11,0x1C(r3) ; lvx128 v8,r0,r11   -> +0x1C is used AS AN ADDRESS
//   0x82BC2950  RigidBody::RemoveRigidBody's unlink    -> +0x2C is `next`, +0x3C is `prev`
//
// ⚠️⚠️ THE DWARF AND THE X360 DISAGREE ABOUT WHERE THE TWO POINTERS LIVE, AND BOTH ARE RIGHT.
//   The DWARF declares `mPad0`(+0x4C) and `mPad1`(+0x5C) as plain uint32_t and then puts
//   `mStasis` (Simulation*) and `mInertia` (Inertia*) AFTER `mCool`, i.e. a 184-byte body.
//   The X360 build has no members at +0xB0/+0xB4 at all (DynamicUpdate never touches them,
//   and RigidBody::operator= @0x825E3410 copies exactly +0x00..+0xAC) -- it puts the two
//   pointers in the two `mPadN` lanes and stays 176 bytes, saving a whole cache line.
//   We follow the X360 for WHAT EXISTS and the DWARF for NAMES: mStasis/mInertia are
//   declared once, the mPadN names are recorded as the lanes they occupy on the console.
//
// ⚠️ mId STAYS 32 BITS -- it is NOT widened. The DWARF types it `uint32_t` and names its
//   role `GetReactionForcesId()`; the shipping x64 builds keep it 32-bit and resolve
//   `Simulation::GetReactionForces() + mId*64` (Xbox One 0x1409B31ED: `mov edi,[rcx+1Ch]`,
//   32-bit, then `shl rdi,6 ; add rdi,[rax+18h]`). X360 caches the ALREADY-RESOLVED address
//   in the same slot, which is only possible because console pointers are 4 bytes. Same
//   semantics, different representation; the PC leaf resolves through the index.
//   ⇒ "the gap grew 4->8, so it is a pointer" is necessary but NOT sufficient: this slot is
//   a pointer on the console and stays 4 bytes on x64.
// =====================================================================================
//
// SCOPE: the read accessors BrnPhysics::ExternallySimulatedBody::ReadFromRenderware and
// ExternalPhysicsBody::ReadPropertiesFromRenderware use, plus the two methods the X360
// binary carries as their own TU (operator=, DynamicUpdate). The rest of the SDK method set
// (Set*, Add*, InertiaUpdate, ...) is still intentionally NOT declared here.

#include "types.hpp"             // u32 / f32
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

        // ⭐ 2026-08-04 (task #140). DWARF rigidbody.h:387 -- `void InertiaUpdate(Inertia*)`.
        // The world inverse-inertia rebuild, I_world = sum_k I_k * r_k (x) r_k over the three
        // basis vectors, stored in the {Ixx,Ixy,Ixz}/{Izz,Iyy,Iyz} split GetInertiaFull() /
        // GetInertiaSplit() document, plus mInvm from the block's inverse mass.
        //
        // ⚠️ IT HAS NO OWN X360 SYMBOL: the console INLINES it, and this header's whole point
        // is that it inlines it in TWO places -- DynamicUpdate @0x82BC2D60..0x82BC2DE8 and
        // Simulation::AddRigidBody @0x82BC35A8..0x82BC365C -- as the SAME instruction sequence
        // (`vpermwi128` 0x97/.zyyw + 0x9B/.zyzw, three `vmulfp128`, four `vmaddfp`). Before
        // this landed, that sequence existed once in the tree, hand-written inside
        // DynamicUpdate; AddRigidBody would have made it twice. A tensor written twice is a
        // tensor transposed once, silently, and nothing in this subsystem asserts on it --
        // so the DWARF's own factoring is used instead of copying the block.
        //
        // ⚠️ mInvm IS PART OF IT. AddRigidBody's copy ends with `lwz r9,0x5C(r11)` +
        // `lvlx v13,r9,r8(=0x10)` + `vspltw` -> mIfull.w, i.e. mInvm = mInertia->mInvMass,
        // inside the same `mInertia != NULL` guard. DynamicUpdate's copy does not re-read it
        // (the value is already live in a register there), which is an inlining artefact, not
        // a semantic difference: mInvm is a pure function of the block either way.
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
        // ⚠️ The console packs mInvm in mIfull.w and mState in mIsplt.w; on the PC both are
        // their own members (see the banner), so these two getters carry XYZ ONLY.
        const rw::math::vpu::Vector4& GetInertiaFull() const  { return mIfull; }
        const rw::math::vpu::Vector4& GetInertiaSplit() const { return mIsplt; }

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
        // The .xyz-only writes are the `vrlimi128 vD,vOld,1,0` w-lane preservation; on the PC
        // those w payloads are their own members, so leaving them alone IS the faithful copy.
        void SetLinearVelocity(const rw::math::vpu::Vector3& lrV)
        { mVel.x = lrV.x; mVel.y = lrV.y; mVel.z = lrV.z; }

        void SetAngularVelocity(const rw::math::vpu::Vector3& lrV)
        { mOmega.x = lrV.x; mOmega.y = lrV.y; mOmega.z = lrV.z; }

        void ResetForces(const rw::math::vpu::Vector3& lrForce)
        {
            mForce.x  = lrForce.x; mForce.y  = lrForce.y; mForce.z  = lrForce.z;   // +0x90
            mTorque.x = 0.0f;      mTorque.y = 0.0f;      mTorque.z = 0.0f;        // +0xA0
        }

        void SetState(BodyState leState) { mState = leState; }        // +0x8C
        void SetTag(u32 luTag)           { mTag   = luTag;   }        // +0x6C

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

    private:
        // The SDK keeps everything private and reaches it through Simulation / the two
        // jacobian builders, which are the only other classes in the subsystem.
        friend class Simulation;
        friend struct JointJacobian;
        friend struct DriveJacobian;

        // ---- the eleven 16-byte pose/inertia registers, DWARF order ----------------------
        // Each `// w:` note records what the CONSOLE packs in that lane. On the PC the lane
        // is unused padding and the payload is one of the named scalars below.
        rw::math::vpu::Quaternion mQuat;  // :419  orientation quaternion  (w: the real part)
        rw::math::vpu::Vector4 mCom;      // :421  centre of mass          (w: mId)
        rw::math::vpu::Vector4 mVel;      // :425  linear velocity         (w: mRight)
        rw::math::vpu::Vector4 mOmega;    // :429  angular velocity        (w: mLeft)
        rw::math::vpu::Vector4 mRi;       // :433  right basis vector      (w: mPad0 = mStasis)
        rw::math::vpu::Vector4 mUp;       // :437  up basis vector         (w: mPad1 = mInertia)
        rw::math::vpu::Vector4 mAt;       // :441  at basis vector         (w: mTag)
        rw::math::vpu::Vector4 mIfull;    // :445  inertia {Ixx,Ixy,Ixz}   (w: mInvm)
        rw::math::vpu::Vector4 mIsplt;    // :449  inertia {Izz,Iyy,Iyz}   (w: mState)
        rw::math::vpu::Vector4 mForce;    // :453  accumulated force       (w: mKine)
        rw::math::vpu::Vector4 mTorque;   // :457  accumulated torque      (w: mCool)

        // ---- the ten packed scalars, promoted -------------------------------------------
        u32         mId;        // :423  console mCom.w   +0x1C  reaction-force block index
        RigidBody*  mRight;     // :427  console mVel.w   +0x2C  intrusive `next`
        RigidBody*  mLeft;      // :431  console mOmega.w +0x3C  intrusive `prev`
        Simulation* mStasis;    // :461  console mRi.w    +0x4C  (the DWARF's mPad0 slot)
        Inertia*    mInertia;   // :463  console mUp.w    +0x5C  (the DWARF's mPad1 slot)
        u32         mTag;       // :443  console mAt.w    +0x6C
        f32         mInvm;      // :447  console mIfull.w +0x7C
        BodyState   mState;     // :451  console mIsplt.w +0x8C
        f32         mKine;      // :455  console mForce.w +0x9C  last tick's sleep energy
        u32         mCool;      // :459  console mTorque.w+0xAC  the sleep counter
    };
}
}

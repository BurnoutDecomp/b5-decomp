#pragma once

// =====================================================================================
// rw::physics::Contact -- one narrow-phase contact record in the simulation's contact
// scratch array (Simulation::m_CJ_Stack); 256 native bytes. Filled by
// CgsPhysics::PhysicsSimulationModule::ProcessAddContactQueue
// (which inlines GenerateFromCollision) and consumed IN PLACE by Simulation::
// ContactBatchBuild @0x82BC14C0, which rewrites the rows into the ContactJacobian overlay
// (bottom of this file) that Anubis/Horus iterate and SpyContactJacobians reads.
//
// EATech RenderWare physics. DWARF: DecFIGS SDKs/EATech/include/cmn/rw/physics/contact.h
// (struct :74, members :216..:239, methods :85..:207). The X360 asm is authoritative for
// behaviour and offsets (drain @0x828A3458; batch @0x82BC14C0).
//
// Native ARTIST contact records occupy 256 bytes. The drain carries the body
// pointers in the position rows at +0x0C/+0x1C. ContactBatchBuild snapshots
// those pointers, replaces the position w lanes with pool indices, and moves
// the pointers to the Jacobian normal/up w lanes at +0x7C/+0xAC.
// Named unions retain both phase-specific meanings without a host pointer tail.
//
// ⭐ THE SNAPSHOT REGION (+0x60..+0xF0) is the DWARF's `Vector4U_32 mPad[10]` (:239), spelled
// here with the lane names its only writer and only reader witness:
//   writer (drain @0x828A3458): ten full 16-byte `lvx/stvx` row copies of the two bodies'
//     mCom/mIfull/mIsplt/mForce/mTorque registers -- each console w lane carries the body's
//     packed scalar (mId/mInvm/mState/mKine/mCool, see rigidbody.h's promotion table);
//   reader (ContactBatchBuild): rA/rB from mPos-mCom, the ACTIVE test from the mIsplt row's
//     w lane (`vspltw ..,3` + `vand 4` + `vcmpequw` on +0xA0/+0xB0), the mint from mCom.w.
// A plain Vector4 mPad[10] on the PC would lose those scalars with no diagnostic (the PC
// RigidBody keeps them as named members, not w lanes) -- the silent-drop-stub shape. Named
// lanes carry them instead.
//
// DWARF methods NOT reconstructed here (recorded so nobody re-derives the list): ctor/dtor,
// GetResourceDescriptor (:93), GetBodyA/GetBodyB(Simulation*) (:105/:109 -- the accessor pair
// that proves mBodyA/mBodyB are pool-relative indices), Get/Set accessor family (:113..:189),
// and the 5-arg GenerateFromCollision (:196). Only the 9-arg GenerateFromCollision (:207),
// the form the drain inlines, has a witnessed body. Declaring the rest without bodies would
// be decls no TU defines; they land with witnesses or not at all.
// =====================================================================================

#include "types.hpp"                 // u32 / s32 / f32
#include "rw/math/vpu/types.h"       // rw::math::vpu::Vector3 (the 16-byte lane register)
#include "rw/physics/rigidbody.h"    // rw::physics::RigidBody, BodyState
#include "rw/physics/simulation.h"   // rw::physics::Simulation (GetFreeContact's home class)

#include "vendor/renderware/physics/JacobianMath.hpp"   // jacobian_detail:: Cross/Dot3/Sub/...

#include <cstddef>                   // offsetof (the layout pins)

namespace rw
{
namespace physics
{

// DWARF class-key is `struct` (contact.h:74). Members public for the same reason Jacobian.hpp's
// are: the offsetof pins below must name them, and the SDK's own friend surface (Simulation,
// via ContactBatchBuild) covers every access the subsystem makes anyway.
struct CGS_ALIGN(16) Contact
{
    // The DWARF spells the vector members `RigidBody::Vector3U_32` -- typedef
    // Vector3Template<float>, the PACKED 12-byte triple (rigidbody.h:35), NOT the 16-byte
    // vpu lane register. The 4-byte scalar after each triple completes the 16-byte row.
    struct Vector3U_32 { f32 x, y, z; };

    // ---- the six jacobian-input rows, DWARF names/order (:216..:238) ---------------------
    Vector3U_32 mPosA;      // +0x00  :216  contact point on body A (world)
    union { s32 mBodyA; RigidBody* mpBodyA; }; // +0x0C pointer; index after batch
    Vector3U_32 mPosB;      // +0x10  :220  contact point on body B (world)
    union { s32 mBodyB; RigidBody* mpBodyB; }; // +0x1C drain pointer / batch index
    Vector3U_32 mRi;        // +0x20  :224  the contact normal (frame row 0)
    f32         mRes;       // +0x2C  :226  restitution
    Vector3U_32 mUp;        // +0x30  :228  cross(T, n) * rsqrt  (frame row 1)
    f32         mMus;       // +0x3C  :230  static friction
    Vector3U_32 mAt;        // +0x40  :232  T * rsqrt            (frame row 2)
    f32         mMud;       // +0x4C  :234  dynamic friction
    Vector3U_32 mVel;       // +0x50  :236  point-relative velocity (B - A)
    u32         mTag;       // +0x5C  :238  the drain stores its LOOP INDEX here (the event's
                            //              muTag is never read -- verified, no `l.. 0x4C(r8)`)

    // ---- the body snapshot -- DWARF `Vector4U_32 mPad[10]` (:239), lanes named per the
    // witnessed writer/reader (see banner) ------------------------------------------------
    Vector3U_32 mComA;      // +0x60  bodyA mCom.xyz     (console row copy of body +0x10)
    u32         mIdA;       // +0x6C  bodyA mId          (console mCom.w -- the mint source)
    Vector3U_32 mComB;      // +0x70  bodyB mCom.xyz
    u32         mIdB;       // +0x7C  bodyB mId
    Vector3U_32 mIfullA;    // +0x80  bodyA mIfull.xyz   (console row copy of body +0x70)
    f32         mInvmA;     // +0x8C  bodyA mInvm        (console mIfull.w)
    Vector3U_32 mIfullB;    // +0x90  bodyB mIfull.xyz
    f32         mInvmB;     // +0x9C  bodyB mInvm
    Vector3U_32 mIspltA;    // +0xA0  bodyA mIsplt.xyz   (console row copy of body +0x80)
    u32         mStateA;    // +0xAC  bodyA mState       (console mIsplt.w -- the batch's
                            //                            `& ACTIVE_BODY` test reads THIS lane)
    Vector3U_32 mIspltB;    // +0xB0  bodyB mIsplt.xyz
    u32         mStateB;    // +0xBC  bodyB mState
    Vector3U_32 mForceA;    // +0xC0  bodyA mForce.xyz   (console row copy of body +0x90)
    f32         mKineA;     // +0xCC  bodyA mKine        (console mForce.w)
    Vector3U_32 mForceB;    // +0xD0  bodyB mForce.xyz
    f32         mKineB;     // +0xDC  bodyB mKine
    Vector3U_32 mTorqueA;   // +0xE0  bodyA mTorque.xyz  (console row copy of body +0xA0)
    u32         mCoolA;     // +0xEC  bodyA mCool        (console mTorque.w)
    Vector3U_32 mTorqueB;   // +0xF0  bodyB mTorque.xyz
    u32         mCoolB;     // +0xFC  bodyB mCool

    // -------------------------------------------------------------------------------------
    // GenerateFromCollision, the 9-argument form (DWARF contact.h:207) -- inlined by the
    // console into ProcessAddContactQueue @0x828A3458 (0x828A3708..0x828A39B4), transcribed
    // from that inline instruction-for-instruction. Fills the whole record from the two
    // bodies and the event payload.
    //
    // ⚠️ THE CONSOLE PRELUDE IS A DEAD ARTIFACT, stated so nobody "restores" it: the inline
    // saves the four live w lanes (+0x2C/+0x3C/+0x4C/+0x5C), zeroes all 256 bytes with two
    // dcbz128, restores the four -- and then overwrites all four with the event's friction/
    // restitution and the caller's tag. Net effect: zero + full fill. Spelled here as the
    // named-member assignments; the final pointer writes fill the position w lanes.
    //
    // ⚠️ THE NORMALISATION IS A SINGLE `vrsqrtefp` ESTIMATE -- unlike the jacobian builders
    // (vrsqrtefp + two Newton-Raphson refines, JacobianMath.hpp Sqrt note), the console does
    // NOT refine here. 1/Sqrt() on the PC is behaviourally identical, not bit-identical
    // (more precise, same zero->+inf edge as vrsqrtefp(0)).
    // -------------------------------------------------------------------------------------
    void GenerateFromCollision(RigidBody* lpBodyA, RigidBody* lpBodyB,
                               const rw::math::vpu::Vector3& lrPointOnA,
                               const rw::math::vpu::Vector3& lrPointOnB,
                               const rw::math::vpu::Vector3& lrNormal,
                               f32 lfStaticFriction, f32 lfDynamicFriction,
                               f32 lfRestitution, u32 luTag)
    {
        using jacobian_detail::V3;
        using jacobian_detail::Xyz;
        using jacobian_detail::Sub;
        using jacobian_detail::Add;
        using jacobian_detail::Scale;
        using jacobian_detail::Cross;
        using jacobian_detail::Dot3;
        using jacobian_detail::MakeV3;
        using jacobian_detail::Sqrt;

        const V3 lvPosA = Xyz(lrPointOnA);
        const V3 lvPosB = Xyz(lrPointOnB);
        const V3 lvN    = Xyz(lrNormal);

        // rA/rB = contact point minus centre of mass (`vsubfp` on the raw rows, 0x828A3788).
        const V3 lvRA = Sub(lvPosA, Xyz(lpBodyA->mCom));
        const V3 lvRB = Sub(lvPosB, Xyz(lpBodyB->mCom));

        // ---- tangent selection (0x828A3790..0x828A38B8) ---------------------------------
        // T0 = cross(n, r) with r chosen by BODY A's ACTIVE bit alone (the `vspltw v4,v6,3`
        // + `vand 4` + `vcmpequw` mask on bodyA's mIsplt row w lane -- console mState).
        const bool lbAActive = (lpBodyA->mState & ACTIVE_BODY) != 0;
        const V3   lvT0      = Cross(lvN, lbAActive ? lvRA : lvRB);
        const f32  lfT0LenSq = Dot3(lvT0, lvT0);

        V3  lvT;
        f32 lfLenSq;
        if (lfT0LenSq >= 1e-4f)   // splat table 0x820F1110; `vcmpgefp v11, |T0|^2, 1e-4`
        {
            lvT     = lvT0;
            lfLenSq = lfT0LenSq;
        }
        else if (lvN.x * lvN.x <= 0.5f)   // `vcmpgefp (0.5 - n.x^2), -0.0` -- tables
        {                                 //  0x820F1120 (0.5) / 0x820F1100 (-0.0)
            // xAxis {1,0,0} - n*n.x (table 0x820F10E0); lenSq is that vector's OWN x lane
            // (`vspltw v9,0` == 1 - n.x^2), exactly as the console takes it.
            lvT     = Sub(MakeV3(1.0f, 0.0f, 0.0f), Scale(lvN, lvN.x));
            lfLenSq = lvT.x;
        }
        else
        {
            // -(zAxis {0,0,1} - n*n.z) (table 0x820F10F0, sign via `vxor` with -0.0); the
            // console takes lenSq from the PRE-FLIP vector's z lane (`vspltw v3,2` == 1 - n.z^2).
            const V3 lvZ = Sub(MakeV3(0.0f, 0.0f, 1.0f), Scale(lvN, lvN.z));
            lfLenSq = lvZ.z;
            lvT     = Scale(lvZ, -1.0f);
        }

        // ONE vrsqrtefp estimate of the chosen squared length -- see the banner note.
        const f32 lfInvLen = 1.0f / Sqrt(lfLenSq);

        const V3 lvUp  = Scale(Cross(lvT, lvN), lfInvLen);   // frame row 1
        const V3 lvAt  = Scale(lvT, lfInvLen);               // frame row 2

        // Point-relative velocity, B minus A (`vsubfp v2, v9, v2` at 0x828A3834).
        const V3 lvVel = Sub(Add(Xyz(lpBodyB->mVel), Cross(Xyz(lpBodyB->mOmega), lvRB)),
                             Add(Xyz(lpBodyA->mVel), Cross(Xyz(lpBodyA->mOmega), lvRA)));

        // ---- the fill -------------------------------------------------------------------
        mPosA.x = lvPosA.x; mPosA.y = lvPosA.y; mPosA.z = lvPosA.z;   // full event row, 0x828A37B8
        mBodyA = 0;                                             // dead until the batch mint
        mPosB.x = lvPosB.x; mPosB.y = lvPosB.y; mPosB.z = lvPosB.z;   // 0x828A37BC
        mBodyB = 0;
        mRi.x = lvN.x; mRi.y = lvN.y; mRi.z = lvN.z;            // 0x828A38E4
        mRes   = lfRestitution;                                 // event +0x48, 0x828A39B4
        mUp.x = lvUp.x; mUp.y = lvUp.y; mUp.z = lvUp.z;         // 0x828A3900
        mMus   = lfStaticFriction;                              // event +0x40, 0x828A39A4
        mAt.x = lvAt.x; mAt.y = lvAt.y; mAt.z = lvAt.z;         // 0x828A390C
        mMud   = lfDynamicFriction;                             // event +0x44, 0x828A39A8
        mVel.x = lvVel.x; mVel.y = lvVel.y; mVel.z = lvVel.z;      // 0x828A3928
        mTag   = luTag;                                         // the CALLER's loop index

        // ---- the snapshot (ten full row copies, 0x828A3930..0x828A3990) -----------------
        mComA.x = lpBodyA->mCom.x; mComA.y = lpBodyA->mCom.y; mComA.z = lpBodyA->mCom.z;
        mIdA     = lpBodyA->mId;
        mComB.x = lpBodyB->mCom.x; mComB.y = lpBodyB->mCom.y; mComB.z = lpBodyB->mCom.z;
        mIdB     = lpBodyB->mId;
        mIfullA.x = lpBodyA->mIfull.x; mIfullA.y = lpBodyA->mIfull.y; mIfullA.z = lpBodyA->mIfull.z;
        mInvmA   = lpBodyA->mInvm;
        mIfullB.x = lpBodyB->mIfull.x; mIfullB.y = lpBodyB->mIfull.y; mIfullB.z = lpBodyB->mIfull.z;
        mInvmB   = lpBodyB->mInvm;
        mIspltA.x = lpBodyA->mIsplt.x; mIspltA.y = lpBodyA->mIsplt.y; mIspltA.z = lpBodyA->mIsplt.z;
        mStateA  = static_cast<u32>(lpBodyA->mState);
        mIspltB.x = lpBodyB->mIsplt.x; mIspltB.y = lpBodyB->mIsplt.y; mIspltB.z = lpBodyB->mIsplt.z;
        mStateB  = static_cast<u32>(lpBodyB->mState);
        mForceA.x = lpBodyA->mForce.x; mForceA.y = lpBodyA->mForce.y; mForceA.z = lpBodyA->mForce.z;
        mKineA   = lpBodyA->mKine;
        mForceB.x = lpBodyB->mForce.x; mForceB.y = lpBodyB->mForce.y; mForceB.z = lpBodyB->mForce.z;
        mKineB   = lpBodyB->mKine;
        mTorqueA.x = lpBodyA->mTorque.x; mTorqueA.y = lpBodyA->mTorque.y; mTorqueA.z = lpBodyA->mTorque.z;
        mCoolA   = lpBodyA->mCool;
        mTorqueB.x = lpBodyB->mTorque.x; mTorqueB.y = lpBodyB->mTorque.y; mTorqueB.z = lpBodyB->mTorque.z;
        mCoolB   = lpBodyB->mCool;

        // Carry the body pointers in the event position w lanes.
        mpBodyA = lpBodyA;
        mpBodyB = lpBodyB;
    }
};

// ---- the layout pins -- console offsets, X360-attested at the cited stores ----------------
CGS_STATIC_ASSERT((offsetof(Contact, mPosA)    == 0x00), "mPosA @+0x00    (drain stvx event row)");
CGS_STATIC_ASSERT((offsetof(Contact, mBodyA)   == 0x0C), "mBodyA @+0x0C   (batch vsel w-lane mint)");
CGS_STATIC_ASSERT((offsetof(Contact, mPosB)    == 0x10), "mPosB @+0x10");
CGS_STATIC_ASSERT((offsetof(Contact, mBodyB)   == 0x1C), "mBodyB @+0x1C");
CGS_STATIC_ASSERT((offsetof(Contact, mRi)      == 0x20), "mRi @+0x20      (the normal)");
CGS_STATIC_ASSERT((offsetof(Contact, mRes)     == 0x2C), "mRes @+0x2C     (drain stfs e+0x48)");
CGS_STATIC_ASSERT((offsetof(Contact, mUp)      == 0x30), "mUp @+0x30");
CGS_STATIC_ASSERT((offsetof(Contact, mMus)     == 0x3C), "mMus @+0x3C     (drain stfs e+0x40)");
CGS_STATIC_ASSERT((offsetof(Contact, mAt)      == 0x40), "mAt @+0x40");
CGS_STATIC_ASSERT((offsetof(Contact, mMud)     == 0x4C), "mMud @+0x4C     (drain stfs e+0x44)");
CGS_STATIC_ASSERT((offsetof(Contact, mVel)     == 0x50), "mVel @+0x50");
CGS_STATIC_ASSERT((offsetof(Contact, mTag)     == 0x5C), "mTag @+0x5C     (drain stw loop index)");
CGS_STATIC_ASSERT((offsetof(Contact, mComA)    == 0x60), "snapshot comA @+0x60   (body +0x10 row)");
CGS_STATIC_ASSERT((offsetof(Contact, mIdA)     == 0x6C), "snapshot idA @+0x6C    (console mCom.w)");
CGS_STATIC_ASSERT((offsetof(Contact, mComB)    == 0x70), "snapshot comB @+0x70");
CGS_STATIC_ASSERT((offsetof(Contact, mIdB)     == 0x7C), "snapshot idB @+0x7C");
CGS_STATIC_ASSERT((offsetof(Contact, mIfullA)  == 0x80), "snapshot IfullA @+0x80 (body +0x70 row)");
CGS_STATIC_ASSERT((offsetof(Contact, mInvmA)   == 0x8C), "snapshot invmA @+0x8C  (console mIfull.w)");
CGS_STATIC_ASSERT((offsetof(Contact, mIfullB)  == 0x90), "snapshot IfullB @+0x90");
CGS_STATIC_ASSERT((offsetof(Contact, mInvmB)   == 0x9C), "snapshot invmB @+0x9C");
CGS_STATIC_ASSERT((offsetof(Contact, mIspltA)  == 0xA0), "snapshot IspltA @+0xA0 (body +0x80 row)");
CGS_STATIC_ASSERT((offsetof(Contact, mStateA)  == 0xAC), "snapshot stateA @+0xAC (the batch's ACTIVE-test lane)");
CGS_STATIC_ASSERT((offsetof(Contact, mIspltB)  == 0xB0), "snapshot IspltB @+0xB0");
CGS_STATIC_ASSERT((offsetof(Contact, mStateB)  == 0xBC), "snapshot stateB @+0xBC");
CGS_STATIC_ASSERT((offsetof(Contact, mForceA)  == 0xC0), "snapshot forceA @+0xC0 (body +0x90 row)");
CGS_STATIC_ASSERT((offsetof(Contact, mKineA)   == 0xCC), "snapshot kineA @+0xCC  (console mForce.w)");
CGS_STATIC_ASSERT((offsetof(Contact, mForceB)  == 0xD0), "snapshot forceB @+0xD0");
CGS_STATIC_ASSERT((offsetof(Contact, mKineB)   == 0xDC), "snapshot kineB @+0xDC");
CGS_STATIC_ASSERT((offsetof(Contact, mTorqueA) == 0xE0), "snapshot torqueA @+0xE0 (body +0xA0 row)");
CGS_STATIC_ASSERT((offsetof(Contact, mCoolA)   == 0xEC), "snapshot coolA @+0xEC  (console mTorque.w)");
CGS_STATIC_ASSERT((offsetof(Contact, mTorqueB) == 0xF0), "snapshot torqueB @+0xF0");
CGS_STATIC_ASSERT((offsetof(Contact, mCoolB)   == 0xFC), "snapshot coolB @+0xFC");
CGS_STATIC_ASSERT((offsetof(Contact, mpBodyA) == 0x0C && offsetof(Contact, mpBodyB) == 0x1C),
                  "ARTIST drain pointer lanes");
CGS_STATIC_ASSERT((sizeof(Contact) == 0x100), "ARTIST contact stride");

// =====================================================================================
// rw::physics::ContactJacobian -- THE SAME 256 BYTES AFTER ContactBatchBuild @0x82BC14C0
// HAS REWRITTEN THEM IN PLACE. The console reuses the storage with entirely different row
// meanings; this overlay names the post-batch lanes so Anubis/Horus and SpyContactJacobians
// can access them BY NAME. ContactBatchBuild reads every drain field it needs into locals,
// then writes every member below (write-cover verified against the X360 store map).
//
// SLOT MAP -- every offset X360-attested (batch stores; Anubis @0x82BC11C0 loads; spy
// @0x82BC4138 loads). XB1's slots DIFFER (its up row is +0x80 where X360's is +0xA0, etc.)
// and none are imported:
//   +0x00 {rA.xyz,  mBodyA = snapshot mIdA (vsel mint)}      +0x10 {rB.xyz, mBodyB}
//   +0x20 {Jx.xyz,  mFlags = (stateA|stateB) & SPY_BODY}     Jx/Jy/Jz = the x/y/z
//   +0x30 {Jy.xyz,  mMus (drain +0x3C, PRESERVED)}           components of the n/up/at
//   +0x40 {Jz.xyz,  mMud (drain +0x4C, PRESERVED)}           triple, each lane pre-divided
//                                                            by its effective mass
//   +0x50 {mLambda.xyz, mLambdaPos}  impulse accumulators, ZEROED by the batch; lanes =
//                                    {normal, tangent1, tangent2, positional-normal}
//   +0x60 {mBias.xyz, mBiasPos}      pre-divided bias impulses; w = penetration/dn
//   +0x70 {n.xyz,   RigidBody* A}                         the contact frame rows
//   +0x80 {TnA.xyz, mInvmA (0 unless A ACTIVE)}              T** = I^-1.(r x frame-row),
//   +0x90 {TnB.xyz, mInvmB}                                  zero rows for non-ACTIVE
//   +0xA0 {up.xyz,  RigidBody* B}                         bodies (the console's
//   +0xB0 {TupA.xyz, dead}                                   branchless vsel masking; XB1
//   +0xC0 {TupB.xyz, dead}                                   branches and SWAPS instead --
//   +0xD0 {at.xyz,  mTag (drain +0x5C, preserved)}           same fixed point)
//   +0xE0 {TatA.xyz, dead}
//   +0xF0 {TatB.xyz, dead}
// =====================================================================================
struct CGS_ALIGN(16) ContactJacobian
{
    typedef Contact::Vector3U_32 Vector3U_32;

    Vector3U_32 mRA;        // +0x00  posA - comA
    s32         mBodyA;     // +0x0C  reaction-force block index (= drain mIdA)
    Vector3U_32 mRB;        // +0x10
    s32         mBodyB;     // +0x1C
    Vector3U_32 mJx;        // +0x20  {n.x/dn, up.x/dup, at.x/dat}
    u32         mFlags;     // +0x2C  (stateA|stateB) & SPY_BODY -- the spy's gate
    Vector3U_32 mJy;        // +0x30  {n.y/dn, up.y/dup, at.y/dat}
    f32         mMus;       // +0x3C  static friction, preserved from the drain record
    Vector3U_32 mJz;        // +0x40
    f32         mMud;       // +0x4C  dynamic friction, preserved
    Vector3U_32 mLambda;    // +0x50  {normal, tangent1, tangent2} accumulated impulses
    f32         mLambdaPos; // +0x5C  positional (split) normal impulse accumulator
    Vector3U_32 mBias;      // +0x60  pre-divided bias impulses (restitution folded into .x)
    f32         mBiasPos;   // +0x6C  penetration / dn
    Vector3U_32 mRi;        // +0x70  the contact normal
    union { f32 mDeadA; RigidBody* mpBodyA; }; // +0x7C relocated body A
    Vector3U_32 mTnA;       // +0x80  I_A^-1 . (rA x n)
    f32         mInvmA;     // +0x8C  bodyA inverse mass (0 unless ACTIVE)
    Vector3U_32 mTnB;       // +0x90
    f32         mInvmB;     // +0x9C
    Vector3U_32 mUp;        // +0xA0  frame row 1
    union { f32 mDeadB; RigidBody* mpBodyB; }; // +0xAC relocated body B
    Vector3U_32 mTupA;      // +0xB0  I_A^-1 . (rA x up)
    f32         mDead0;     // +0xBC
    Vector3U_32 mTupB;      // +0xC0
    f32         mDead1;     // +0xCC
    Vector3U_32 mAt;        // +0xD0  frame row 2
    u32         mTag;       // +0xDC  preserved drain mTag (the spy emits it)
    Vector3U_32 mTatA;      // +0xE0  I_A^-1 . (rA x at)
    f32         mDead2;     // +0xEC
    Vector3U_32 mTatB;      // +0xF0
    f32         mDead3;     // +0xFC
};

CGS_STATIC_ASSERT((offsetof(ContactJacobian, mpBodyA) == 0x7C &&
                   offsetof(ContactJacobian, mpBodyB) == 0xAC), "ARTIST Jacobian pointer lanes");
CGS_STATIC_ASSERT((sizeof(ContactJacobian) == sizeof(Contact)),
              "the batch rewrites the record IN PLACE -- the overlay must cover it exactly");
CGS_STATIC_ASSERT((offsetof(ContactJacobian, mBodyA)   == 0x0C), "post-batch idA @+0x0C");
CGS_STATIC_ASSERT((offsetof(ContactJacobian, mFlags)   == 0x2C), "flags @+0x2C (spy `lwz 0x2C` + `& 8`)");
CGS_STATIC_ASSERT((offsetof(ContactJacobian, mMus)     == 0x3C), "mus preserved @+0x3C (Anubis vspltw)");
CGS_STATIC_ASSERT((offsetof(ContactJacobian, mMud)     == 0x4C), "mud preserved @+0x4C");
CGS_STATIC_ASSERT((offsetof(ContactJacobian, mLambda)  == 0x50), "L accumulator row @+0x50");
CGS_STATIC_ASSERT((offsetof(ContactJacobian, mBias)    == 0x60), "bias row @+0x60");
CGS_STATIC_ASSERT((offsetof(ContactJacobian, mRi)      == 0x70), "normal @+0x70 (spy row 0)");
CGS_STATIC_ASSERT((offsetof(ContactJacobian, mInvmA)   == 0x8C), "invmA @+0x8C (spy midpoint weight)");
CGS_STATIC_ASSERT((offsetof(ContactJacobian, mInvmB)   == 0x9C), "invmB @+0x9C");
CGS_STATIC_ASSERT((offsetof(ContactJacobian, mAt)      == 0xD0), "at @+0xD0 (spy row 2)");
CGS_STATIC_ASSERT((offsetof(ContactJacobian, mTag)     == 0xDC), "tag @+0xDC (spy emit +0x68)");
CGS_STATIC_ASSERT((offsetof(ContactJacobian, mpBodyA) == offsetof(Contact, mpBodyA) + 0x70),
                  "ARTIST batch relocates body A from position to normal w lane");

// -------------------------------------------------------------------------------------------
// Simulation::GetFreeContact (DWARF simulation.h:475) -- bump-allocate the next contact record
// out of m_CJ_Stack, NULL when the frame's budget (m_CT_Max) is spent. Declared in
// simulation.h; defined here because it needs Contact complete. Transcribed from the inline
// in ProcessAddContactQueue @0x828A36B0..0x828A36EC:
//     if (m_CT_Count == m_CT_Max) return NULL;            // `cmplw 0x64 vs 0x7C`
//     lpContact = m_CJ_Stack + (m_CT_Count << 8);         // native sizeof(Contact) == 256
//     ++m_CT_Count;                                       // `stw r8, 0x64`
// -------------------------------------------------------------------------------------------
inline Contact* Simulation::GetFreeContact()
{
    if (m_CT_Count == m_CT_Max)
    {
        return NULL;
    }
    Contact* const lpContact = static_cast<Contact*>(m_CJ_Stack) + m_CT_Count;
    ++m_CT_Count;
    return lpContact;
}

} // namespace physics
} // namespace rw

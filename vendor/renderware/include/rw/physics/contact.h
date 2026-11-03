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
    typedef rw::math::fpu::Vector3Template<f32> Vector3U_32;

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
    // NOT refine here; this path uses the native estimate directly.
    // -------------------------------------------------------------------------------------
    void GenerateFromCollision(RigidBody* lpBodyA, RigidBody* lpBodyB,
                               const rw::math::vpu::Vector3& lrPointOnA,
                               const rw::math::vpu::Vector3& lrPointOnB,
                               const rw::math::vpu::Vector3& lrNormal,
                               f32 lfStaticFriction, f32 lfDynamicFriction,
                               f32 lfRestitution, u32 luTag)
    {
        // Permute masks and constants are the words at ARTIST 0x820F1090..1120.
        static const CGS_ALIGN(16) u32 kXYZ[4] = {~0u, ~0u, ~0u, 0};
        static const CGS_ALIGN(16) u32 kZXY[4] = {0x08090A0B, 0x00010203, 0x04050607, 0x0C0D0E0F};
        static const CGS_ALIGN(16) u32 kYZX[4] = {0x04050607, 0x08090A0B, 0x00010203, 0x0C0D0E0F};
        static const CGS_ALIGN(16) f32 kAxisX[4] = {1.0f, 0.0f, 0.0f, 0.0f};
        static const CGS_ALIGN(16) f32 kAxisZ[4] = {0.0f, 0.0f, 1.0f, 0.0f};
        static const CGS_ALIGN(16) u32 kSign[4] = {0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u};
        static const CGS_ALIGN(16) f32 kSmall[4] = {0.0001f, 0.0001f, 0.0001f, 0.0001f};
        const __vector4 zero = __vspltisw(0);
        const __vector4 pA = lrPointOnA.mV, pB = lrPointOnB.mV, normal = lrNormal.mV;
        const __vector4 rA = __vsubfp(pA, __lvx(&lpBodyA->mCom, 0));
        const __vector4 rB = __vsubfp(pB, __lvx(&lpBodyB->mCom, 0));
        const __vector4 yz = __lvx(kYZX, 0), zx = __lvx(kZXY, 0);
        const __vector4 normalYZ = __vperm(normal, normal, yz);
        const __vector4 normalZX = __vperm(normal, normal, zx);
        const __vector4 aYZ = __vperm(rA, rA, yz), aZX = __vperm(rA, rA, zx);
        const __vector4 bYZ = __vperm(rB, rB, yz), bZX = __vperm(rB, rB, zx);
        // IDA displays VMX VA, VB (addend), VC (multiplier); intrinsics
        // take VA, VC, VB. ARTIST 0x828A37F0..3810 forms normal x r.
        const __vector4 tangentA = __vmaddfp(aZX, normalYZ, __vnmsubfp(aYZ, normalZX, zero));
        const __vector4 tangentB = __vmaddfp(bZX, normalYZ, __vnmsubfp(bYZ, normalZX, zero));
        const __vector4 state = __vspltw(__lvx(&lpBodyA->mIsplt, 0), 3);
        const __vector4 four = __vspltisw(4);
        const __vector4 active = __vcmpequw(__vand(state, four), four);
        const __vector4 candidate = __vsel(tangentB, tangentA, active);
        const __vector4 tx = __vspltw(candidate, 0), ty = __vspltw(candidate, 1), tz = __vspltw(candidate, 2);
        __vector4 lengthSq = __vmaddfp(tx, tx, zero);
        lengthSq = __vmaddfp(ty, ty, lengthSq);
        lengthSq = __vmaddfp(tz, tz, lengthSq);
        const __vector4 nx = __vspltw(normal, 0), nz = __vspltw(normal, 2);
        const __vector4 axisX = __vnmsubfp(normal, nx, __lvx(kAxisX, 0));
        const __vector4 axisZ = __vnmsubfp(normal, nz, __lvx(kAxisZ, 0));
        const __vector4 half = __vcfsx(__vspltisw(1), 1);
        const __vector4 chooseX = __vcmpgefp(__vnmsubfp(nx, nx, half), __lvx(kSign, 0));
        const __vector4 alternative = __vsel(__vxor(axisZ, __lvx(kSign, 0)), axisX, chooseX);
        const __vector4 alternativeSq = __vsel(__vspltw(axisZ, 2), __vspltw(axisX, 0), chooseX);
        const __vector4 chooseCandidate = __vcmpgefp(lengthSq, __lvx(kSmall, 0));
        const __vector4 tangent = __vsel(alternative, candidate, chooseCandidate);
        const __vector4 inverseLength = __vrsqrtefp(__vsel(alternativeSq, lengthSq, chooseCandidate));
        const __vector4 tangentYZ = __vperm(tangent, tangent, yz);
        const __vector4 tangentZX = __vperm(tangent, tangent, zx);
        // ARTIST 0x828A38C8..38EC forms tangent x normal.
        const __vector4 upProduct = __vmaddfp(tangentYZ, normalZX, zero);
        const __vector4 up = __vmulfp(__vnmsubfp(tangentZX, normalYZ, upProduct), inverseLength);
        const __vector4 at = __vmulfp(tangent, inverseLength);

        const __vector4 omegaA = __lvx(&lpBodyA->mOmega, 0), omegaB = __lvx(&lpBodyB->mOmega, 0);
        // ARTIST 0x828A3800..3820 forms velocity + omega x r.
        const __vector4 velA = __vmaddfp(aZX, __vperm(omegaA, omegaA, yz),
                                __vnmsubfp(aYZ, __vperm(omegaA, omegaA, zx), __lvx(&lpBodyA->mVel, 0)));
        const __vector4 velB = __vmaddfp(bZX, __vperm(omegaB, omegaB, yz),
                                __vnmsubfp(bYZ, __vperm(omegaB, omegaB, zx), __lvx(&lpBodyB->mVel, 0)));
        const __vector4 velocity = __vsubfp(velB, velA);
        const __vector4 xyz = __lvx(kXYZ, 0);
        __stvx(pA, &mPosA, 0); mpBodyA = lpBodyA;
        __stvx(pB, &mPosB, 0); mpBodyB = lpBodyB;
        __stvx(__vand(normal, xyz), &mRi, 0); mRes = lfRestitution;
        __stvx(__vand(up, xyz), &mUp, 0); mMus = lfStaticFriction;
        __stvx(__vand(at, xyz), &mAt, 0); mMud = lfDynamicFriction;
        __stvx(__vand(velocity, xyz), &mVel, 0); mTag = luTag;
        __stvx(__lvx(&lpBodyA->mCom, 0), &mComA, 0);
        __stvx(__lvx(&lpBodyB->mCom, 0), &mComB, 0);
        __stvx(__lvx(&lpBodyA->mIfull, 0), &mIfullA, 0);
        __stvx(__lvx(&lpBodyB->mIfull, 0), &mIfullB, 0);
        __stvx(__lvx(&lpBodyA->mIsplt, 0), &mIspltA, 0);
        __stvx(__lvx(&lpBodyB->mIsplt, 0), &mIspltB, 0);
        __stvx(__lvx(&lpBodyA->mForce, 0), &mForceA, 0);
        __stvx(__lvx(&lpBodyB->mForce, 0), &mForceB, 0);
        __stvx(__lvx(&lpBodyA->mTorque, 0), &mTorqueA, 0);
        __stvx(__lvx(&lpBodyB->mTorque, 0), &mTorqueB, 0);
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

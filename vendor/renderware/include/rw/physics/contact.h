#pragma once

// =====================================================================================
// rw::physics::Contact -- one narrow-phase contact record in the simulation's contact
// scratch array (Simulation::m_CJ_Stack); 256 bytes on the console, 272 on the host (the
// pointer tail below). Filled by CgsPhysics::PhysicsSimulationModule::ProcessAddContactQueue
// (which inlines GenerateFromCollision) and consumed IN PLACE by Simulation::
// ContactBatchBuild @0x82BC14C0, which rewrites the rows into the ContactJacobian overlay
// (bottom of this file) that Anubis/Horus iterate and SpyContactJacobians reads.
//
// EATech RenderWare physics. DWARF: DecFIGS SDKs/EATech/include/cmn/rw/physics/contact.h
// (struct :74, members :216..:239, methods :85..:207). The X360 asm is authoritative for
// behaviour and offsets (drain @0x828A3458; batch @0x82BC14C0).
//
// ⭐⭐ LAYOUT: the 16 DWARF rows byte-for-byte (256), PLUS a 16-byte POINTER TAIL = 272 ON
// THE HOST. ⚠️⚠️ CORRECTION 2026-08-06 (the pipelines wave) TO THE "256 BYTES ON THE HOST
// TOO" BANNER THAT STOOD HERE: the claim "this record contains NO pointer" was true of the
// 16 DWARF rows but FALSE of the record's full life cycle. SpyContactJacobians @0x82BC4138
// loads +0x7C/+0xAC (`lwz r7,0x7C / lwz r8,0xAC` @0x82BC422C/0x82BC4224) and DEREFERENCES
// them (`lvx128 v5,r8,0x10` = RigidBody::mCom) to reconstruct the world contact points as
// mCom + rA / mCom + rB -- those lanes are 4-byte RigidBody POINTERS on the console, ridden
// in from the EVENT's mPointOnA.w/mPointOnB.w lanes by the drain's full-row `stvx` and
// RELOCATED (not minted) by ContactBatchBuild's vsel into the +0x70/+0xA0 rows' w lanes.
// An 8-byte host pointer cannot live in a 4-byte lane, so the host record grows a tail --
// EXACTLY the Xbox One move: its drain-time record carries RigidBody* at +0x10/+0x28, its
// batch parks them at +0x90/+0xA8, its record is 272 bytes, and its spy chases [ptr+0x10]
// identically (sub_1409B7390). The host 272 == XB1's 272 is convergence, not import: every
// row offset below is still X360's, and the 16 rows still reproduce the console
// byte-for-byte. All walks use sizeof(Contact); the SimulationWorkspace sizer's console
// `liMaxContacts << 8` term is sizeof(Contact) on the host (fixed in the same commit).
//
// ⭐⭐ THE mBodyA/mBodyB CONTRACT, READ FROM THE CONSUMER. At drain time the console's
// full-row `stvx` of the event's mPointOnA/mPointOnB parks the event's w lanes in
// +0x0C/+0x1C -- dead for the SOLVER (see above for the spy). The SOLVER values are minted
// by ContactBatchBuild @0x82BC14C0, which stores {rA.xyz, comA.w} / {rB.xyz, comB.w} over
// the mPosA/mPosB rows with vsel mask [0x82181650+0x10] = {FFFF,FFFF,FFFF,0000} -- i.e.
// mBodyA/mBodyB := the snapshot mComA/mComB w lanes := RigidBody::mCom.w := RigidBody::mId,
// the reaction-force block index (rigidbody.h:269). On the PC the snapshot carries that
// index as the named mIdA/mIdB members below, GenerateFromCollision writes mBodyA/mBodyB= 0
// (dead until the batch), and ContactBatchBuild (BODIED 2026-08-06, Simulation.cpp) assigns
// mBodyA = mIdA / mBodyB = mIdB when it rewrites the record as a ContactJacobian (below).
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
struct alignas(16) Contact
{
    // The DWARF spells the vector members `RigidBody::Vector3U_32` -- typedef
    // Vector3Template<float>, the PACKED 12-byte triple (rigidbody.h:35), NOT the 16-byte
    // vpu lane register. The 4-byte scalar after each triple completes the 16-byte row.
    struct Vector3U_32 { f32 x, y, z; };

    // ---- the six jacobian-input rows, DWARF names/order (:216..:238) ---------------------
    Vector3U_32 mPosA;      // +0x00  :216  contact point on body A (world)
    s32         mBodyA;     // +0x0C  :218  body A's reaction-force block index -- DEAD at
                            //              drain time; minted by ContactBatchBuild from mIdA
    Vector3U_32 mPosB;      // +0x10  :220  contact point on body B (world)
    s32         mBodyB;     // +0x1C  :222  body B's index -- same contract as mBodyA
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

    // ---- the HOST POINTER TAIL (+0x100, past the console's 256) --------------------------
    // On the console these are the EVENT's mPointOnA.w/mPointOnB.w lanes (4-byte RigidBody*,
    // parked at +0x0C/+0x1C by the drain, relocated to +0x7C/+0xAC by the batch, chased by
    // SpyContactJacobians at [ptr+0x10] = mCom). On XB1 they are the drain's resolved
    // lpBodyA/lpBodyB in widened slots (+0x10/+0x28 -> batch +0x90/+0xA8), which is the
    // proof they equal the bodies the snapshot rows were read from -- so the host fills
    // them in GenerateFromCollision from its own arguments. Spy-only; the solver never
    // reads them.
    RigidBody*  mpBodyA;    // +0x100  host tail (console: event w-lane ride)
    RigidBody*  mpBodyB;    // +0x108

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
    // named-member assignments only; mBodyA/mBodyB get 0 (see the banner's contract note).
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
        mPosA  = Vector3U_32{ lvPosA.x, lvPosA.y, lvPosA.z };   // full event row, 0x828A37B8
        mBodyA = 0;                                             // dead until the batch mint
        mPosB  = Vector3U_32{ lvPosB.x, lvPosB.y, lvPosB.z };   // 0x828A37BC
        mBodyB = 0;
        mRi    = Vector3U_32{ lvN.x, lvN.y, lvN.z };            // 0x828A38E4
        mRes   = lfRestitution;                                 // event +0x48, 0x828A39B4
        mUp    = Vector3U_32{ lvUp.x, lvUp.y, lvUp.z };         // 0x828A3900
        mMus   = lfStaticFriction;                              // event +0x40, 0x828A39A4
        mAt    = Vector3U_32{ lvAt.x, lvAt.y, lvAt.z };         // 0x828A390C
        mMud   = lfDynamicFriction;                             // event +0x44, 0x828A39A8
        mVel   = Vector3U_32{ lvVel.x, lvVel.y, lvVel.z };      // 0x828A3928
        mTag   = luTag;                                         // the CALLER's loop index

        // ---- the snapshot (ten full row copies, 0x828A3930..0x828A3990) -----------------
        mComA    = Vector3U_32{ lpBodyA->mCom.x,    lpBodyA->mCom.y,    lpBodyA->mCom.z    };
        mIdA     = lpBodyA->mId;
        mComB    = Vector3U_32{ lpBodyB->mCom.x,    lpBodyB->mCom.y,    lpBodyB->mCom.z    };
        mIdB     = lpBodyB->mId;
        mIfullA  = Vector3U_32{ lpBodyA->mIfull.x,  lpBodyA->mIfull.y,  lpBodyA->mIfull.z  };
        mInvmA   = lpBodyA->mInvm;
        mIfullB  = Vector3U_32{ lpBodyB->mIfull.x,  lpBodyB->mIfull.y,  lpBodyB->mIfull.z  };
        mInvmB   = lpBodyB->mInvm;
        mIspltA  = Vector3U_32{ lpBodyA->mIsplt.x,  lpBodyA->mIsplt.y,  lpBodyA->mIsplt.z  };
        mStateA  = static_cast<u32>(lpBodyA->mState);
        mIspltB  = Vector3U_32{ lpBodyB->mIsplt.x,  lpBodyB->mIsplt.y,  lpBodyB->mIsplt.z  };
        mStateB  = static_cast<u32>(lpBodyB->mState);
        mForceA  = Vector3U_32{ lpBodyA->mForce.x,  lpBodyA->mForce.y,  lpBodyA->mForce.z  };
        mKineA   = lpBodyA->mKine;
        mForceB  = Vector3U_32{ lpBodyB->mForce.x,  lpBodyB->mForce.y,  lpBodyB->mForce.z  };
        mKineB   = lpBodyB->mKine;
        mTorqueA = Vector3U_32{ lpBodyA->mTorque.x, lpBodyA->mTorque.y, lpBodyA->mTorque.z };
        mCoolA   = lpBodyA->mCool;
        mTorqueB = Vector3U_32{ lpBodyB->mTorque.x, lpBodyB->mTorque.y, lpBodyB->mTorque.z };
        mCoolB   = lpBodyB->mCool;

        // ---- the spy pointers (host tail; console: the event w lanes ride through the
        // rows above). XB1's drain stores exactly these two resolved pointers; see the
        // tail-member banner.
        mpBodyA = lpBodyA;
        mpBodyB = lpBodyB;
    }
};

// ---- the layout pins -- console offsets, X360-attested at the cited stores ----------------
static_assert(sizeof(Contact) == 272,
              "Contact = the 256B console rows + the 16B host pointer tail (the XB1 number, "
              "by convergence). Every walk uses sizeof(Contact) (GetFreeContact, "
              "ContactBatchBuild, the pipelines, SpyContactJacobians) and the "
              "SimulationWorkspace sizer's contact term is sizeof(Contact) -- if this pin "
              "moves, that sizer and every walk move with it IN THE SAME COMMIT");
static_assert(offsetof(Contact, mPosA)    == 0x00, "mPosA @+0x00    (drain stvx event row)");
static_assert(offsetof(Contact, mBodyA)   == 0x0C, "mBodyA @+0x0C   (batch vsel w-lane mint)");
static_assert(offsetof(Contact, mPosB)    == 0x10, "mPosB @+0x10");
static_assert(offsetof(Contact, mBodyB)   == 0x1C, "mBodyB @+0x1C");
static_assert(offsetof(Contact, mRi)      == 0x20, "mRi @+0x20      (the normal)");
static_assert(offsetof(Contact, mRes)     == 0x2C, "mRes @+0x2C     (drain stfs e+0x48)");
static_assert(offsetof(Contact, mUp)      == 0x30, "mUp @+0x30");
static_assert(offsetof(Contact, mMus)     == 0x3C, "mMus @+0x3C     (drain stfs e+0x40)");
static_assert(offsetof(Contact, mAt)      == 0x40, "mAt @+0x40");
static_assert(offsetof(Contact, mMud)     == 0x4C, "mMud @+0x4C     (drain stfs e+0x44)");
static_assert(offsetof(Contact, mVel)     == 0x50, "mVel @+0x50");
static_assert(offsetof(Contact, mTag)     == 0x5C, "mTag @+0x5C     (drain stw loop index)");
static_assert(offsetof(Contact, mComA)    == 0x60, "snapshot comA @+0x60   (body +0x10 row)");
static_assert(offsetof(Contact, mIdA)     == 0x6C, "snapshot idA @+0x6C    (console mCom.w)");
static_assert(offsetof(Contact, mComB)    == 0x70, "snapshot comB @+0x70");
static_assert(offsetof(Contact, mIdB)     == 0x7C, "snapshot idB @+0x7C");
static_assert(offsetof(Contact, mIfullA)  == 0x80, "snapshot IfullA @+0x80 (body +0x70 row)");
static_assert(offsetof(Contact, mInvmA)   == 0x8C, "snapshot invmA @+0x8C  (console mIfull.w)");
static_assert(offsetof(Contact, mIfullB)  == 0x90, "snapshot IfullB @+0x90");
static_assert(offsetof(Contact, mInvmB)   == 0x9C, "snapshot invmB @+0x9C");
static_assert(offsetof(Contact, mIspltA)  == 0xA0, "snapshot IspltA @+0xA0 (body +0x80 row)");
static_assert(offsetof(Contact, mStateA)  == 0xAC, "snapshot stateA @+0xAC (the batch's ACTIVE-test lane)");
static_assert(offsetof(Contact, mIspltB)  == 0xB0, "snapshot IspltB @+0xB0");
static_assert(offsetof(Contact, mStateB)  == 0xBC, "snapshot stateB @+0xBC");
static_assert(offsetof(Contact, mForceA)  == 0xC0, "snapshot forceA @+0xC0 (body +0x90 row)");
static_assert(offsetof(Contact, mKineA)   == 0xCC, "snapshot kineA @+0xCC  (console mForce.w)");
static_assert(offsetof(Contact, mForceB)  == 0xD0, "snapshot forceB @+0xD0");
static_assert(offsetof(Contact, mKineB)   == 0xDC, "snapshot kineB @+0xDC");
static_assert(offsetof(Contact, mTorqueA) == 0xE0, "snapshot torqueA @+0xE0 (body +0xA0 row)");
static_assert(offsetof(Contact, mCoolA)   == 0xEC, "snapshot coolA @+0xEC  (console mTorque.w)");
static_assert(offsetof(Contact, mTorqueB) == 0xF0, "snapshot torqueB @+0xF0");
static_assert(offsetof(Contact, mCoolB)   == 0xFC, "snapshot coolB @+0xFC");
static_assert(offsetof(Contact, mpBodyA)  == 0x100, "spy pointer tail A (host-only region)");
static_assert(offsetof(Contact, mpBodyB)  == 0x108, "spy pointer tail B");

// =====================================================================================
// rw::physics::ContactJacobian -- THE SAME 272 BYTES AFTER ContactBatchBuild @0x82BC14C0
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
//   +0x70 {n.xyz,   dead (console: RigidBody* A)}            the contact frame rows
//   +0x80 {TnA.xyz, mInvmA (0 unless A ACTIVE)}              T** = I^-1.(r x frame-row),
//   +0x90 {TnB.xyz, mInvmB}                                  zero rows for non-ACTIVE
//   +0xA0 {up.xyz,  dead (console: RigidBody* B)}            bodies (the console's
//   +0xB0 {TupA.xyz, dead}                                   branchless vsel masking; XB1
//   +0xC0 {TupB.xyz, dead}                                   branches and SWAPS instead --
//   +0xD0 {at.xyz,  mTag (drain +0x5C, preserved)}           same fixed point)
//   +0xE0 {TatA.xyz, dead}
//   +0xF0 {TatB.xyz, dead}
//   +0x100 mpBodyA / +0x108 mpBodyB  (host tail, carried through from the drain record)
// =====================================================================================
struct alignas(16) ContactJacobian
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
    f32         mDeadA;     // +0x7C  console: RigidBody* A -- host: dead, 0
    Vector3U_32 mTnA;       // +0x80  I_A^-1 . (rA x n)
    f32         mInvmA;     // +0x8C  bodyA inverse mass (0 unless ACTIVE)
    Vector3U_32 mTnB;       // +0x90
    f32         mInvmB;     // +0x9C
    Vector3U_32 mUp;        // +0xA0  frame row 1
    f32         mDeadB;     // +0xAC  console: RigidBody* B -- host: dead, 0
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
    RigidBody*  mpBodyA;    // +0x100  same slots as Contact's tail -- carried through
    RigidBody*  mpBodyB;    // +0x108
};

static_assert(sizeof(ContactJacobian) == sizeof(Contact),
              "the batch rewrites the record IN PLACE -- the overlay must cover it exactly");
static_assert(offsetof(ContactJacobian, mBodyA)   == 0x0C, "post-batch idA @+0x0C");
static_assert(offsetof(ContactJacobian, mFlags)   == 0x2C, "flags @+0x2C (spy `lwz 0x2C` + `& 8`)");
static_assert(offsetof(ContactJacobian, mMus)     == 0x3C, "mus preserved @+0x3C (Anubis vspltw)");
static_assert(offsetof(ContactJacobian, mMud)     == 0x4C, "mud preserved @+0x4C");
static_assert(offsetof(ContactJacobian, mLambda)  == 0x50, "L accumulator row @+0x50");
static_assert(offsetof(ContactJacobian, mBias)    == 0x60, "bias row @+0x60");
static_assert(offsetof(ContactJacobian, mRi)      == 0x70, "normal @+0x70 (spy row 0)");
static_assert(offsetof(ContactJacobian, mInvmA)   == 0x8C, "invmA @+0x8C (spy midpoint weight)");
static_assert(offsetof(ContactJacobian, mInvmB)   == 0x9C, "invmB @+0x9C");
static_assert(offsetof(ContactJacobian, mUp)      == 0xA0, "up @+0xA0 (spy row 1 -- X360 slot, not XB1's +0x80)");
static_assert(offsetof(ContactJacobian, mAt)      == 0xD0, "at @+0xD0 (spy row 2)");
static_assert(offsetof(ContactJacobian, mTag)     == 0xDC, "tag @+0xDC (spy emit +0x68)");
static_assert(offsetof(ContactJacobian, mpBodyA)  == offsetof(Contact, mpBodyA),
              "the tail must not move between the two phases");

// -------------------------------------------------------------------------------------------
// Simulation::GetFreeContact (DWARF simulation.h:475) -- bump-allocate the next contact record
// out of m_CJ_Stack, NULL when the frame's budget (m_CT_Max) is spent. Declared in
// simulation.h; defined here because it needs Contact complete. Transcribed from the inline
// in ProcessAddContactQueue @0x828A36B0..0x828A36EC:
//     if (m_CT_Count == m_CT_Max) return NULL;            // `cmplw 0x64 vs 0x7C`
//     lpContact = m_CJ_Stack + (m_CT_Count << 8);         // console 256; host sizeof(Contact)
//     ++m_CT_Count;                                       // `stw r8, 0x64`      == 272
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

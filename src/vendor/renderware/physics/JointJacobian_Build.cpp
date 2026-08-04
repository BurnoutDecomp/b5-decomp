#include "vendor/renderware/physics/Jacobian.hpp"
#include "vendor/renderware/physics/JacobianMath.hpp"
#include "vendor/renderware/physics/Joint.hpp"
#include "rw/physics/quaternion.h"
#include "rw/physics/simulation.h"

#include <cfloat>   // FLT_MAX

// =====================================================================================
// rw::physics::JointJacobian::Build @0x82BC42E8 -- 873 X360 instructions.
//
// A JOINT is an INEQUALITY constraint, not a servo: its linear half is a prismatic box and
// its angular half is a swing/twist limit cone, and every row carries a [lo, hi] pair rather
// than a bias and a strength. That is the whole structural difference from the drive builder,
// and it is why JointLimits has no mStrength field at all -- the same record lanes the drive
// fills with `mStrength * h^2` clamps, the joint fills with its limit pair.
//
// WITNESSES read instruction for instruction:
//   [X360]  BURNOUT_X360_ARTIST.XEX @0x82BC42E8 -- AUTHORITY for every offset and for what
//           exists. Every offset in this file is X360's.
//   [BPR]   BurnoutPR.exe sub_7E5B10, 1777 insn, x86/SSE -- ALGORITHM ORACLE ONLY.
//           ⛔ ITS RECORD DIVERGES: m_spy at +0xBC stored `<<1` with bit 0 carrying
//           "bodyB is ACTIVE", inverse masses at +0xEC/+0x13C. Import no offset.
//   [XB1]   Burnout_External_Xbox_One.exe sub_1409B0D20 -- x64 ALGORITHM oracle. Its record
//           is 400 bytes, two groups of five at stride 0x50, node an 8-byte field at +0xD0.
//           Import no offset from it either; it is used only to cross-check the SET of the
//           twelve M^-1 J^T products, which it reproduces exactly.
//   [DWARF] references/DecFIGS/.../rw/physics/{joint,jointframes,jointlimits,rigidbody,
//           inertia,simulation}.h.
//
// ⭐ EVERY GAP THE READ-ONLY WAVES LEFT OPEN IS CLOSED, AND NONE OF IT IS GUESSED:
//   * The six "undefined source registers" (v87/v88/v92/v93/v94/v95) were a single systematic
//     IDA VMX128 decode bug -- the printed source register is exactly 32 too high. Proven
//     three ways: all three `vor128` in this function have vA = vB + 32 and become the
//     canonical save-before-reuse `vmr128`; across the whole 30 084-function export set 416 of
//     518 `vor128` have delta +32 with no other delta above 10; and 346 of 389 resolvable
//     phantom reads resolve by -32.
//   * All seven `vperm` control tables were READ out of the X360 image, not inferred.
//   * The per-slot (body, row) label for all twelve M^-1 J^T products was resolved by
//     following every accumulator chain to its last term. Self-check: every cross-product
//     chain uses the inertia rows of the SAME body whose anchor r it was built from -- all
//     six match, which is the check that would have caught a swap.
// =====================================================================================

namespace rw
{
namespace physics
{

using namespace rw::physics::jacobian_detail;

namespace
{
    // `flt_82001D9C` = 0x40000000 = 2.0f -- READ from the X360 image, at the right symbol and
    // the right use site (`lis r20,flt_82001D9C@ha` / `lfs f12,flt_82001D9C@l(r20)`, and f12
    // is the multiplier in exactly the five LOCKED-arm `fmuls`). It is also independently
    // forced by the constraint identity (J, b) == (sJ, sb) against BurnoutPR, which instead
    // scales the RQD-derived AXIS by 0.5f.
    const f32 KF_ANGLE = 2.0f;

    // `flt_821817E0` = 0x3F7FFF58 = 0.999989986f -- the SWING_CONE degeneracy threshold, read
    // from the X360 image and byte-identical to BurnoutPR's `dword_D5DE60` in the same
    // `comiss` against M_Rel(0,0).
    const f32 KF_CONE_DEGENERATE = 0.999989986f;

    // `flt_82035570` = 0xFF7FFFFF and `flt_821815B0[0]` = 0x7F7FFFFF -- the two "no limit"
    // sentinels that seed the angular lo/hi pair. Also corroborated by use: a value that is
    // later `min`'d must be the upper sentinel.
    inline V3 Splat(f32 lfV) { return MakeV3(lfV, lfV, lfV); }
}

void JointJacobian::Build(const Joint& lrJoint, Simulation* lpSim)
{
    const JointFrames& lrF   = *lrJoint.m_skel;    // `lwz r26,0(r27)`
    const JointLimits& lrLim = *lrJoint.m_limit;   // `lwz r28,4(r27)`
    RigidBody&         lrA   = *lrJoint.m_bodyA;   // `lwz r29,0x10(r27)`
    RigidBody&         lrB   = *lrJoint.m_bodyB;   // `lwz r30,0x14(r27)`
    const f32          lfH   = lpSim->GetTimeStep();   // the ONLY field read from Simulation

    // =================================================================================
    // BLOCK 1 -- 0x82BC42E8..0x82BC44D4   PROLOGUE.   [X360 + BPR]  VERIFIED
    // =================================================================================
    Quat lqA = QuatMul(lrA.mQuat, lrF.GetChildAngularFrame());     // qA'
    Quat lqB = QuatMul(lrB.mQuat, lrF.GetParentAngularFrame());    // qB'
    // ** THE THIRD COMPOSITION USES BODY B, ON BOTH BUILDS. ** X360 `lvx128 v13,r26,r25` with
    // r25 = 0x40 = JointFrames::mQuatL over a base of [r30] = body B; BurnoutPR `[esi+40h]`
    // with edx = [ebp-424h] = m_bodyB. This is what mQuatL is FOR: the frame the LINEAR
    // constraint is expressed in.
    Quat lqL = QuatMul(lrB.mQuat, lrF.GetParentLinearFrame());     // qL'

    // The 64-byte RQD destination is pre-filled with `flt_82001CC0` = 0.0f by 16 `stfs`
    // before the call. Dead -- Create writes all 16 slots.
    Jacobian_RQD lRQD;
    lRQD.Create(&lqA, &lqB);   // = L(conj qA') . R(qB')

    // ⭐ The code then gathers the .w COLUMN of RQD into one 16-byte local and feeds THAT to
    // UnitQuaternionToMatrix (X360 0x82BC448C..0x82BC44AC). Only a unit quaternion can go
    // there, and L(conj qA').R(qB') makes that column exactly conj(qA') (x) qB'. That is an
    // independent, in-code confirmation of Jacobian_RQD::Create's closed form.
    Quat lqRel = { lRQD.RowW(0u), lRQD.RowW(1u), lRQD.RowW(2u), lRQD.RowW(3u) };

    M33 lM_A, lM_B, lM_L, lM_Rel;
    Quaternion::UnitQuaternionToMatrix(&lM_A,   &lqA);
    Quaternion::UnitQuaternionToMatrix(&lM_B,   &lqB);
    Quaternion::UnitQuaternionToMatrix(&lM_L,   &lqL);
    Quaternion::UnitQuaternionToMatrix(&lM_Rel, &lqRel);

    // =================================================================================
    // BLOCK 2 -- 0x82BC44D8..0x82BC46FC   ANCHORS, PREDICTED MOTION, PRISMATIC BOX.
    //            [X360 + BPR 0x7E5FB9..0x7E624F]  VERIFIED on both.
    // =================================================================================
    const V3 lvPosA = Xyz(lrF.GetChildPosition());
    const V3 lvPosB = Xyz(lrF.GetParentPosition());
    const V3 lvRA = Add(Add(Scale(Xyz(lrA.mRi), lvPosA.x), Scale(Xyz(lrA.mUp), lvPosA.y)),
                        Scale(Xyz(lrA.mAt), lvPosA.z));
    const V3 lvRB = Add(Add(Scale(Xyz(lrB.mRi), lvPosB.x), Scale(Xyz(lrB.mUp), lvPosB.y)),
                        Scale(Xyz(lrB.mAt), lvPosB.z));

    // Unlike the drive builder, the joint folds velocity and acceleration into ONE predicted
    // velocity -- it has no separate gains to apply to them.
    // (`cross(mOmega, r)`, not the transpose -- the sign is read off tables 0x82181680 = YZX
    // and 0x82181670 = ZXY, and BurnoutPR's `pshufd 9`/`12h` pair agrees.)
    const V3 lvVelA = Add(Add(Xyz(lrA.mVel), Cross(Xyz(lrA.mOmega), lvRA)),
                          Scale(Add(Xyz(lrA.mForce), Cross(Xyz(lrA.mTorque), lvRA)), lfH));
    const V3 lvVelB = Add(Add(Xyz(lrB.mVel), Cross(Xyz(lrB.mOmega), lvRB)),
                          Scale(Add(Xyz(lrB.mForce), Cross(Xyz(lrB.mTorque), lvRB)), lfH));

    const V3 lvRelVel = Sub(lvVelB, lvVelA);
    const V3 lvErr    = Sub(Add(Xyz(lrB.mCom), lvRB), Add(Xyz(lrA.mCom), lvRA));
    const V3 lvDisp   = Add(lvErr, Scale(lvRelVel, lfH));   // predicted end-of-step offset

    // Project both onto the LINEAR frame -- the three rows of M_L.
    const V3 lvLocalVel  = Project(lM_L, lvRelVel);
    const V3 lvLocalDisp = Project(lM_L, lvDisp);

    // The prismatic limit box. X360 0x82BC4690..0x82BC46D8 == BPR 0x7E6222..0x7E624F, the
    // same five operations in the same order.
    //     lo = clamp((v - V)*h, disp - P, disp + P)      P = mPprism, V = mVprism
    //     hi = clamp((v + V)*h, disp - P, disp + P)
    const V3 lvPrism   = Xyz(lrLim.GetPositionLimit());
    const V3 lvVPrism  = Xyz(lrLim.GetLinearVelocityLimit());
    const V3 lvLoBoxL  = Sub(lvLocalDisp, lvPrism);
    const V3 lvHiBoxL  = Add(lvLocalDisp, lvPrism);
    const V3 lvLinLo   = Max3(lvLoBoxL, Min3(Scale(Sub(lvLocalVel, lvVPrism), lfH), lvHiBoxL));
    const V3 lvLinHi   = Min3(lvHiBoxL, Max3(Scale(Add(lvLocalVel, lvVPrism), lfH), lvLoBoxL));

    // =================================================================================
    // BLOCK 3 -- 0x82BC4700..0x82BC49DC   switch (mSwingf)  -- 5 arms.
    // BLOCK 4 -- 0x82BC49E0..0x82BC4AC0   switch (mTwistf)  -- 3 arms.
    //            [X360 all 15 paths] VERIFIED; 8 of them cross-read on BurnoutPR.
    //
    // ⭐ THE THREE ANGULAR LANES ARE NAMED BY THE DATA. Block 5 assembles the angular velocity
    // limit as (mVtwist, mVswing, mVswing) -- X360 `lfs 0x20/0x24(r28)` into three stack
    // slots, BurnoutPR an `unpcklps` chain over [ecx+20h]/[ecx+24h].
    // ⇒ lane x = TWIST, lanes y and z = the two SWING rows.
    // =================================================================================
    V3 lvAxisT  = MakeV3(0.0f, 0.0f, 0.0f);
    V3 lvAxisS1 = MakeV3(0.0f, 0.0f, 0.0f);
    V3 lvAxisS2 = MakeV3(0.0f, 0.0f, 0.0f);
    V3 lvAngLoRaw = Splat(-FLT_MAX);   // flt_82035570
    V3 lvAngHiRaw = Splat(+FLT_MAX);   // flt_821815B0[0]

    SwingType leSwing = lrLim.GetSwingType();

    // SWING_CONE degenerates to SWING_FREE when the two reference axes are (near) parallel:
    // X360 compares M_Rel(0,0) against flt_821817E0 and branches straight into the FREE arm.
    if (leSwing == SWING_CONE && lM_Rel.xAxis.x >= KF_CONE_DEGENERATE)
        leSwing = SWING_FREE;

    switch (leSwing)
    {
    case SWING_LOCKED:                                   // loc_82BC4978
        lvAngLoRaw.y = lvAngHiRaw.y = KF_ANGLE * lRQD.RowW(1u);
        lvAngLoRaw.z = lvAngHiRaw.z = KF_ANGLE * lRQD.RowW(2u);
        lvAxisS1 = MakeV3(lRQD.RowX(1u), lRQD.RowY(1u), lRQD.RowZ(1u));
        lvAxisS2 = MakeV3(lRQD.RowX(2u), lRQD.RowY(2u), lRQD.RowZ(2u));
        break;

    case SWING_CONE:                                     // loc_82BC4884
        {
            const f32 lfS2 = lM_Rel.yAxis.x * lM_Rel.yAxis.x + lM_Rel.zAxis.x * lM_Rel.zAxis.x;
            const f32 lfR  = 1.0f / Sqrt(lfS2);
            lvAxisS1 = Scale(Cross(lM_A.xAxis, lM_B.xAxis), lfR);
            lvAxisS2 = Cross(lvAxisS1, lM_A.xAxis);
            // A ONE-SIDED lower bound: the cone only limits how far apart the axes may open.
            lvAngLoRaw.y = (lrLim.GetSwingLimit() - lM_Rel.xAxis.x) * lfR;
        }
        break;

    case SWING_HINGE:                                    // loc_82BC47B4
    case SWING_AXLE:
        // ⚠️ BOTH arms load swing axis 1 = M_B row 1 BEFORE their twist sub-switch (X360 lines
        // 279-280 for AXLE, 310-311 for HINGE) -- easy to miss, and a missing axis would
        // silently null a constraint row.
        lvAxisS1 = lM_B.yAxis;
        if (lrLim.GetTwistType() == TWIST_LOCKED)
        {
            lvAngLoRaw.z = lvAngHiRaw.z = KF_ANGLE * lRQD.RowW(2u);
            lvAxisS2 = MakeV3(lRQD.RowX(2u), lRQD.RowY(2u), lRQD.RowZ(2u));
        }
        else
        {
            // X360 negates with `vspltisw -1` + `vslw` (a 0x80000000 sign mask) + `vxor`;
            // BurnoutPR with `xorps xmm3,xmm3` + `subps`. Both are a unary minus.
            lvAngLoRaw.z = lvAngHiRaw.z = -lM_Rel.yAxis.x;
            lvAxisS2 = Cross(lM_A.xAxis, lM_B.yAxis);
        }
        if (lrLim.GetSwingType() == SWING_HINGE)         // loc_82BC483C
        {
            const f32 lfD = lM_Rel.zAxis.x;
            if (lfD != 0.0f)                             // the `fcmpu` guard vs 0.0f
            {
                const f32 lfV = (lrLim.GetSwingLimit() - lM_Rel.xAxis.x) / lfD;
                // The SIGN OF THE DENOMINATOR flips the inequality direction.
                if (lfD >= 0.0f) lvAngLoRaw.y = lfV;
                else             lvAngHiRaw.y = lfV;
            }
        }
        break;

    case SWING_FREE:                                     // loc_82BC4894
    default:
        lvAxisS1 = lM_B.yAxis;
        lvAxisS2 = lM_B.zAxis;
        break;                                           // bounds stay -+FLT_MAX
    }

    switch (lrLim.GetTwistType())                        // loc_82BC49E0
    {
    case TWIST_LOCKED:                                   // loc_82BC4A90
        lvAngLoRaw.x = lvAngHiRaw.x = KF_ANGLE * lRQD.RowW(0u);
        lvAxisT = MakeV3(lRQD.RowX(0u), lRQD.RowY(0u), lRQD.RowZ(0u));
        break;

    case TWIST_ARC:                                      // loc_82BC49F8
        lvAxisT = lM_A.xAxis;
        {
            const f32 lfD = lM_Rel.yAxis.z - lM_Rel.zAxis.y;
            if (lfD != 0.0f)
            {
                const f32 lfV = ((lM_Rel.xAxis.x + 1.0f) * lrLim.GetTwistLimit()
                                 - (lM_Rel.yAxis.y + lM_Rel.zAxis.z)) / lfD;
                if (lfD >= 0.0f) lvAngLoRaw.x = lfV;
                else             lvAngHiRaw.x = lfV;
            }
        }
        break;

    case TWIST_FREE:
    default:
        lvAxisT = lM_B.xAxis;
        break;
    }

    // =================================================================================
    // BLOCK 5 -- 0x82BC4AC4..0x82BC4B30   ANGULAR PROJECTION + THE SAME CLAMP SHAPE.
    //            [X360 0x82BC4FB0..0x82BC5014] == [BPR 0x7E6A18..0x7E6A82]  VERIFIED both.
    //
    // ⭐ ONE FORMULA, TWO HALVES: this is literally the prismatic box of block 2 again, with
    // (lo, hi) = (raw + w.h, raw + w.h) instead of (disp - P, disp + P).
    // =================================================================================
    const M33 lAngFrame = { lvAxisT, lvAxisS1, lvAxisS2 };
    const V3  lvRelOmega = Add(Sub(Xyz(lrB.mOmega), Xyz(lrA.mOmega)),
                               Scale(Sub(Xyz(lrB.mTorque), Xyz(lrA.mTorque)), lfH));
    const V3  lvLocalOmega = Project(lAngFrame, lvRelOmega);
    const V3  lvVAng = Xyz(lrLim.GetAngularVelocityLimit());   // (mVtwist, mVswing, mVswing)

    const V3 lvAngLoPred = Add(lvAngLoRaw, Scale(lvLocalOmega, lfH));
    const V3 lvAngHiPred = Add(lvAngHiRaw, Scale(lvLocalOmega, lfH));
    const V3 lvAngLo = Max3(lvAngLoPred, Min3(Scale(Sub(lvLocalOmega, lvVAng), lfH), lvAngHiPred));
    const V3 lvAngHi = Min3(lvAngHiPred, Max3(Scale(Add(lvLocalOmega, lvVAng), lfH), lvAngLoPred));

    // =================================================================================
    // BLOCK 6 -- 0x82BC4B34..0x82BC4EBC   RECORD HEAD + PER-BODY M^-1 J^T.
    // =================================================================================
    // ⭐ The joint builder zeroes the ENTIRE 384-byte record first (three `dcbz128` at +0x00 /
    // +0x80 / +0x100); the drive builder does not. That is what covers the lanes neither
    // builder writes -- +0x5C in particular, which the drive leaves genuinely untouched.
    for (u32 luRow = 0u; luRow < 24u; ++luRow)
    {
        mRows[luRow].x = 0.0f; mRows[luRow].y = 0.0f;
        mRows[luRow].z = 0.0f; mRows[luRow].w = 0.0f;
    }
    mIdA = 0u; mIdB = 0u; mSpy = 0u; mpNode = 0;

    // Branchless ACTIVE test on the console: `vspltw(mIsplt,3) & 4 == 4` -> a `vcmpequw` lane
    // mask, then `vsel(0, mIfull, mask)` / `vsel(0, mIsplt, mask)`. Zeroing mIfull also zeroes
    // its w lane (the inverse mass), so one operation gives both "infinite mass" and "zero
    // inverse inertia".
    const bool lbActiveA = (lrA.mState & ACTIVE_BODY) != 0;
    const bool lbActiveB = (lrB.mState & ACTIVE_BODY) != 0;
    const f32  lfInvMassA = lbActiveA ? lrA.mInvm : 0.0f;
    const f32  lfInvMassB = lbActiveB ? lrB.mInvm : 0.0f;
    const M33  lInvIA = lbActiveA ? UnpackInverseInertia(lrA.mIfull, lrA.mIsplt) : ZeroMatrix33();
    const M33  lInvIB = lbActiveB ? UnpackInverseInertia(lrB.mIfull, lrB.mIsplt) : ZeroMatrix33();

    // Slots 0 and 1: the anchor arms with the reaction-force ids in their w lanes. The console
    // builds them with `vsel(mCom, r, tbl@0x82181660)` = (r.xyz, mCom.w) and RigidBody+0x1C
    // is mId -- so rA pairs with A's id, which is an independent corroboration of rA/rB.
    mRows[0].x = lvRA.x; mRows[0].y = lvRA.y; mRows[0].z = lvRA.z;   // +0x00
    mIdA = lrA.mId;                                                  // +0x0C
    mRows[1].x = lvRB.x; mRows[1].y = lvRB.y; mRows[1].z = lvRB.z;   // +0x10
    mIdB = lrB.mId;                                                  // +0x1C
    // +0x20 and +0x30 are stored as an explicit zero vector (already zeroed above).

    // The three linear constraint axes are the rows of M_L; the angular ones are the triple
    // the two switches selected.
    V3  lavCrossA[3], lavCrossB[3], lavAngA[3], lavAngB[3];
    f32 lafMassL[3],  lafMassA[3];
    for (u32 luI = 0u; luI < 3u; ++luI)
    {
        const V3 lvLinAxis = Row(lM_L, luI);
        const V3 lvAngAxis = Row(lAngFrame, luI);

        const V3 lvCA = Cross(lvRA, lvLinAxis);
        const V3 lvCB = Cross(lvRB, lvLinAxis);
        lavCrossA[luI] = Transform(lInvIA, lvCA);
        lavCrossB[luI] = Transform(lInvIB, lvCB);
        lavAngA[luI]   = Transform(lInvIA, lvAngAxis);
        lavAngB[luI]   = Transform(lInvIB, lvAngAxis);

        lafMassL[luI] = lfInvMassA + lfInvMassB
                      + Dot3(lvCA, lavCrossA[luI]) + Dot3(lvCB, lavCrossB[luI]);
        lafMassA[luI] = Dot3(lvAngAxis, lavAngA[luI]) + Dot3(lvAngAxis, lavAngB[luI]);
    }

    // ⭐ THE RECORD IS THREE IDENTICAL 0x40-BYTE GROUPS, base 0xC0 + 0x40*i:
    //     +0x00 : ( L_i                 , [i=0: &joint | i=1: A.mInvm | i=2: B.mInvm] )
    //     +0x10 : ( I_A^-1 . (rA x L_i) , (I_B^-1 . angAxis_i).x )
    //     +0x20 : ( I_A^-1 . angAxis_i  , (I_B^-1 . angAxis_i).y )
    //     +0x30 : ( I_B^-1 . (rB x L_i) , (I_B^-1 . angAxis_i).z )
    // = 3 linear axes + 12 M^-1 J^T products (nine in xyz, three spread over the w lanes),
    // six per body. ⚠️ The w spread carries I_**B**^-1 . angAxis_i in group i, NOT I_A^-1.
    // The Xbox One build confirms the SET (its body-A group is exactly
    // cross(rA,L0..L2) + angAxis0..2 through I_A^-1) but PACKS IT DIFFERENTLY -- two groups of
    // five at stride 0x50, w-spreading I_A^-1.angAxis0 inside A's own group. Same twelve,
    // different placement. The placement here is X360's.
    for (u32 luI = 0u; luI < 3u; ++luI)
    {
        const u32 luBase = 12u + luI * 4u;      // rows 12, 16, 20 -> +0xC0 / +0x100 / +0x140
        const V3  lvLinAxis = Row(lM_L, luI);

        mRows[luBase + 0u].x = lvLinAxis.x;
        mRows[luBase + 0u].y = lvLinAxis.y;
        mRows[luBase + 0u].z = lvLinAxis.z;

        mRows[luBase + 1u].x = lavCrossA[luI].x;
        mRows[luBase + 1u].y = lavCrossA[luI].y;
        mRows[luBase + 1u].z = lavCrossA[luI].z;
        mRows[luBase + 1u].w = lavAngB[luI].x;

        mRows[luBase + 2u].x = lavAngA[luI].x;
        mRows[luBase + 2u].y = lavAngA[luI].y;
        mRows[luBase + 2u].z = lavAngA[luI].z;
        mRows[luBase + 2u].w = lavAngB[luI].y;

        mRows[luBase + 3u].x = lavCrossB[luI].x;
        mRows[luBase + 3u].y = lavCrossB[luI].y;
        mRows[luBase + 3u].z = lavCrossB[luI].z;
        mRows[luBase + 3u].w = lavAngB[luI].z;
    }
    mpNode      = const_cast<Joint*>(&lrJoint);   // +0xCC  `stw r27,0xCC(r31)`
    mRows[16].w = lfInvMassA;                     // +0x10C
    mRows[20].w = lfInvMassB;                     // +0x14C

    // =================================================================================
    // BLOCK 7 -- 0x82BC4EC0..0x82BC5088   TAIL.
    // =================================================================================
    // A `vmrghw`/`vmrglw` 4x4 transpose feeds the two accumulated effective masses, each
    // inverted once with `vrefp`. ⚠️ THE ANGULAR RECIPROCAL IS THEN HALVED (X360 line 834
    // multiplies by [flt_821815B0+0x210], which reads 0.5f x4 out of the image). Dropping that
    // factor would double every angular impulse.
    // The linear sum ADDS the two inverse masses; the angular sum has no mass term, which is
    // correct for a pure rotational row.
    const V3 lvInvMassL = MakeV3(1.0f / lafMassL[0], 1.0f / lafMassL[1], 1.0f / lafMassL[2]);
    const V3 lvInvMassA = MakeV3(0.5f / lafMassA[0], 0.5f / lafMassA[1], 0.5f / lafMassA[2]);

    // +0x40..+0x90 are the TRANSPOSED, PRE-DIVIDED J^T, linear and angular interleaved:
    //     +0x40 = (L0.x, L1.x, L2.x) * 1/mEffLin      +0x50 = (T.x , S1.x, S2.x) * 0.5/mEffAng
    //     +0x60 = (L0.y, L1.y, L2.y) * 1/mEffLin      +0x70 = (T.y , S1.y, S2.y) * 0.5/mEffAng
    //     +0x80 = (L0.z, L1.z, L2.z) * 1/mEffLin      +0x90 = (T.z , S1.z, S2.z) * 0.5/mEffAng
    // The row index i is the SAME i as the twelve-slot groups: block 2 builds the linear
    // projection as (dot(L0,.), dot(L1,.), dot(L2,.)) and block 5 the angular one as
    // (dot(T,.), dot(S1,.), dot(S2,.)) through the identical `vmsum3fp128` + gather, so lane i
    // is row i in both.
    for (u32 luC = 0u; luC < 3u; ++luC)
    {
        const V3 lvLin = MulLanes(GatherComponent(lM_L.xAxis, lM_L.yAxis, lM_L.zAxis, luC),
                                  lvInvMassL);
        const V3 lvAng = MulLanes(GatherComponent(lvAxisT, lvAxisS1, lvAxisS2, luC), lvInvMassA);
        rw::math::vpu::Vector4& lrLinRow = mRows[4u + luC * 2u];   // +0x40, +0x60, +0x80
        rw::math::vpu::Vector4& lrAngRow = mRows[5u + luC * 2u];   // +0x50, +0x70, +0x90
        lrLinRow.x = lvLin.x; lrLinRow.y = lvLin.y; lrLinRow.z = lvLin.z;
        lrAngRow.x = lvAng.x; lrAngRow.y = lvAng.y; lrAngRow.z = lvAng.z;
    }

    // ⭐ THE ANGULAR lo/hi PAIR IS NOT STORED WHOLE. Only its .z reaches +0xA0/+0xB0; its .x
    // and .y ride in the w lanes of +0x60/+0x70 (lo) and +0x80/+0x90 (hi). Missing that would
    // leave two thirds of the angular limits at whatever the `dcbz128` left behind.
    const V3 lvAngLoOut = MulLanes(lvAngLo, lvInvMassA);
    const V3 lvAngHiOut = MulLanes(lvAngHi, lvInvMassA);
    mRows[6].w = lvAngLoOut.x;   // +0x6C
    mRows[7].w = lvAngLoOut.y;   // +0x7C
    mRows[8].w = lvAngHiOut.x;   // +0x8C
    mRows[9].w = lvAngHiOut.y;   // +0x9C

    // +0xA0 / +0xB0: the PRE-DIVIDED linear lo/hi pair, with the angular pair's .z in w.
    const V3 lvLinLoOut = MulLanes(lvLinLo, lvInvMassL);
    const V3 lvLinHiOut = MulLanes(lvLinHi, lvInvMassL);
    mRows[10].x = lvLinLoOut.x; mRows[10].y = lvLinLoOut.y; mRows[10].z = lvLinLoOut.z;
    mRows[10].w = lvAngLoOut.z;   // +0xAC
    mRows[11].x = lvLinHiOut.x; mRows[11].y = lvLinHiOut.y; mRows[11].z = lvLinHiOut.z;
    mRows[11].w = lvAngHiOut.z;   // +0xBC

    mSpy = lrJoint.m_spy;   // +0x4C  `lwz r9,0x1C(r27)` / `stw r9,0x4C(r31)`
    // ⚠️ +0x5C: the console's whole-vector store at +0x50 leaves a FOURTH gathered lane there
    // (S2.y scaled by the w lane of the reciprocal vector) and nothing ever reads it. It is
    // left at the `dcbz128` zero rather than reproducing an undefined lane.
}

} // namespace physics
} // namespace rw

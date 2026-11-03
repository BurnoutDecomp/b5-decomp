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
    Quat lqRel;
    __vector4 lRelative;
    lRelative.v[0] = lRQD.RowW(0u); lRelative.v[1] = lRQD.RowW(1u);
    lRelative.v[2] = lRQD.RowW(2u); lRelative.v[3] = lRQD.RowW(3u);
    __stvx(lRelative, &lqRel, 0);

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
    // ARTIST uses one rounded multiply followed by two fused accumulations.
    __vector4 lRA = __vmulfp(__lvx(&lrA.mRi, 0), __vspltw(lvPosA.mV, 0));
    lRA = __vmaddfp(__lvx(&lrA.mUp, 0), __vspltw(lvPosA.mV, 1), lRA);
    lRA = __vmaddfp(__lvx(&lrA.mAt, 0), __vspltw(lvPosA.mV, 2), lRA);
    __vector4 lRB = __vmulfp(__lvx(&lrB.mRi, 0), __vspltw(lvPosB.mV, 0));
    lRB = __vmaddfp(__lvx(&lrB.mUp, 0), __vspltw(lvPosB.mV, 1), lRB);
    lRB = __vmaddfp(__lvx(&lrB.mAt, 0), __vspltw(lvPosB.mV, 2), lRB);
    const V3 lvRA(lRA), lvRB(lRB);

    // Unlike the drive builder, the joint folds velocity and acceleration into ONE predicted
    // velocity -- it has no separate gains to apply to them.
    // (`cross(mOmega, r)`, not the transpose -- the sign is read off tables 0x82181680 = YZX
    // and 0x82181670 = ZXY, and BurnoutPR's `pshufd 9`/`12h` pair agrees.)
    // ARTIST 0x82BC458C..4664: the point velocity and acceleration
    // each carry the body's vector as the addend of a fused cross product.
    const __vector4 rAYZ = __vpermwi(lvRA.mV, 0x63), rAZX = __vpermwi(lvRA.mV, 0x87);
    const __vector4 rBYZ = __vpermwi(lvRB.mV, 0x63), rBZX = __vpermwi(lvRB.mV, 0x87);
    const __vector4 omegaA = __lvx(&lrA.mOmega, 0), torqueA = __lvx(&lrA.mTorque, 0);
    const __vector4 omegaB = __lvx(&lrB.mOmega, 0), torqueB = __lvx(&lrB.mTorque, 0);
    const __vector4 forceA = __lvx(&lrA.mForce, 0), forceB = __lvx(&lrB.mForce, 0);
    const __vector4 motionA = __vmaddfp(rAZX, __vpermwi(omegaA, 0x63),
                              __vnmsubfp(rAYZ, __vpermwi(omegaA, 0x87), __lvx(&lrA.mVel, 0)));
    const __vector4 motionB = __vmaddfp(rBZX, __vpermwi(omegaB, 0x63),
                              __vnmsubfp(rBYZ, __vpermwi(omegaB, 0x87), __lvx(&lrB.mVel, 0)));
    const __vector4 accelA = __vmaddfp(rAZX, __vpermwi(torqueA, 0x63),
                             __vnmsubfp(rAYZ, __vpermwi(torqueA, 0x87), forceA));
    const __vector4 accelB = __vmaddfp(rBZX, __vpermwi(torqueB, 0x63),
                             __vnmsubfp(rBYZ, __vpermwi(torqueB, 0x87), forceB));
    const __vector4 timestep = __vspltw(__lvlx(&lfH, 0), 0);
    const V3 lvVelA(__vmaddfp(accelA, timestep, motionA));
    const V3 lvVelB(__vmaddfp(accelB, timestep, motionB));

    const V3 lvRelVel = Sub(lvVelB, lvVelA);
    const V3 lvErr    = Sub(Add(V3(__lvx(&lrB.mCom, 0)), lvRB), Add(V3(__lvx(&lrA.mCom, 0)), lvRA));
    const V3 lvDisp(__vmaddfp(lvRelVel.mV, timestep, lvErr.mV)); // 0x82BC467C

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
    V3 lvAxisT, lvAxisS1, lvAxisS2;
    // 0x82BC460C..4614 replaces XYZ of the relative quaternion local only.
    V3 lvAngLoRaw(lRelative);
    lvAngLoRaw.mV.v[0] = -FLT_MAX;
    lvAngLoRaw.mV.v[1] = -FLT_MAX;
    lvAngLoRaw.mV.v[2] = -FLT_MAX;
    // 0x82BC46DC..46EC replaces XYZ of the qB local, preserving its W.
    V3 lvAngHiRaw(__lvx(&lqB, 0));
    lvAngHiRaw.mV.v[0] = FLT_MAX;
    lvAngHiRaw.mV.v[1] = FLT_MAX;
    lvAngHiRaw.mV.v[2] = FLT_MAX;

    SwingType leSwing = lrLim.GetSwingType();

    // SWING_CONE degenerates to SWING_FREE when the two reference axes are (near) parallel:
    // X360 compares M_Rel(0,0) against flt_821817E0 and branches straight into the FREE arm.
    if (leSwing == SWING_CONE && lM_Rel.xAxis.mV.v[0] >= KF_CONE_DEGENERATE)
        leSwing = SWING_FREE;

    switch (leSwing)
    {
    case SWING_LOCKED:                                   // loc_82BC4978
        lvAngLoRaw.mV.v[1] = lvAngHiRaw.mV.v[1] = KF_ANGLE * lRQD.RowW(1u);
        lvAngLoRaw.mV.v[2] = lvAngHiRaw.mV.v[2] = KF_ANGLE * lRQD.RowW(2u);
        lvAxisS1 = V3(__lvx(&lRQD.mData[4], 0));
        lvAxisS2 = V3(__lvx(&lRQD.mData[8], 0));
        break;

    case SWING_CONE:                                     // loc_82BC4884
        {
            const f32 lfS2 = lM_Rel.yAxis.mV.v[0] * lM_Rel.yAxis.mV.v[0] + lM_Rel.zAxis.mV.v[0] * lM_Rel.zAxis.mV.v[0];
            const f32 lfR  = 1.0f / Sqrt(lfS2);
            lvAxisS1 = Scale(Cross(lM_A.xAxis, lM_B.xAxis), lfR);
            lvAxisS2 = Cross(lvAxisS1, lM_A.xAxis);
            // A ONE-SIDED lower bound: the cone only limits how far apart the axes may open.
            lvAngLoRaw.mV.v[1] = (lrLim.GetSwingLimit() - lM_Rel.xAxis.mV.v[0]) * lfR;
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
            lvAngLoRaw.mV.v[2] = lvAngHiRaw.mV.v[2] = KF_ANGLE * lRQD.RowW(2u);
            lvAxisS2 = V3(__lvx(&lRQD.mData[8], 0));
        }
        else
        {
            // X360 negates with `vspltisw -1` + `vslw` (a 0x80000000 sign mask) + `vxor`;
            // BurnoutPR with `xorps xmm3,xmm3` + `subps`. Both are a unary minus.
            lvAngLoRaw.mV.v[2] = lvAngHiRaw.mV.v[2] = -lM_Rel.yAxis.mV.v[0];
            lvAxisS2 = Cross(lM_A.xAxis, lM_B.yAxis);
        }
        if (lrLim.GetSwingType() == SWING_HINGE)         // loc_82BC483C
        {
            const f32 lfD = lM_Rel.zAxis.mV.v[0];
            if (lfD != 0.0f)                             // the `fcmpu` guard vs 0.0f
            {
                const f32 lfV = (lrLim.GetSwingLimit() - lM_Rel.xAxis.mV.v[0]) / lfD;
                // The SIGN OF THE DENOMINATOR flips the inequality direction.
                if (lfD >= 0.0f) lvAngLoRaw.mV.v[1] = lfV;
                else             lvAngHiRaw.mV.v[1] = lfV;
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
        lvAngLoRaw.mV.v[0] = lvAngHiRaw.mV.v[0] = KF_ANGLE * lRQD.RowW(0u);
        lvAxisT = V3(__lvx(&lRQD.mData[0], 0));
        break;

    case TWIST_ARC:                                      // loc_82BC49F8
        lvAxisT = lM_A.xAxis;
        {
            const f32 lfD = lM_Rel.yAxis.mV.v[2] - lM_Rel.zAxis.mV.v[1];
            if (lfD != 0.0f)
            {
                const f32 lfV = ((lM_Rel.xAxis.mV.v[0] + 1.0f) * lrLim.GetTwistLimit()
                                 - (lM_Rel.yAxis.mV.v[1] + lM_Rel.zAxis.mV.v[2])) / lfD;
                if (lfD >= 0.0f) lvAngLoRaw.mV.v[0] = lfV;
                else             lvAngHiRaw.mV.v[0] = lfV;
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
    const M33 lAngFrame(lvAxisT, lvAxisS1, lvAxisS2);
    const __vector4 angularVelocity = __vsubfp(__lvx(&lrB.mOmega, 0), __lvx(&lrA.mOmega, 0));
    const __vector4 angularAccel = __vsubfp(__lvx(&lrB.mTorque, 0), __lvx(&lrA.mTorque, 0));
    const V3  lvRelOmega(__vmaddfp(angularAccel, timestep, angularVelocity));
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
    // builder writes before the subsequent packed stores.
    for (u32 luRow = 0u; luRow < 24u; ++luRow)
    {
        mRows[luRow].mV.v[0] = 0.0f; mRows[luRow].mV.v[1] = 0.0f;
        mRows[luRow].mV.v[2] = 0.0f; mRows[luRow].mV.v[3] = 0.0f;
    }
    mRows[0].mV.u[3] = 0u; mRows[1].mV.u[3] = 0u; mRows[4].mV.u[3] = 0u; mRows[12].mV.u[3] = 0;

    // Branchless ACTIVE test on the console: `vspltw(mIsplt,3) & 4 == 4` -> a `vcmpequw` lane
    // mask, then `vsel(0, mIfull, mask)` / `vsel(0, mIsplt, mask)`. Zeroing mIfull also zeroes
    // its w lane (the inverse mass), so one operation gives both "infinite mass" and "zero
    // inverse inertia".
    const bool lbActiveA = (lrA.mState & ACTIVE_BODY) != 0;
    const bool lbActiveB = (lrB.mState & ACTIVE_BODY) != 0;
    const f32  lfInvMassA = lbActiveA ? lrA.mInvm : 0.0f;
    const f32  lfInvMassB = lbActiveB ? lrB.mInvm : 0.0f;
    const M33  lInvIA = lbActiveA ? UnpackInverseInertia(V4(__lvx(&lrA.mIfull, 0)), V4(__lvx(&lrA.mIsplt, 0))) : ZeroMatrix33();
    const M33  lInvIB = lbActiveB ? UnpackInverseInertia(V4(__lvx(&lrB.mIfull, 0)), V4(__lvx(&lrB.mIsplt, 0))) : ZeroMatrix33();

    // Slots 0 and 1: the anchor arms with the reaction-force ids in their w lanes. The console
    // builds them with `vsel(mCom, r, tbl@0x82181660)` = (r.xyz, mCom.w) and RigidBody+0x1C
    // is mId -- so rA pairs with A's id, which is an independent corroboration of rA/rB.
    mRows[0].mV.v[0] = lvRA.mV.v[0]; mRows[0].mV.v[1] = lvRA.mV.v[1]; mRows[0].mV.v[2] = lvRA.mV.v[2];   // +0x00
    mRows[0].mV.u[3] = lrA.mId;                                                  // +0x0C
    mRows[1].mV.v[0] = lvRB.mV.v[0]; mRows[1].mV.v[1] = lvRB.mV.v[1]; mRows[1].mV.v[2] = lvRB.mV.v[2];   // +0x10
    mRows[1].mV.u[3] = lrB.mId;                                                  // +0x1C
    // +0x20 and +0x30 are stored as an explicit zero vector (already zeroed above).

    // The three linear constraint axes are the rows of M_L; the angular ones are the triple
    // the two switches selected.
    V3  lavCrossA[3], lavCrossB[3], lavAngA[3], lavAngB[3];
    __vector4 lMassTermsL[3], lMassTermsA[3];
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

        // 0x82BC4E48..4EC0 keeps each component until the packed transpose.
        const __vector4 aLinear = __vmaddfp(lvCA.mV, lavCrossA[luI].mV, __vspltisw(0));
        lMassTermsL[luI] = __vmaddfp(lvCB.mV, lavCrossB[luI].mV, aLinear);
        const __vector4 angInertia = __vaddfp(lavAngA[luI].mV, lavAngB[luI].mV);
        lMassTermsA[luI] = __vmaddfp(lvAngAxis.mV, angInertia, __vspltisw(0));
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

        mRows[luBase + 0u].mV.v[0] = lvLinAxis.mV.v[0];
        mRows[luBase + 0u].mV.v[1] = lvLinAxis.mV.v[1];
        mRows[luBase + 0u].mV.v[2] = lvLinAxis.mV.v[2];

        mRows[luBase + 1u].mV.v[0] = lavCrossA[luI].mV.v[0];
        mRows[luBase + 1u].mV.v[1] = lavCrossA[luI].mV.v[1];
        mRows[luBase + 1u].mV.v[2] = lavCrossA[luI].mV.v[2];
        mRows[luBase + 1u].mV.v[3] = lavAngB[luI].mV.v[0];

        mRows[luBase + 2u].mV.v[0] = lavAngA[luI].mV.v[0];
        mRows[luBase + 2u].mV.v[1] = lavAngA[luI].mV.v[1];
        mRows[luBase + 2u].mV.v[2] = lavAngA[luI].mV.v[2];
        mRows[luBase + 2u].mV.v[3] = lavAngB[luI].mV.v[1];

        mRows[luBase + 3u].mV.v[0] = lavCrossB[luI].mV.v[0];
        mRows[luBase + 3u].mV.v[1] = lavCrossB[luI].mV.v[1];
        mRows[luBase + 3u].mV.v[2] = lavCrossB[luI].mV.v[2];
        mRows[luBase + 3u].mV.v[3] = lavAngB[luI].mV.v[2];
    }
    mRows[12].mV.u[3] = reinterpret_cast<u32>(const_cast<Joint*>(&lrJoint));   // +0xCC  `stw r27,0xCC(r31)`
    mRows[16].mV.v[3] = lfInvMassA;                     // +0x10C
    mRows[20].mV.v[3] = lfInvMassB;                     // +0x14C

    // =================================================================================
    // BLOCK 7 -- 0x82BC4EC0..0x82BC5088   TAIL.
    // =================================================================================
    // A `vmrghw`/`vmrglw` 4x4 transpose feeds the two accumulated effective masses, each
    // inverted once with `vrefp`. ⚠️ THE ANGULAR RECIPROCAL IS THEN HALVED (X360 line 834
    // multiplies by [flt_821815B0+0x210], which reads 0.5f x4 out of the image). Dropping that
    // factor would double every angular impulse.
    // The linear sum ADDS the two inverse masses; the angular sum has no mass term, which is
    // correct for a pure rotational row.
    // The controls below are the image bytes at 0x821816C0..0x82181750.
    // The fourth lane is deliberately carried through the same merges and
    // permutations: +0x5C is a full-vector store on ARTIST.
    static const CGS_ALIGN(16) u32 kYZ[4] = {0x08090A0B, 0x0C0D0E0F, 0x14151617, 0x10111213};
    static const CGS_ALIGN(16) u32 kZX[4] = {0x00010203, 0x04050607, 0x18191A1B, 0x10111213};
    static const CGS_ALIGN(16) u32 kXY[4] = {0x00010203, 0x04050607, 0x10111213, 0x14151617};
    static const CGS_ALIGN(16) u32 kInsertX[4] = {0x00010203, 0x04050607, 0x08090A0B, 0x10111213};
    static const CGS_ALIGN(16) u32 kInsertY[4] = {0x00010203, 0x04050607, 0x08090A0B, 0x14151617};
    static const CGS_ALIGN(16) u32 kInsertZ[4] = {0x00010203, 0x04050607, 0x08090A0B, 0x18191A1B};
    const __vector4 yz = __lvx(kYZ, 0), zx = __lvx(kZX, 0), xy = __lvx(kXY, 0);
    const __vector4 zero = __vspltisw(0);
    const __vector4 lin01h = __vmrghw(lMassTermsL[0], lMassTermsL[1]);
    const __vector4 lin01l = __vmrglw(lMassTermsL[0], lMassTermsL[1]);
    const __vector4 ang01h = __vmrghw(lMassTermsA[0], lMassTermsA[1]);
    const __vector4 ang01l = __vmrglw(lMassTermsA[0], lMassTermsA[1]);
    const __vector4 linX = __vperm(lin01h, lMassTermsL[2], xy);
    const __vector4 linY = __vperm(lin01h, lMassTermsL[2], yz);
    const __vector4 linZ = __vperm(lin01l, lMassTermsL[2], zx);
    const __vector4 angX = __vperm(ang01h, lMassTermsA[2], xy);
    const __vector4 angY = __vperm(ang01h, lMassTermsA[2], yz);
    const __vector4 angZ = __vperm(ang01l, lMassTermsA[2], zx);
    const __vector4 massA = __vspltw(__lvx(&lrA.mIfull, 0), 3);
    const __vector4 massB = __vspltw(__lvx(&lrB.mIfull, 0), 3);
    const __vector4 activeA = __vcmpequw(__vand(__vspltw(__lvx(&lrA.mIsplt, 0), 3), __vspltisw(4)), __vspltisw(4));
    const __vector4 activeB = __vcmpequw(__vand(__vspltw(__lvx(&lrB.mIsplt, 0), 3), __vspltisw(4)), __vspltisw(4));
    const __vector4 invMassSum = __vaddfp(__vsel(zero, massA, activeA), __vsel(zero, massB, activeB));
    const __vector4 massLin = __vaddfp(__vaddfp(linZ, linY), __vaddfp(linX, invMassSum));
    const __vector4 massAng = __vaddfp(angZ, __vaddfp(angX, angY));
    const __vector4 invLin = __vrefp(massLin); // 0x82BC4FA4: no Newton refinement
    const __vector4 half = __vcfsx(__vspltisw(1), 1);
    const __vector4 invAng = __vmaddfp(__vrefp(massAng), half, zero); // 4FC8, 4FEC

    const __vector4 l01h = __vmrghw(lM_L.xAxis.mV, lM_L.yAxis.mV);
    const __vector4 l01l = __vmrglw(lM_L.xAxis.mV, lM_L.yAxis.mV);
    const __vector4 a01h = __vmrghw(lvAxisT.mV, lvAxisS1.mV);
    const __vector4 a01l = __vmrglw(lvAxisT.mV, lvAxisS1.mV);
    const __vector4 linearX = __vmaddfp(__vperm(l01h, lM_L.zAxis.mV, xy), invLin, zero);
    const __vector4 linearY = __vmaddfp(__vperm(l01h, lM_L.zAxis.mV, yz), invLin, zero);
    const __vector4 linearZ = __vmaddfp(__vperm(l01l, lM_L.zAxis.mV, zx), invLin, zero);
    const __vector4 angularX = __vmaddfp(__vperm(a01h, lvAxisS2.mV, xy), invAng, zero);
    const __vector4 angularY = __vmaddfp(__vperm(a01h, lvAxisS2.mV, yz), invAng, zero);
    const __vector4 angularZ = __vmaddfp(__vperm(a01l, lvAxisS2.mV, zx), invAng, zero);
    const __vector4 angularLo = __vmaddfp(lvAngLo.mV, invAng, zero);
    const __vector4 angularHi = __vmaddfp(lvAngHi.mV, invAng, zero);
    const __vector4 linearLo = __vmaddfp(lvLinLo.mV, invLin, zero);
    const __vector4 linearHi = __vmaddfp(lvLinHi.mV, invLin, zero);
    mRows[4].mV = linearX;
    mRows[5].mV = angularX;
    mRows[6].mV = __vperm(linearY, angularLo, __lvx(kInsertX, 0));
    mRows[7].mV = __vperm(angularY, angularLo, __lvx(kInsertY, 0));
    mRows[8].mV = __vperm(linearZ, angularHi, __lvx(kInsertX, 0));
    mRows[9].mV = __vperm(angularZ, angularHi, __lvx(kInsertY, 0));
    mRows[10].mV = __vperm(linearLo, angularLo, __lvx(kInsertZ, 0));
    mRows[11].mV = __vperm(linearHi, angularHi, __lvx(kInsertZ, 0));

    mRows[4].mV.u[3] = lrJoint.m_spy;   // +0x4C  `lwz r9,0x1C(r27)` / `stw r9,0x4C(r31)`
    // +0x5C is the fourth lane of angularX from the packed store above.
}

} // namespace physics
} // namespace rw

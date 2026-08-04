#include "vendor/renderware/physics/Jacobian.hpp"
#include "vendor/renderware/physics/JacobianMath.hpp"
#include "vendor/renderware/physics/Drive.hpp"
#include "rw/physics/quaternion.h"
#include "rw/physics/simulation.h"

// ⚠️ AFTER every include, not before: <windows.h> arrives through the platform headers this
// TU pulls in and defines `GetDriveType` as `GetDriveTypeA`, which silently renames the SDK
// accessor at every call site below (C2039). DriveDynamics.hpp drops the macro too, but a
// later include re-defines it, so the last word has to be here. Same class of collision as
// GetObject / GetMessage. The DWARF name (drivedynamics.h:158) is what is kept.
#ifdef GetDriveType
#undef GetDriveType
#endif

// =====================================================================================
// rw::physics::DriveJacobian::Build @0x82BC5590 -- 1320 X360 instructions, the largest
// function in the rw::physics closure.
//
// A DRIVE is a soft or hard servo that pulls two rigid bodies towards a target relative
// pose. Build turns one drive into the 384-byte jacobian record the solver pipelines
// consume: three linear constraint rows along the drive frame's axes, three angular rows
// about the relative-rotation axes, each with its pre-divided impulse and an impulse clamp.
//
// TWO INDEPENDENT WITNESSES were read instruction for instruction and agree throughout:
//   [X360] BURNOUT_X360_ARTIST.XEX @0x82BC5590 -- AUTHORITY for every offset and for what
//          exists. Every offset in this file is X360's.
//   [BPR]  BurnoutPR.exe sub_7E3C50, 1653 insn, x86/SSE -- ALGORITHM ORACLE ONLY. Used
//          because every SSE swizzle is an IMMEDIATE, which settles the swizzle identities
//          and the cross-product SIGN that the X360 `vperm` tables hide.
//          ⛔ ITS RECORD OFFSETS DIFFER (node pointer at +0x12C, inverse masses at
//          +0xEC/+0x13C, m_spy stored SHIFTED LEFT BY ONE at +0xBC). Import none of them.
//   [DWARF] references/DecFIGS/.../rw/physics/{drive,driveframes,drivedynamics,rigidbody,
//          inertia,simulation}.h -- names, types, declaration order.
//
// ⭐ NOTHING BELOW IS A PLACEHOLDER. The two items the previous wave left open were closed
// by READING, in this wave, and both are recorded at their site:
//   (a) K_ANGLE: `flt_82001D9C` = 0x40000000 = 2.0f, read from the X360 image and confirmed
//       at its use site (`lis r20,flt_82001D9C@ha` / `lfs f12,...`, and f12 is the multiplier
//       in exactly the LOCKED-arm `fmuls`).
//   (b) block 7's transpose lane order and the two gains at +0x2C/+0x3C -- see the notes at
//       those two sites.
// =====================================================================================

namespace rw
{
namespace physics
{

using namespace rw::physics::jacobian_detail;

namespace
{
    // The three linear/angular "softening" gains a Params block produces, and the switch that
    // picks them. VERIFIED on X360 (0x82BC5860 linear, 0x82BC5D28 angular) and on BurnoutPR.
    //   SOFT_DRIVE : D = 1 + k*h^2 + c*h ; gains = (k*h^2/D, c*h/D, (c*h + k*h^2)/D)
    //   NO_DRIVE and HARD_DRIVE : gains = (1/(1 + c*h), 1, 1)
    // `flt_82001C98` (the 1/x numerator) reads 0x3F800000 = 1.0f out of the X360 image.
    struct DriveGains
    {
        f32 mfPos;
        f32 mfVel;
        f32 mfAcc;
    };

    DriveGains ComputeGains(const DriveDynamics::Params& lrParams, f32 lfH)
    {
        DriveGains lResult;
        if (lrParams.GetDriveType() == SOFT_DRIVE)
        {
            const f32 lfKh2 = lrParams.GetSpring()  * lfH * lfH;
            const f32 lfCh  = lrParams.GetDamping() * lfH;
            const f32 lfDen = 1.0f + lfKh2 + lfCh;
            lResult.mfPos = lfKh2 / lfDen;
            lResult.mfVel = lfCh  / lfDen;
            lResult.mfAcc = (lfCh + lfKh2) / lfDen;
        }
        else
        {
            lResult.mfPos = 1.0f / (1.0f + lrParams.GetDamping() * lfH);
            lResult.mfVel = 1.0f;
            lResult.mfAcc = 1.0f;
        }
        return lResult;
    }

    // HARD_DRIVE clamps the positional error to a per-step travel limit.
    // ⭐ THIS IS WHY THE DWARF EXPOSES BOTH GetSpring() AND GetMaxVelocity() OVER +0x00: in
    // SOFT the slot is a spring constant, in HARD the same slot is a maximum velocity and the
    // limit is `GetMaxVelocity() * h`. The code proves the reading.
    V3 ClampHardError(const V3& lrErr, const DriveDynamics::Params& lrParams, f32 lfH)
    {
        if (lrParams.GetDriveType() != HARD_DRIVE)
            return lrErr;

        const f32 lfMaxStep = lrParams.GetMaxVelocity() * lfH;
        const f32 lfLen2    = Dot3(lrErr, lrErr);
        if (lfLen2 > lfMaxStep * lfMaxStep)
            return Scale(lrErr, lfMaxStep / Sqrt(lfLen2));
        return lrErr;
    }

    // `flt_82001D9C` = 0x40000000 = 2.0f, READ from the X360 image (task #130 §4) and
    // independently forced by the constraint identity (J, b) == (sJ, sb): X360 scales the
    // RQD-derived ERROR by this constant while BurnoutPR instead scales the RQD-derived AXIS
    // by 0.5f, and the two agree iff the constant is exactly 2. It is also the natural
    // half-angle -> angle factor, since RQD[i].w ~ sin(theta_i / 2).
    const f32 KF_ANGLE = 2.0f;
}

void DriveJacobian::Build(const Drive& lrDrive, Simulation* lpSim)
{
    const DriveFrames&   lrF = *lrDrive.m_skel;
    const DriveDynamics& lrD = *lrDrive.m_crtl;
    RigidBody&           lrA = *lrDrive.m_bodyA;
    RigidBody&           lrB = *lrDrive.m_bodyB;
    const f32            lfH = lpSim->GetTimeStep();   // the ONLY field read from Simulation

    // =================================================================================
    // BLOCK 1 -- 0x82BC5590..0x82BC56DC   PROLOGUE.   [X360 8/8 + BPR]  VERIFIED
    // =================================================================================
    const Quat lqA = QuatMul(lrA.mQuat, lrF.GetChildOrientation());    // qA'
    const Quat lqB = QuatMul(lrB.mQuat, lrF.GetParentOrientation());   // qB'

    // The 64-byte RQD destination is pre-filled with `flt_82001CC0` (= 0x00000000 = 0.0f,
    // read from the image) by 16 `stfs` before the call. Dead -- Create writes all 16 slots.
    Jacobian_RQD lRQD;
    lRQD.Create(&lqA, &lqB);   // = L(conj qA') . R(qB')

    // ** THE BASIS COMES FROM qB', NOT qA'. ** X360 passes r4 = var_200 = qB'; BurnoutPR's
    // inlined expansion runs on [ebp-2E0h] = qB'. VERIFIED on both.
    // (X360 then copies the three rows to a second stack local that is never written again
    // and reads the COPY in block 7. Semantically one value; kept as one variable.)
    Quat lqBasis = lqB;
    M33  lBasis;
    Quaternion::UnitQuaternionToMatrix(&lBasis, &lqBasis);

    // =================================================================================
    // BLOCK 2 -- 0x82BC56E0..0x82BC587C   ANCHOR POINTS + PREDICTED MOTION.
    //            [X360 + BPR 0x7E3E24..0x7E4037, instruction for instruction]  VERIFIED
    // =================================================================================
    // r = R_body . framePos, built as axis*component -- which is what makes RigidBody's
    // mRi/mUp/mAt the COLUMNS of the body->world rotation.
    const V3 lvPosA = Xyz(lrF.GetChildPosition());
    const V3 lvPosB = Xyz(lrF.GetParentPosition());
    const V3 lvRA = Add(Add(Scale(Xyz(lrA.mRi), lvPosA.x), Scale(Xyz(lrA.mUp), lvPosA.y)),
                        Scale(Xyz(lrA.mAt), lvPosA.z));
    const V3 lvRB = Add(Add(Scale(Xyz(lrB.mRi), lvPosB.x), Scale(Xyz(lrB.mUp), lvPosB.y)),
                        Scale(Xyz(lrB.mAt), lvPosB.z));

    const V3 lvAnchorA = Add(Xyz(lrA.mCom), lvRA);
    const V3 lvAnchorB = Add(Xyz(lrB.mCom), lvRB);

    // ** cross(omega, r), NOT cross(r, omega). ** The sign is READ: table 0x82181680 = YZX and
    // 0x82181670 = ZXY, so the fused pair is omega.yzx*r.zxy - omega.zxy*r.yzx = cross(omega, r).
    // BurnoutPR's `pshufd 9` / `pshufd 12h` pair says the same thing.
    // Unlike the joint builder, the drive keeps velocity and acceleration APART -- it has
    // three separate gains to apply to them.
    const V3 lvVelA = Scale(Add(Xyz(lrA.mVel), Cross(Xyz(lrA.mOmega), lvRA)), lfH);
    const V3 lvVelB = Scale(Add(Xyz(lrB.mVel), Cross(Xyz(lrB.mOmega), lvRB)), lfH);
    const V3 lvAccA = Scale(Add(Xyz(lrA.mForce), Cross(Xyz(lrA.mTorque), lvRA)), lfH * lfH);
    const V3 lvAccB = Scale(Add(Xyz(lrB.mForce), Cross(Xyz(lrB.mTorque), lvRB)), lfH * lfH);

    // =================================================================================
    // BLOCK 3 -- 0x82BC5860..0x82BC5D24   LINEAR HALF.
    //            [X360 + BPR 0x7E3FE5..0x7E4206]  VERIFIED
    // =================================================================================
    const DriveGains lLinGains = ComputeGains(lrD.LinearParams(), lfH);

    const V3 lvErrL  = ClampHardError(Sub(lvAnchorB, lvAnchorA), lrD.LinearParams(), lfH);
    const V3 lvBiasL = Add(Add(Scale(lvErrL, lLinGains.mfPos),
                               Scale(Sub(lvVelB, lvVelA), lLinGains.mfVel)),
                           Scale(Sub(lvAccB, lvAccA), lLinGains.mfAcc));
    const V3 lvLocalBiasL = Project(lBasis, lvBiasL);

    // =================================================================================
    // BLOCK 4 -- 0x82BC5D28..0x82BC5F90   ANGULAR HALF.
    //            [X360 + BPR 0x7E41E4..0x7E461A]  VERIFIED
    // =================================================================================
    // The three angular constraint axes come out of the RQD matrix. RQD = L(conj qA').R(qB'),
    // so RQD[i].w is component i of conj(qA') (x) qB' -- the relative rotation. For each row:
    //     s_i    = 1 / sqrt(1 - RQD[i].w^2)
    //     axis_i = vec3(RQD[i]) * s_i     <- the row of d(angle_i)/d(omega)
    //     err_i  = RQD[i].w    * s_i      <- the tangent of the half-angle about that axis
    // ⇒ RQD IS the rotation-error jacobian, which is what the name decodes to.
    V3 lavAxis[3];
    f32 lafErr[3];
    for (u32 luI = 0u; luI < 3u; ++luI)
    {
        const f32 lfW = lRQD.RowW(luI);
        const f32 lfS = 1.0f / Sqrt(1.0f - lfW * lfW);
        lavAxis[luI] = Scale(MakeV3(lRQD.RowX(luI), lRQD.RowY(luI), lRQD.RowZ(luI)), lfS);
        lafErr[luI]  = lfW * lfS;
    }

    const M33 lAngFrame = { lavAxis[0], lavAxis[1], lavAxis[2] };

    V3 lvErrA = Scale(MakeV3(lafErr[0], lafErr[1], lafErr[2]), KF_ANGLE);
    lvErrA = ClampHardError(lvErrA, lrD.AngularParams(), lfH);

    const V3 lvDOmega  = Scale(Sub(Xyz(lrB.mOmega),  Xyz(lrA.mOmega)),  lfH);
    const V3 lvDTorque = Scale(Sub(Xyz(lrB.mTorque), Xyz(lrA.mTorque)), lfH * lfH);

    const DriveGains lAngGains = ComputeGains(lrD.AngularParams(), lfH);
    const V3 lvLocalBiasA = Add(Add(Scale(lvErrA, lAngGains.mfPos),
                                    Scale(Project(lAngFrame, lvDOmega), lAngGains.mfVel)),
                                Scale(Project(lAngFrame, lvDTorque), lAngGains.mfAcc));

    // =================================================================================
    // BLOCKS 5 & 6 -- 0x82BC5F9C..0x82BC657C   PER-BODY M^-1 J^T TERMS.
    //   The ACTIVE_BODY gate and every scalar-lane write: [X360] VERIFIED by write-cover.
    //   The inertia maths: [BPR 0x7E46E8..0x7E48FF] VERIFIED, and the mIfull/mIsplt unpack
    //   is now READ from the X360 tables 0x821816A0/0x821816B0 (see JacobianMath.hpp).
    //
    // ⭐ THE RECORD IS THREE IDENTICAL 0x40-BYTE GROUPS, base 0xC0 + 0x40*i (row index i):
    //     +0x00 : ( L_i                    , [i=0: &node | i=1: A.mInvm | i=2: B.mInvm] )
    //     +0x10 : ( I_A^-1 . (rA x L_i)    , (I_B^-1 . angAxis_i).x )
    //     +0x20 : ( I_A^-1 . angAxis_i     , (I_B^-1 . angAxis_i).y )
    //     +0x30 : ( I_B^-1 . (rB x L_i)    , (I_B^-1 . angAxis_i).z )
    // i.e. constraint row i is the PAIR (linear along L_i, angular about angAxis_i) and the
    // four vectors of the group are that row-pair's four M^-1 J^T pieces. 6 rows x 2 bodies
    // = 12 products: nine in xyz, three spread across the `w` lanes.
    // ⚠️ The `w` spread carries I_**B**^-1 . angAxis_i in group i -- NOT I_A^-1. Body
    // attribution was resolved by following each accumulator chain to its last term: the
    // chains ending `+ v3*..` carry I_B^-1 row2, the ones ending `+ v5*..` carry I_A^-1 row2.
    // Self-check that would have caught a swap: every cross-product chain uses the inertia
    // rows of the SAME body whose anchor r it was built from. All six match.
    // =================================================================================
    const bool lbActiveA = (lrA.mState & ACTIVE_BODY) != 0;   // `rlwinm. r10,r10,0,29,29`
    const bool lbActiveB = (lrB.mState & ACTIVE_BODY) != 0;

    // A non-ACTIVE body contributes infinite mass and zero inverse inertia -- on the console
    // that falls out of one `vsel`, because zeroing mIfull also zeroes its w lane (mInvm).
    const f32 lfInvMassA = lbActiveA ? lrA.mInvm : 0.0f;
    const f32 lfInvMassB = lbActiveB ? lrB.mInvm : 0.0f;
    const M33 lInvIA = lbActiveA ? UnpackInverseInertia(lrA.mIfull, lrA.mIsplt) : ZeroMatrix33();
    const M33 lInvIB = lbActiveB ? UnpackInverseInertia(lrB.mIfull, lrB.mIsplt) : ZeroMatrix33();

    V3  lavCrossA[3], lavCrossB[3];      // I_A^-1 . (rA x L_i) and I_B^-1 . (rB x L_i)
    V3  lavAngA[3],   lavAngB[3];        // I_A^-1 . angAxis_i  and I_B^-1 . angAxis_i
    f32 lafMassL[3],  lafMassA[3];

    for (u32 luI = 0u; luI < 3u; ++luI)
    {
        const V3 lvLinAxis = Row(lBasis, luI);

        // The angular jacobian of a point constraint is r x n.
        const V3 lvCA = Cross(lvRA, lvLinAxis);
        const V3 lvCB = Cross(lvRB, lvLinAxis);
        lavCrossA[luI] = Transform(lInvIA, lvCA);
        lavCrossB[luI] = Transform(lInvIB, lvCB);

        lavAngA[luI] = Transform(lInvIA, lavAxis[luI]);
        lavAngB[luI] = Transform(lInvIB, lavAxis[luI]);

        // The textbook effective masses, accumulated over both bodies:
        //     linear  1/mA + 1/mB + (rA x n).I_A^-1(rA x n) + (rB x n).I_B^-1(rB x n)
        //     angular            axis . (I_A^-1 axis + I_B^-1 axis)      -- no mass term
        lafMassL[luI] = lfInvMassA + lfInvMassB
                      + Dot3(lvCA, lavCrossA[luI]) + Dot3(lvCB, lavCrossB[luI]);
        lafMassA[luI] = Dot3(lavAxis[luI], lavAngA[luI]) + Dot3(lavAxis[luI], lavAngB[luI]);
    }

    // =================================================================================
    // BLOCK 7 -- 0x82BC6580..0x82BC6A2C   TAIL: normalise and lay the record out.
    // =================================================================================
    // The six effective masses are inverted once and FOLDED INTO the stored rows -- the
    // record never keeps 1/mEff on its own.
    const V3 lvInvMassL = MakeV3(1.0f / lafMassL[0], 1.0f / lafMassL[1], 1.0f / lafMassL[2]);
    const V3 lvInvMassA = MakeV3(1.0f / lafMassA[0], 1.0f / lafMassA[1], 1.0f / lafMassA[2]);

    // ---- slots 0 and 1: the two anchor arms, with the reaction-force ids in their w lanes.
    mRows[0].x = lvRA.x; mRows[0].y = lvRA.y; mRows[0].z = lvRA.z;   // +0x00
    mIdA = lrA.mId;                                                  // +0x0C
    mRows[1].x = lvRB.x; mRows[1].y = lvRB.y; mRows[1].z = lvRB.z;   // +0x10
    mIdB = lrB.mId;                                                  // +0x1C

    // ---- +0x20 and +0x30: explicit zero triples (X360 0x82BC6A08..0x82BC6A1C, `stfs f31`
    //      with f31 = flt_82001CC0 = 0.0f).
    mRows[2].x = mRows[2].y = mRows[2].z = 0.0f;
    mRows[3].x = mRows[3].y = mRows[3].z = 0.0f;

    // ⭐ +0x2C and +0x3C: the LINEAR and ANGULAR acceleration gains. CLOSED BY READING this
    // wave: f6 is last defined at X360 line 240 (`fmr f6,f0`, the NO_DRIVE/HARD_DRIVE arm's
    // 1.0) or line 225 (`fdivs f6,f12,f13`, the SOFT arm) and is NOT redefined anywhere before
    // the store at line 1057 `stfs f6,0x2C(r31)`; f11 likewise between lines 546/557 and the
    // store at line 1058 `stfs f11,0x3C(r31)`. A def-use scan over all 1320 lines confirms it.
    mRows[2].w = lLinGains.mfAcc;   // +0x2C
    mRows[3].w = lAngGains.mfAcc;   // +0x3C

    // ---- the six TRANSPOSED, mass-normalised constraint rows. The solver's inner loop reads
    //      one xyz-component of all three rows at once, so the record stores the transpose:
    //        +0x40/+0x60/+0x80 = (L0, L1, L2) component x/y/z, each scaled by 1/mEffLinear
    //        +0x50/+0x70/+0x90 = (A0, A1, A2) component x/y/z, each scaled by 1/mEffAngular
    //      The lane order is READ, not inferred -- see GatherComponent() in JacobianMath.hpp.
    for (u32 luC = 0u; luC < 3u; ++luC)
    {
        const V3 lvLin = MulLanes(GatherComponent(lBasis.xAxis, lBasis.yAxis, lBasis.zAxis, luC),
                                  lvInvMassL);
        const V3 lvAng = MulLanes(GatherComponent(lavAxis[0], lavAxis[1], lavAxis[2], luC),
                                  lvInvMassA);
        rw::math::vpu::Vector4& lrLinRow = mRows[4u + luC * 2u];   // +0x40, +0x60, +0x80
        rw::math::vpu::Vector4& lrAngRow = mRows[5u + luC * 2u];   // +0x50, +0x70, +0x90
        lrLinRow.x = lvLin.x; lrLinRow.y = lvLin.y; lrLinRow.z = lvLin.z;
        lrAngRow.x = lvAng.x; lrAngRow.y = lvAng.y; lrAngRow.z = lvAng.z;
    }

    mSpy = lrDrive.m_spy;   // +0x4C  (X360 stores it RAW; BurnoutPR shifts it left by one)

    // ---- the six per-row impulse clamps, mStrength * h^2. Three linear lanes and three
    //      angular ones -- NOT "the tail scales the rows by mStrength", which is what an
    //      earlier reading claimed. The joint builder fills these same lanes with a [lo, hi]
    //      limit pair instead, and JointLimits has no mStrength at all: same record, same
    //      solver, two ways of filling the clamp lanes.
    const f32 lfClampL = lrD.LinearParams().GetMaxStrength()  * lfH * lfH;
    const f32 lfClampA = lrD.AngularParams().GetMaxStrength() * lfH * lfH;
    mRows[6].w  = lfClampL;   // +0x6C
    mRows[7].w  = lfClampL;   // +0x7C
    mRows[8].w  = lfClampA;   // +0x8C
    mRows[9].w  = lfClampA;   // +0x9C
    mRows[10].w = lfClampL;   // +0xAC
    mRows[11].w = lfClampA;   // +0xBC

    // ---- +0xA0 / +0xB0: the PRE-DIVIDED impulses (lambda), one per half.
    const V3 lvImpulseL = MulLanes(lvLocalBiasL, lvInvMassL);
    const V3 lvImpulseA = MulLanes(lvLocalBiasA, lvInvMassA);
    mRows[10].x = lvImpulseL.x; mRows[10].y = lvImpulseL.y; mRows[10].z = lvImpulseL.z;
    mRows[11].x = lvImpulseA.x; mRows[11].y = lvImpulseA.y; mRows[11].z = lvImpulseA.z;

    // ---- the three 0x40-byte groups.
    for (u32 luI = 0u; luI < 3u; ++luI)
    {
        const u32 luBase = 12u + luI * 4u;                  // rows 12, 16, 20 -> +0xC0/0x100/0x140
        const V3  lvLinAxis = Row(lBasis, luI);

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

    mpNode      = const_cast<Drive*>(&lrDrive);   // +0xCC  the only real pointer in the record
    mRows[16].w = lfInvMassA;                     // +0x10C (0.0f unless bodyA is ACTIVE)
    mRows[20].w = lfInvMassB;                     // +0x14C

    // ⚠️ +0x5C IS WRITTEN BY NO PATH IN THIS FUNCTION. Every other lane of the 384 bytes is
    // covered by the map above; this one the console genuinely leaves alone (the joint builder
    // `dcbz128`s the whole record first, so it covers +0x5C with a zero it never revisits).
    // Recorded rather than "tidied": writing a zero here would be an invention.
}

} // namespace physics
} // namespace rw

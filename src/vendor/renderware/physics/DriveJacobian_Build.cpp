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
    //   SOFT_DRIVE : D = (c*h + k*h^2) + 1 ; gains = (k*h^2/D, c*h/D, (c*h + k*h^2)/D)
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
        const __vector4 h = __vspltw(__lvlx(&lfH, 0), 0);
        const f32 dampingValue = lrParams.GetDamping();
        const __vector4 damping = __vspltw(__lvlx(&dampingValue, 0), 0);
        const f32 ch = __vmulfp(h, damping).v[0];
        DriveGains result;
        if (lrParams.GetDriveType() == SOFT_DRIVE)
        {
            const f32 springValue = lrParams.GetSpring();
            const __vector4 spring = __vspltw(__lvlx(&springValue, 0), 0);
            const f32 kh2 = __vmulfp(__vmulfp(h, spring), h).v[0];
            const f32 sum = ch + kh2;
            const f32 denominator = sum + 1.0f;
            result.mfPos = kh2 / denominator;
            result.mfVel = ch / denominator;
            result.mfAcc = sum / denominator;
        }
        else
        {
            result.mfPos = 1.0f / (ch + 1.0f);
            result.mfVel = 1.0f;
            result.mfAcc = 1.0f;
        }
        return result;
    }

    // ARTIST 0x82BC68C8..6958: the drive tail gathers three columns
    // with distinct controls; W takes the first source row's X lane.
    V3 GatherDriveColumn(const V3& a, const V3& b, const V3& c, u32 component)
    {
        static const CGS_ALIGN(16) u32 kX[4] = {0x00010203, 0x10111213, 0x00010203, 0x00010203};
        static const CGS_ALIGN(16) u32 kY[4] = {0x04050607, 0x14151617, 0x00010203, 0x00010203};
        static const CGS_ALIGN(16) u32 kZ[4] = {0x08090A0B, 0x18191A1B, 0x00010203, 0x00010203};
        if (component == 0u)
            return V3(__vrlimi(__vperm(a.mV, b.mV, __lvx(kX, 0)), c.mV, 2, 2));
        if (component == 1u)
            return V3(__vrlimi(__vperm(a.mV, b.mV, __lvx(kY, 0)), c.mV, 2, 3));
        return V3(__vrlimi(__vperm(a.mV, b.mV, __lvx(kZ, 0)), c.mV, 2, 0));
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
    // ARTIST uses one rounded multiply followed by two fused accumulations.
    __vector4 lRA = __vmulfp(__lvx(&lrA.mRi, 0), __vspltw(lvPosA.mV, 0));
    lRA = __vmaddfp(__lvx(&lrA.mUp, 0), __vspltw(lvPosA.mV, 1), lRA);
    lRA = __vmaddfp(__lvx(&lrA.mAt, 0), __vspltw(lvPosA.mV, 2), lRA);
    __vector4 lRB = __vmulfp(__lvx(&lrB.mRi, 0), __vspltw(lvPosB.mV, 0));
    lRB = __vmaddfp(__lvx(&lrB.mUp, 0), __vspltw(lvPosB.mV, 1), lRB);
    lRB = __vmaddfp(__lvx(&lrB.mAt, 0), __vspltw(lvPosB.mV, 2), lRB);
    const V3 lvRA(lRA), lvRB(lRB);

    const V3 lvAnchorA = Add(V3(__lvx(&lrA.mCom, 0)), lvRA);
    const V3 lvAnchorB = Add(V3(__lvx(&lrB.mCom, 0)), lvRB);

    // ** cross(omega, r), NOT cross(r, omega). ** The sign is READ: table 0x82181680 = YZX and
    // 0x82181670 = ZXY, so the fused pair is omega.yzx*r.zxy - omega.zxy*r.yzx = cross(omega, r).
    // BurnoutPR's `pshufd 9` / `pshufd 12h` pair says the same thing.
    // Unlike the joint builder, the drive keeps velocity and acceleration APART -- it has
    // three separate gains to apply to them.
    // ARTIST 0x82BC57C8..5828: fused point motion and acceleration;
    // acceleration is multiplied by the timestep twice, in that order.
    const __vector4 rAYZ = __vpermwi(lvRA.mV, 0x63), rAZX = __vpermwi(lvRA.mV, 0x87);
    const __vector4 rBYZ = __vpermwi(lvRB.mV, 0x63), rBZX = __vpermwi(lvRB.mV, 0x87);
    const __vector4 omegaA = __lvx(&lrA.mOmega, 0), torqueA = __lvx(&lrA.mTorque, 0);
    const __vector4 omegaB = __lvx(&lrB.mOmega, 0), torqueB = __lvx(&lrB.mTorque, 0);
    const __vector4 motionA = __vmaddfp(rAZX, __vpermwi(omegaA, 0x63),
                              __vnmsubfp(rAYZ, __vpermwi(omegaA, 0x87), __lvx(&lrA.mVel, 0)));
    const __vector4 motionB = __vmaddfp(rBZX, __vpermwi(omegaB, 0x63),
                              __vnmsubfp(rBYZ, __vpermwi(omegaB, 0x87), __lvx(&lrB.mVel, 0)));
    const __vector4 accelA = __vmaddfp(rAZX, __vpermwi(torqueA, 0x63),
                             __vnmsubfp(rAYZ, __vpermwi(torqueA, 0x87), __lvx(&lrA.mForce, 0)));
    const __vector4 accelB = __vmaddfp(rBZX, __vpermwi(torqueB, 0x63),
                             __vnmsubfp(rBYZ, __vpermwi(torqueB, 0x87), __lvx(&lrB.mForce, 0)));
    const __vector4 timestep = __vspltw(__lvlx(&lfH, 0), 0);
    const V3 lvVelA(__vmulfp(motionA, timestep)), lvVelB(__vmulfp(motionB, timestep));
    const V3 lvAccA(__vmulfp(__vmulfp(accelA, timestep), timestep));
    const V3 lvAccB(__vmulfp(__vmulfp(accelB, timestep), timestep));

    // =================================================================================
    // BLOCK 3 -- 0x82BC5860..0x82BC5D24   LINEAR HALF.
    //            [X360 + BPR 0x7E3FE5..0x7E4206]  VERIFIED
    // =================================================================================
    const DriveGains lLinGains = ComputeGains(lrD.LinearParams(), lfH);

    const V3 lvErrL  = ClampHardError(Sub(lvAnchorB, lvAnchorA), lrD.LinearParams(), lfH);
    // ARTIST 0x82BC5B14..5B68: velocity product first, followed by
    // fused position and acceleration contributions, across all four lanes.
    const V3 lvDeltaVel = Sub(lvVelB, lvVelA);
    const V3 lvDeltaAcc = Sub(lvAccB, lvAccA);
    const __vector4 lGainVel = __vspltw(__lvlx(&lLinGains.mfVel, 0), 0);
    const __vector4 lGainPos = __vspltw(__lvlx(&lLinGains.mfPos, 0), 0);
    const __vector4 lGainAcc = __vspltw(__lvlx(&lLinGains.mfAcc, 0), 0);
    __vector4 lBiasL = __vmulfp(lvDeltaVel.mV, lGainVel);
    lBiasL = __vmaddfp(lvErrL.mV, lGainPos, lBiasL);
    lBiasL = __vmaddfp(lvDeltaAcc.mV, lGainAcc, lBiasL);
    const V3 lvBiasL(lBiasL);
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
        // ARTIST 0x82BC5C94..5D24 scales each complete RQD row, including W.
        // Assemble from scalar-addressed loads because Jacobian_RQD's declared
        // float array does not itself promise 16-byte alignment.
        const f32* const lpRow = &lRQD.mData[4u * luI];
        __vector4 lRow = __vspltw(__lvlx(lpRow, 0), 0);
        lRow = __vrlimi(lRow, __vspltw(__lvlx(lpRow + 1, 0), 0), 4, 0);
        lRow = __vrlimi(lRow, __vspltw(__lvlx(lpRow + 2, 0), 0), 2, 0);
        lRow = __vrlimi(lRow, __vspltw(__lvlx(lpRow + 3, 0), 0), 1, 0);
        lavAxis[luI] = Scale(V3(lRow), lfS);
        lafErr[luI]  = lfW * lfS;
    }

    const M33 lAngFrame(lavAxis[0], lavAxis[1], lavAxis[2]);

    // 0x82BC5D54..5D6C writes the three errors and an explicit zero W.
    V3 lvErrA = Scale(V3(lafErr[0], lafErr[1], lafErr[2]), KF_ANGLE);
    lvErrA = ClampHardError(lvErrA, lrD.AngularParams(), lfH);

    const V3 lvDOmega  = Scale(Sub(V3(__lvx(&lrB.mOmega, 0)),  V3(__lvx(&lrA.mOmega, 0))),  lfH);
    const V3 lvDTorque = Scale(Sub(V3(__lvx(&lrB.mTorque, 0)), V3(__lvx(&lrA.mTorque, 0))), lfH * lfH);

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
    const M33 lInvIA = lbActiveA ? UnpackInverseInertia(V4(__lvx(&lrA.mIfull, 0)), V4(__lvx(&lrA.mIsplt, 0))) : ZeroMatrix33();
    const M33 lInvIB = lbActiveB ? UnpackInverseInertia(V4(__lvx(&lrB.mIfull, 0)), V4(__lvx(&lrB.mIsplt, 0))) : ZeroMatrix33();

    V3  lavCrossA[3], lavCrossB[3];      // I_A^-1 . (rA x L_i) and I_B^-1 . (rB x L_i)
    V3  lavAngA[3],   lavAngB[3];        // I_A^-1 . angAxis_i  and I_B^-1 . angAxis_i
    f32 lafLinearA[3], lafLinearB[3];
    __vector4 lavAngularMassA[3], lavAngularMassB[3];

    for (u32 luI = 0u; luI < 3u; ++luI)
    {
        const V3 lvLinAxis = Row(lBasis, luI);

        // The angular jacobian of a point constraint is r x n.
        const V3 lvCA = Cross(lvRA, lvLinAxis);
        const V3 lvCB = Cross(lvRB, lvLinAxis);
        const V3 lZero(__vspltisw(0));
        lavCrossA[luI] = lbActiveA ? Transform(lInvIA, lvCA) : lZero;
        lavCrossB[luI] = lbActiveB ? Transform(lInvIB, lvCB) : lZero;
        lavAngA[luI] = lbActiveA ? Transform(lInvIA, lavAxis[luI]) : lZero;
        lavAngB[luI] = lbActiveB ? Transform(lInvIB, lavAxis[luI]) : lZero;

        // ARTIST 0x82BC617C..6270 / 64EC..6570: each active body
        // forms its three dot products separately. Linear mass adds the
        // body's inverse mass with scalar fadds before the vector sum.
        lafLinearA[luI] = lbActiveA ? lfInvMassA + Dot3(lvCA, lavCrossA[luI]) : 0.0f;
        lafLinearB[luI] = lbActiveB ? lfInvMassB + Dot3(lvCB, lavCrossB[luI]) : 0.0f;
        lavAngularMassA[luI] = lbActiveA ? __vmsum3fp(lavAxis[luI].mV, lavAngA[luI].mV) : __vspltisw(0);
        lavAngularMassB[luI] = lbActiveB ? __vmsum3fp(lavAxis[luI].mV, lavAngB[luI].mV) : __vspltisw(0);
    }

    // =================================================================================
    // BLOCK 7 -- 0x82BC6580..0x82BC6A2C   TAIL: normalise and lay the record out.
    // =================================================================================
    // The six effective masses are inverted once and FOLDED INTO the stored rows -- the
    // record never keeps 1/mEff on its own.
    static const CGS_ALIGN(16) u32 kMassGather[4] = {0x00010203, 0x14151617, 0x00010203, 0x00010203};
    const __vector4 lGather = __lvx(kMassGather, 0);
    const __vector4 lAngA = __vrlimi(__vperm(lavAngularMassA[0], lavAngularMassA[1], lGather), lavAngularMassA[2], 2, 0);
    const __vector4 lAngB = __vrlimi(__vperm(lavAngularMassB[0], lavAngularMassB[1], lGather), lavAngularMassB[2], 2, 0);
    const V3 lLinA(lafLinearA[0], lafLinearA[1], lafLinearA[2]);
    const V3 lLinB(lafLinearB[0], lafLinearB[1], lafLinearB[2]);
    __vector4 lMassL = __vaddfp(lLinA.mV, lLinB.mV);
    __vector4 lMassA = __vaddfp(lAngA, lAngB);

    // 0x82BC6624..66EC overwrites only XYZ with fdivs; preserve each
    // accumulated W, including the angular gather's first-dot payload.
    const f32 lfLinearX = 1.0f / lMassL.v[0], lfLinearY = 1.0f / lMassL.v[1], lfLinearZ = 1.0f / lMassL.v[2];
    const f32 lfAngularX = 1.0f / lMassA.v[0], lfAngularY = 1.0f / lMassA.v[1], lfAngularZ = 1.0f / lMassA.v[2];
    lMassL = __vrlimi(lMassL, __vspltw(__lvlx(&lfLinearX, 0), 0), 8, 0);
    lMassL = __vrlimi(lMassL, __vspltw(__lvlx(&lfLinearY, 0), 0), 4, 0);
    lMassL = __vrlimi(lMassL, __vspltw(__lvlx(&lfLinearZ, 0), 0), 2, 0);
    lMassA = __vrlimi(lMassA, __vspltw(__lvlx(&lfAngularX, 0), 0), 8, 0);
    lMassA = __vrlimi(lMassA, __vspltw(__lvlx(&lfAngularY, 0), 0), 4, 0);
    lMassA = __vrlimi(lMassA, __vspltw(__lvlx(&lfAngularZ, 0), 0), 2, 0);
    const V3 lvInvMassL(lMassL), lvInvMassA(lMassA);

    // ---- slots 0 and 1: the two anchor arms, with resolved reaction-force addresses in their w lanes.
    mRows[0].mV.v[0] = lvRA.mV.v[0]; mRows[0].mV.v[1] = lvRA.mV.v[1]; mRows[0].mV.v[2] = lvRA.mV.v[2];   // +0x00
    mRows[0].mV.u[3] = lrA.mId;                                                  // +0x0C
    mRows[1].mV.v[0] = lvRB.mV.v[0]; mRows[1].mV.v[1] = lvRB.mV.v[1]; mRows[1].mV.v[2] = lvRB.mV.v[2];   // +0x10
    mRows[1].mV.u[3] = lrB.mId;                                                  // +0x1C

    // ---- +0x20 and +0x30: explicit zero triples (X360 0x82BC6A08..0x82BC6A1C, `stfs f31`
    //      with f31 = flt_82001CC0 = 0.0f).
    mRows[2].mV.v[0] = mRows[2].mV.v[1] = mRows[2].mV.v[2] = 0.0f;
    mRows[3].mV.v[0] = mRows[3].mV.v[1] = mRows[3].mV.v[2] = 0.0f;

    // ⭐ +0x2C and +0x3C: the LINEAR and ANGULAR acceleration gains. CLOSED BY READING this
    // wave: f6 is last defined at X360 line 240 (`fmr f6,f0`, the NO_DRIVE/HARD_DRIVE arm's
    // 1.0) or line 225 (`fdivs f6,f12,f13`, the SOFT arm) and is NOT redefined anywhere before
    // the store at line 1057 `stfs f6,0x2C(r31)`; f11 likewise between lines 546/557 and the
    // store at line 1058 `stfs f11,0x3C(r31)`. A def-use scan over all 1320 lines confirms it.
    mRows[2].mV.v[3] = lLinGains.mfAcc;   // +0x2C
    mRows[3].mV.v[3] = lAngGains.mfAcc;   // +0x3C

    // ---- the six TRANSPOSED, mass-normalised constraint rows. The solver's inner loop reads
    //      one xyz-component of all three rows at once, so the record stores the transpose:
    //        +0x40/+0x60/+0x80 = (L0, L1, L2) component x/y/z, each scaled by 1/mEffLinear
    //        +0x50/+0x70/+0x90 = (A0, A1, A2) component x/y/z, each scaled by 1/mEffAngular
    //      The lane order follows the controls used by GatherDriveColumn above.
    for (u32 luC = 0u; luC < 3u; ++luC)
    {
        const V3 lvLin = MulLanes(GatherDriveColumn(lBasis.xAxis, lBasis.yAxis, lBasis.zAxis, luC),
                                  lvInvMassL);
        const V3 lvAng = MulLanes(GatherDriveColumn(lavAxis[0], lavAxis[1], lavAxis[2], luC),
                                  lvInvMassA);
        rw::math::vpu::Vector4& lrLinRow = mRows[4u + luC * 2u];   // +0x40, +0x60, +0x80
        rw::math::vpu::Vector4& lrAngRow = mRows[5u + luC * 2u];   // +0x50, +0x70, +0x90
        lrLinRow.mV.v[0] = lvLin.mV.v[0]; lrLinRow.mV.v[1] = lvLin.mV.v[1]; lrLinRow.mV.v[2] = lvLin.mV.v[2];
        lrAngRow.mV.v[0] = lvAng.mV.v[0]; lrAngRow.mV.v[1] = lvAng.mV.v[1]; lrAngRow.mV.v[2] = lvAng.mV.v[2];
    }

    mRows[4].mV.u[3] = lrDrive.m_spy;   // +0x4C  (X360 stores it RAW; BurnoutPR shifts it left by one)

    // ---- the six per-row impulse clamps, mStrength * h^2. Three linear lanes and three
    //      angular ones -- NOT "the tail scales the rows by mStrength", which is what an
    //      earlier reading claimed. The joint builder fills these same lanes with a [lo, hi]
    //      limit pair instead, and JointLimits has no mStrength at all: same record, same
    //      solver, two ways of filling the clamp lanes.
    const f32 lfClampL = lrD.LinearParams().GetMaxStrength()  * lfH * lfH;
    const f32 lfClampA = lrD.AngularParams().GetMaxStrength() * lfH * lfH;
    mRows[6].mV.v[3]  = lfClampL;   // +0x6C
    mRows[7].mV.v[3]  = lfClampL;   // +0x7C
    mRows[8].mV.v[3]  = lfClampA;   // +0x8C
    mRows[9].mV.v[3]  = lfClampA;   // +0x9C
    mRows[10].mV.v[3] = lfClampL;   // +0xAC
    mRows[11].mV.v[3] = lfClampA;   // +0xBC

    // ---- +0xA0 / +0xB0: the PRE-DIVIDED impulses (lambda), one per half.
    const V3 lvImpulseL = MulLanes(lvLocalBiasL, lvInvMassL);
    const V3 lvImpulseA = MulLanes(lvLocalBiasA, lvInvMassA);
    mRows[10].mV.v[0] = lvImpulseL.mV.v[0]; mRows[10].mV.v[1] = lvImpulseL.mV.v[1]; mRows[10].mV.v[2] = lvImpulseL.mV.v[2];
    mRows[11].mV.v[0] = lvImpulseA.mV.v[0]; mRows[11].mV.v[1] = lvImpulseA.mV.v[1]; mRows[11].mV.v[2] = lvImpulseA.mV.v[2];

    // ---- the three 0x40-byte groups.
    for (u32 luI = 0u; luI < 3u; ++luI)
    {
        const u32 luBase = 12u + luI * 4u;                  // rows 12, 16, 20 -> +0xC0/0x100/0x140
        const V3  lvLinAxis = Row(lBasis, luI);

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

    mRows[12].mV.u[3] = reinterpret_cast<u32>(const_cast<Drive*>(&lrDrive));   // +0xCC  drive node pointer
    mRows[16].mV.v[3] = lfInvMassA;                     // +0x10C (0.0f unless bodyA is ACTIVE)
    mRows[20].mV.v[3] = lfInvMassB;                     // +0x14C

    // ⚠️ +0x5C IS WRITTEN BY NO PATH IN THIS FUNCTION. Every other lane of the 384 bytes is
    // covered by the map above; this one the console genuinely leaves alone (the joint builder
    // `dcbz128`s the whole record first, so it covers +0x5C with a zero it never revisits).
    // Recorded rather than "tidied": writing a zero here would be an invention.
}

} // namespace physics
} // namespace rw

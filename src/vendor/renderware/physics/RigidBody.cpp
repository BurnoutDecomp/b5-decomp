#include "rw/physics/rigidbody.h"
#include "rw/physics/simulation.h"   // the owning Simulation's parameters
#include "JointFrames.hpp"           // rw::math::vpu::QuaternionFromMatrix33 (its current home) -- SetTransform


// ===========================================================================
// rw::physics::RigidBody -- definition home for the two RigidBody methods the
// X360 binary carries as their own TU. Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   RigidBody::operator=      @ 0x825E3410   (BODIED below)
//   RigidBody::DynamicUpdate  @ 0x82BC2B78   (BODIED below, 2026-08-04)
//
// The RigidBody type itself lives in its canonical home, rw/physics/rigidbody.h.
// These two methods were grown into that header additively (declarations only).
// ===========================================================================

namespace rw
{
namespace physics
{

// ---------------------------------------------------------------------------
// RigidBody::operator= @ 0x825E3410
//
// A full field copy of the 176-byte (0xB0) body. The asm copies the first
// 16-byte register at +0x00 (lvx128/stvx128) then every 4-byte field from
// +0x10 to +0xAC (lfs/stfs for the float lanes, lwz/stw for the integer
// housekeeping lanes at +0x1C/+0x2C/+0x3C/.../+0xAC).
//
// ⛔⛔ CORRECTED 2026-08-05 (the rigid-body drain group). This body used to copy ONLY the
// eleven vector registers "by name" -- but on the console the +0x10..+0xAC range CONTAINS
// the ten packed housekeeping scalars (they live in the vectors' w lanes there), so the
// vectors-only spelling omitted ten field copies on the PC -- with no diagnostic -- where those scalars
// are their own members. That was exactly the wrong-but-plausible shape: every caller
// still linked and the pose still copied.
// ⭐ THE WITNESS THAT SETTLED IT IS A CALLER'S OWN SAVE/RESTORE: ProcessUpdateRigidBodyQueue
// @0x828A3AD4 saves the destination's +0x2C/+0x3C (mRight/mLeft) around this call and puts
// them back afterwards -- which is only meaningful if operator= CLOBBERS the intrusive
// links, i.e. if it copies the w-lane payloads too. (The OutUpdateRigidBody AddEvent
// @0x828A66F8 also delegates its 192-byte element copy here, id slot aside.)
// (RigidBody's members are private; this method is a member, so the named copy is
// well-formed.)
// ---------------------------------------------------------------------------
RigidBody& RigidBody::operator=(const RigidBody& rOther)
{
    mQuat   = rOther.mQuat;     // +0x00  (lvx128/stvx128 register copy)
    mCom    = rOther.mCom;      // +0x10
    mVel    = rOther.mVel;      // +0x20
    mOmega  = rOther.mOmega;    // +0x30
    mRi     = rOther.mRi;       // +0x40
    mUp     = rOther.mUp;       // +0x50
    mAt     = rOther.mAt;       // +0x60
    mIfull  = rOther.mIfull;    // +0x70
    mIsplt  = rOther.mIsplt;    // +0x80
    mForce  = rOther.mForce;    // +0x90
    mTorque = rOther.mTorque;   // +0xA0  (ends at +0xAC)

    // The ten w-lane payloads, in ascending console-offset order. Each line IS one of the
    // `lwz/stw` (or `lfs/stfs`) pairs of the +0x10..+0xAC sweep; on the PC they are the
    // named scalars the banner above describes.
    mId      = rOther.mId;      // console +0x1C (mCom.w)
    mRight   = rOther.mRight;   // console +0x2C (mVel.w)   -- callers that must keep their
    mLeft    = rOther.mLeft;    // console +0x3C (mOmega.w)    links save/restore around this
    mStasis  = rOther.mStasis;  // console +0x4C (mRi.w)
    mInertia = rOther.mInertia; // console +0x5C (mUp.w)
    mTag     = rOther.mTag;     // console +0x6C (mAt.w)
    mInvm    = rOther.mInvm;    // console +0x7C (mIfull.w)
    mState   = rOther.mState;   // console +0x8C (mIsplt.w)
    mKine    = rOther.mKine;    // console +0x9C (mForce.w)
    mCool    = rOther.mCool;    // console +0xAC (mTorque.w)
    return *this;
}

// ---------------------------------------------------------------------------
// The three read accessors ExternallySimulatedBody::ReadFromRenderware @0x825A2E88
// consumes. ADDITIVE 2026-08-02 (physics wave 3): they were declared-only in
// rigidbody.h, which made ExternallySimulatedBody.cpp unlinkable (three of the 17
// LNK2019s measured for the vehicle-dynamics core).
//
// They have no X360 symbol of their own -- the console inlines them into the
// caller, and the caller's asm IS the specification. ExternallySimulatedBody.cpp's
// own committed banner records that asm:
//     mTransform       = GetTransform()        "copies the body's mRi/mUp/mAt
//                                               orientation rows + mCom position"
//     mLinearVelocity  = GetLinearVelocity()   "lvx r4+0x20 -> this+0x40"
//     mAngularVelocity = GetAngularVelocity()  "lvx r4+0x30 -> this+0x50"
// and rigidbody.h's committed member sequence puts mCom at +0x10, mVel at +0x20,
// mOmega at +0x30, mRi at +0x40, mUp at +0x50, mAt at +0x60 -- so the two velocity
// loads land on mVel / mOmega EXACTLY, and the three orientation rows the transform
// is assembled from are mRi / mUp / mAt with mCom as the translation row. Nothing
// here is inferred beyond that already-committed offset table.
//
// The console packs housekeeping scalars in each register's w lane (mCom.w == mId,
// mVel.w == mRight, ...). Matrix44Affine / Vector3 are Vector3-shaped (x,y,z,w) so a
// lane-for-lane copy would carry those scalars into the caller's w lanes, exactly as
// the console's lvx/stvx register copies do. The w lanes are reproduced verbatim
// rather than zeroed, because the console copy is a whole-register move.
// ---------------------------------------------------------------------------
rw::math::vpu::Matrix44Affine RigidBody::GetTransform() const
{
    rw::math::vpu::Matrix44Affine lResult;
    // Each aligned native row comprises the packed triple and its adjacent word.
    lResult.xAxis = __lvx(&mRi, 0);
    lResult.yAxis = __lvx(&mUp, 0);
    lResult.zAxis = __lvx(&mAt, 0);
    lResult.wAxis = __lvx(&mCom, 0);
    return lResult;
}

rw::math::vpu::Vector3 RigidBody::GetLinearVelocity() const
{
    return rw::math::vpu::Vector3(__lvx(&mVel, 0));
}

rw::math::vpu::Vector3 RigidBody::GetAngularVelocity() const
{
    return rw::math::vpu::Vector3(__lvx(&mOmega, 0));
}

// ---------------------------------------------------------------------------
// RigidBody::GetLocalInvInertiaDiagonal -- now an inline member read in
// rigidbody.h. The note that stood here said its console storage is "a POINTER
// packed into the mUp.w float lane ... which the committed 64-bit PC layout
// cannot represent". That was true of the old layout; the Inertia pointer is a
// real named member now, so the accessor is trivial and its one caller
// (ExternalPhysicsBody::ReadPropertiesFromRenderware) links and is mounted.
// ---------------------------------------------------------------------------

namespace
{
    // ---------------------------------------------------------------------------------------
    // THE ORIENTATION TAIL OF DynamicUpdate (X360 0x82BC2C58..0x82BC2D38) is two rwmath inlines,
    // added 2026-09-24 (H2-D1). Their ORIGINAL SOURCE is rwmath 1.02.00 in the Feb-2007 drop
    // (EARenderWare/stable/external/rwmath/1.02.00/include/rw/math/vpu/detail/):
    //     quaternion_operation_inline.h:152        Quaternion Normalize(Quaternion::InParam)
    //     matrix33_operation_platform_inline.h:30  Matrix33   Matrix33FromQuaternion(Quaternion::InParam)
    // -- PS3 AltiVec spellings; the X360 lowering transcribed below is read off DynamicUpdate's
    // words (vmsum4fp128 and the fused vnmsubfp / vmaddfp are X360-only). Neither is
    // rw::physics::Quaternion::UnitQuaternionToMatrix @0x82BC3EC0, which is a scalar routine
    // with a different rounding (Quaternion.cpp).
    // ⚠️ CANONICAL HOME: rw/math/vpu/quaternion_operation.h / matrix33_operation.h, which do not
    // exist in this tree yet -- the footing JointFrames.hpp records for QuaternionFromMatrix33.
    // Move them there when those headers are grown: the replay serialisers' Get{Prop,Part}
    // Transform @0x822BB944 and SoundSerialiser::ReadAsQuatPos @0x826534E0 inline the same
    // Matrix33FromQuaternion (same gSqrt2s, same permute constants 0x82CDA3D0/450/410).
    // ---------------------------------------------------------------------------------------

    // rw::math::vpu::detail::gSqrt2s @0x821814F0: 0x3FB504F3 (1.41421354f) in all four lanes.
    const f32 KF_SQRT2 = 1.41421354f;

    // ARTIST 0x82BC2C58..0x82BC2C90: two fused refinements,
    // without a zero or NaN guard.
    inline rw::math::vpu::Quaternion Normalize(const rw::math::vpu::Quaternion& lrQ)
    {
        const __vector4 q = __lvx(&lrQ, 0);
        const __vector4 squared = __vmsum4fp(q, q);
        const __vector4 one = __vcfsx(__vspltisw(1), 0);
        const __vector4 half = __vcfsx(__vspltisw(1), 1);
        __vector4 reciprocal = __vrsqrtefp(squared);
        for (int step = 0; step != 2; ++step)
        {
            const __vector4 error = __vnmsubfp(squared, __vmulfp(reciprocal, reciprocal), one);
            reciprocal = __vmaddfp(__vmulfp(reciprocal, half), error, reciprocal);
        }
        rw::math::vpu::Quaternion result;
        __stvx(__vmulfp(q, reciprocal), &result, 0);
        return result;
    }

    // rwmath Matrix33FromQuaternion, X360 0x82BC2C94..0x82BC2D14 (the source's names quoted):
    //   s     = gSqrt2s * q            "scaled_q"        vmulfp128 @0x82BC2C9C
    //   d     = 0.5 - s*s              "neg_qq"          vnmsubfp @0x82BC2CA8 -- ONE rounding
    //   p     = s * s.yzxx             "xy_yz_zx"        vpermwi128 0x60 @0x82BC2CAC, vmulfp128
    //   r     = s.w * s.zxyx           "wz_wx_wy"        vpermwi128 0x84 + vspltw 3, vmulfp128
    //   add   = p + r ; sub = p - r    "addVec"/"subVec" vaddfp @0x82BC2CCC, vsubfp @0x82BC2CD4
    //   diag  = d + d.yzxx             "diagonalValues"  vaddfp @0x82BC2CC4
    //   xAxis = (diag.y, add.x, sub.z) vperm 0x82CDA3D0 (A.y,B.x) + vrlimi128 z <- sub.z,  rot 0
    //   yAxis = (sub.x, diag.z, add.y) vperm 0x82CDA450 (A.x,B.z) + vrlimi128 z <- add.y,  rot 3
    //   zAxis = (add.z, sub.y, diag.x) vperm 0x82CDA410 (A.z,B.y) + vrlimi128 z <- diag.x, rot 2
    // With s = sqrt(2) q that is the textbook basis, but ROUNDED DIFFERENTLY from 0x82BC3EC0.
    // The console's row w lanes are permute leftovers that DynamicUpdate's stores replace with the
    // member's own w (vrlimi128 mask 1).
    inline rw::math::vpu::Matrix33 Matrix33FromQuaternion(const rw::math::vpu::Quaternion& lrQ)
    {
        static const CGS_ALIGN(16) u32 kX[4] = {0x04050607, 0x10111213, 0x00010203, 0x00010203};
        static const CGS_ALIGN(16) u32 kY[4] = {0x00010203, 0x18191A1B, 0x00010203, 0x00010203};
        static const CGS_ALIGN(16) u32 kZ[4] = {0x08090A0B, 0x14151617, 0x00010203, 0x00010203};
        static const CGS_ALIGN(16) f32 kSqrt2[4] = {KF_SQRT2, KF_SQRT2, KF_SQRT2, KF_SQRT2};
        const __vector4 s = __vmulfp(__lvx(&lrQ, 0), __lvx(kSqrt2, 0));
        const __vector4 half = __vcfsx(__vspltisw(1), 1);
        const __vector4 d = __vnmsubfp(s, s, half);
        const __vector4 p = __vmulfp(s, __vpermwi(s, 0x60));
        const __vector4 r = __vmulfp(__vspltw(s, 3), __vpermwi(s, 0x84));
        const __vector4 diagonal = __vaddfp(d, __vpermwi(d, 0x60));
        const __vector4 add = __vaddfp(p, r);
        const __vector4 sub = __vsubfp(p, r);
        const __vector4 x = __vrlimi(__vperm(diagonal, add, __lvx(kX, 0)), sub, 2, 0);
        const __vector4 y = __vrlimi(__vperm(sub, diagonal, __lvx(kY, 0)), add, 2, 3);
        const __vector4 z = __vrlimi(__vperm(add, sub, __lvx(kZ, 0)), diagonal, 2, 2);
        return rw::math::vpu::Matrix33(x, y, z);
    }
}

// ---------------------------------------------------------------------------
// RigidBody::InertiaUpdate(Inertia*)   -- DWARF rigidbody.h:387. NO OWN X360 SYMBOL:
// the console inlines it, and it inlines it TWICE, which is the whole reason it is
// factored out here rather than copied:
//     DynamicUpdate            0x82BC2D60 .. 0x82BC2DE8
//     Simulation::AddRigidBody 0x82BC35A8 .. 0x82BC365C   (once per BodyState path)
// Both emissions are the same instruction sequence, transcribed from AddRigidBody's copy
// (task #140) and checked against DynamicUpdate's, which was already in this file:
//     vspltw v9/v10/v7 <- the Inertia's inverse-inertia diagonal, lanes 0/1/2
//     vmulfp128 v0,v13,v9  /  v10,v12,v10  /  v9,v11,v7      = I_k * r_k, k = Ri/Up/At
//     vpermwi128 .. 0x97 (.zyyw) and 0x9B (.zyzw)            = the (2,2)/(1,1)/(1,2) gather
//     vspltw .. ,0                                           = r_k.x
//     four vmaddfp                                           = the two accumulations
//
// I_world = sum_k I_k * (r_k (x) r_k). The tensor is symmetric, so only the six unique
// terms are kept, in the split rigidbody.h documents:
//     mIfull.xyz = { W00, W01, W02 }   ("Ixx, Ixy, Ixz")
//     mIsplt.xyz = { W22, W11, W12 }   ("Izz, Iyy, Iyz")  -- Izz FIRST
// (BurnoutPR writes those six sums to +112/+116/+120 and +128/+132/+136 in that order.)
//
// ⚠️ `vmaddfp vD,vA,vB,vC` is `vD = vA*vC + vB`. Applying it left-to-right transposes the
// tensor, and nothing in this subsystem would assert -- the car would simply tumble wrong.
// The two accumulations decode as:
//     v0  = (At*Iz)*At.x + [ (Ri*Ix)*Ri.x + (Up*Iy)*Up.x ]                 -> mIfull
//     v13 = At.zyyw*(At*Iz).zyzw + [ Ri.zyyw*(Ri*Ix).zyzw + Up.zyyw*(Up*Iy).zyzw ] -> mIsplt
// which is exactly the {W00,W01,W02} / {W22,W11,W12} pair below.
//
// The tensor stores preserve mInvm and mState. AddRigidBody then refreshes mInvm
// from the Inertia block; DynamicUpdate's tensor rebuild leaves it unchanged.
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// RigidBody::SetTransform(const Matrix44Affine&)   -- DWARF rigidbody.h:200. NO OWN X360
// SYMBOL: the console inlines it into PhysicsSimulationModule::ProcessUpdateExternalBodyQueue
// @0x828A3E28..0x828A3FAC, which is the transcription basis (task: the rigid-body drain
// group, 2026-08-05).
//
// The inline decomposes as:
//   1. four w-preserving row copies (`vrlimi128 vD,vOld,1,0` before every store):
//          mRi  (+0x40) <- xAxis     mUp  (+0x50) <- yAxis
//          mAt  (+0x60) <- zAxis     mCom (+0x10) <- wAxis (the position row)
//      The original W words (mStasis/mInertia/mTag/mId) are retained.
//   2. mQuat (+0x00) = QuaternionFromMatrix33(upper 3x3), stored as a FULL 16-byte row (the
//      one store in the block with no vrlimi -- mQuat.w is the quaternion's real part, not a
//      packed scalar). The console computes it from the ARGUMENT's rows (`lvx128` from the
//      event copy at r30+0x10/+0x20/+0x30), not from the just-written body rows; same values,
//      but the source is transcribed as-is. The epsilon the console feeds the vsel network is
//      `lvlx` of flt_820F105C == 0.0f (byte-read via x360rd this wave) == the committed
//      inline's defaulted argument, so no explicit epsilon is passed.
//
// ⚠️ WHAT IS *NOT* IN HERE: the drain follows this inline with the `if (mInertia != NULL)`
// world-inertia rebuild. That block is RigidBody::InertiaUpdate (above) -- a separate DWARF
// method with its own guard at the CALL SITE, exactly as Simulation::AddRigidBody emits it.
// Folding it into SetTransform would make the two other InertiaUpdate call sites wrong.
// ---------------------------------------------------------------------------
void RigidBody::SetTransform(const rw::math::vpu::Matrix44Affine& lrTransform)
{
    __stvx(__vrlimi(lrTransform.xAxis.mV, __lvx(&mRi, 0), 1, 0), &mRi, 0);
    __stvx(__vrlimi(lrTransform.yAxis.mV, __lvx(&mUp, 0), 1, 0), &mUp, 0);
    __stvx(__vrlimi(lrTransform.zAxis.mV, __lvx(&mAt, 0), 1, 0), &mAt, 0);
    __stvx(__vrlimi(lrTransform.wAxis.mV, __lvx(&mCom, 0), 1, 0), &mCom, 0);

    rw::math::vpu::Matrix33 lBasis;
    lBasis.xAxis = lrTransform.xAxis;
    lBasis.yAxis = lrTransform.yAxis;
    lBasis.zAxis = lrTransform.zAxis;
    mQuat = rw::math::vpu::QuaternionFromMatrix33(lBasis);   // epsilon 0.0f (flt_820F105C)
}

// ---------------------------------------------------------------------------
// RigidBody::DynamicUpdate @ 0x82BC2B78   (276 instructions, 71% VMX)
//
// One body, one tick. This is a POSITION-LEVEL (Verlet-flavoured) integrator: it forms
// position/orientation DELTAS from acceleration + velocity + the solver's accumulated
// impulses, applies them, then BACK-DERIVES the velocities as delta/dt with damping. That
// asymmetry is real and is confirmed on all three builds.
//
// TRANSCRIPTION BASIS
//   * X360 0x82BC2B78, read instruction by instruction (every offset below is quoted).
//   * BurnoutPR 0x5992A60 -- Hex-Rays `_mm_*`, struct offsets byte-identical to X360.
//   * Burnout_External_Xbox_One.exe 0x1409B3180 (AVX) for the x64 widening.
//
// ⚠️ `vmaddfp vD,vA,vB,vC` is `vD = vA*vC + vB` -- the THIRD printed operand is the second
// multiplicand and the SECOND is the addend. That is the AltiVec definition, not an IDA
// quirk (proved from the image word at 0x82BC4E88 = 0x139D1C6E, whose VD/VA/VB/VC fields
// decode field-for-field). Read left to right, every expression here comes out transposed.
//
// SIGNATURE: r3 is never reassigned, so the function RETURNS `this`. Settled by the caller:
// BatchIntegrator does `lwz r3,0x2C(r3)` on the RESULT, and the Xbox One build's inlined
// copy reads `mov rdi,[rdi+30h]` from the body it passed IN. BurnoutPR's Hex-Rays
// `return *(a1+76)` (the Simulation*) is an artefact of the last value left in eax.
// ---------------------------------------------------------------------------
RigidBody* RigidBody::DynamicUpdate()
{
    Simulation* const lpSim = mStasis;
    const f32 lfDt = lpSim->GetTimeStep();
    const __vector4 dt = __vspltw(__lvlx(&lfDt, 0), 0);
    const __vector4 one = __vcfsx(__vspltisw(1), 0);
    const __vector4 half = __vcfsx(__vspltisw(1), 1);
    rw::math::vpu::Vector4* const lpAccum = reinterpret_cast<rw::math::vpu::Vector4*>(mId);

    const __vector4 velocity = __vmaddfp(__lvx(&mForce, 0), dt, __lvx(&mVel, 0));
    const __vector4 omega = __vmaddfp(__lvx(&mTorque, 0), dt, __lvx(&mOmega, 0));
    const __vector4 move = __vmaddfp(velocity, dt, lpAccum[0].mV);
    const __vector4 turn = __vmaddfp(omega, dt, lpAccum[2].mV);
    const __vector4 oldCom = __lvx(&mCom, 0);
    const __vector4 com = __vaddfp(__vaddfp(oldCom, lpAccum[1].mV), move);
    __stvx(__vrlimi(com, oldCom, 1, 0), &mCom, 0);

    const __vector4 spin = __vaddfp(turn, lpAccum[3].mV);
    const __vector4 q = __lvx(&mQuat, 0);
    const __vector4 first = __vmulfp(spin, __vpermwi(q, 0x63));
    const __vector4 cross = __vpermwi(__vnmsubfp(__vpermwi(spin, 0x63), q, first), 0x63);
    const __vector4 vec = __vmulfp(__vmaddfp(spin, __vspltw(q, 3), cross), half);
    const __vector4 sign = __vslw(__vspltisw(-1), __vspltisw(-1));
    const __vector4 real = __vmulfp(__vmsum3fp(spin, q), __vxor(half, sign));
    rw::math::vpu::Quaternion qNew;
    __stvx(__vaddfp(q, __vrlimi(vec, real, 1, 0)), &qNew, 0);
    mQuat = Normalize(qNew);
    const rw::math::vpu::Matrix33 basis = Matrix33FromQuaternion(mQuat);
    __stvx(__vrlimi(basis.xAxis.mV, __lvx(&mRi, 0), 1, 0), &mRi, 0);
    __stvx(__vrlimi(basis.yAxis.mV, __lvx(&mUp, 0), 1, 0), &mUp, 0);
    __stvx(__vrlimi(basis.zAxis.mV, __lvx(&mAt, 0), 1, 0), &mAt, 0);

    // DynamicUpdate rebuilds inertia without the inverse-mass reload used by AddRigidBody.
    const Inertia* const lpInertia = mInertia;
    const __vector4 ri = __lvx(&mRi, 0), up = __lvx(&mUp, 0), at = __lvx(&mAt, 0);
    const __vector4 diagonal = lpInertia->GetInverseInertia().mV;
    const __vector4 ir = __vmulfp(ri, __vspltw(diagonal, 0));
    const __vector4 iu = __vmulfp(up, __vspltw(diagonal, 1));
    const __vector4 ia = __vmulfp(at, __vspltw(diagonal, 2));
    __vector4 full = __vmulfp(iu, __vspltw(up, 0));
    full = __vmaddfp(ir, __vspltw(ri, 0), full);
    full = __vmaddfp(ia, __vspltw(at, 0), full);
    __stvx(__vrlimi(full, __lvx(&mIfull, 0), 1, 0), &mIfull, 0);
    __vector4 split = __vmulfp(__vpermwi(up, 0x97), __vpermwi(iu, 0x9B));
    split = __vmaddfp(__vpermwi(ri, 0x97), __vpermwi(ir, 0x9B), split);
    split = __vmaddfp(__vpermwi(at, 0x97), __vpermwi(ia, 0x9B), split);
    __stvx(__vrlimi(split, __lvx(&mIsplt, 0), 1, 0), &mIsplt, 0);

    const __vector4 invDt = rw::math::vpu::detail::Vector3NativeReciprocal(dt);
    const f32 linDrag = lpInertia->GetLinearDrag(), angDrag = lpInertia->GetAngularDrag();
    const __vector4 linScale = __vmulfp(__vsubfp(one, __vspltw(__lvlx(&linDrag, 0), 0)), invDt);
    const __vector4 angScale = __vmulfp(__vsubfp(one, __vspltw(__lvlx(&angDrag, 0), 0)), invDt);
    __vector4 linear = __vmulfp(move, linScale);
    __vector4 angular = __vmulfp(turn, angScale);
    f32 angMeasure = __vmsum3fp(angular, angular).v[0];
    const f32 angMax = lpInertia->GetMaxAngularVelocity();
    const f32 angMaxSq = angMax * angMax;
    if (angMeasure > angMaxSq)
    {
        const f32 ratio = angMaxSq / angMeasure;
        const __vector4 x = __vspltw(__lvlx(&ratio, 0), 0);
        const __vector4 root = rw::math::vpu::detail::Vector3NativeReciprocalRoot(x, 2);
        const __vector4 zero = __vspltisw(0);
        angular = __vmulfp(angular, __vsel(__vmulfp(x, root), zero, __vcmpeqfp(x, zero)));
        angMeasure = angMaxSq;
    }
    f32 linMeasure = __vmsum3fp(linear, linear).v[0];
    const f32 linMax = lpInertia->GetMaxLinearVelocity();
    const f32 linMaxSq = linMax * linMax;
    if (linMeasure > linMaxSq)
    {
        const f32 ratio = linMaxSq / linMeasure;
        const __vector4 x = __vspltw(__lvlx(&ratio, 0), 0);
        const __vector4 root = rw::math::vpu::detail::Vector3NativeReciprocalRoot(x, 2);
        const __vector4 zero = __vspltisw(0);
        linear = __vmulfp(linear, __vsel(__vmulfp(x, root), zero, __vcmpeqfp(x, zero)));
        linMeasure = linMaxSq;
    }
    const f32 sphericalMass = lpInertia->GetSphericalInertia() * mInvm;
    const f32 energy = sphericalMass * angMeasure + linMeasure;
    if (!(energy < lpSim->GetFreezingEnergy()))
        mCool = 0;
    else
    {
        if (!(energy > mKine)) ++mCool;
        if (mCool > lpSim->GetCoolDown()) mCool = lpSim->GetCoolDown();
    }
    mKine = energy;
    __stvx(__vrlimi(linear, __lvx(&mVel, 0), 1, 0), &mVel, 0);
    __stvx(__vrlimi(angular, __lvx(&mOmega, 0), 1, 0), &mOmega, 0);
    const __vector4 zero = __vspltisw(0);
    for (int i = 0; i != 4; ++i) lpAccum[i].mV = zero;
    __stvx(__vrlimi(lpSim->GetGravity().mV, __lvx(&mForce, 0), 1, 0), &mForce, 0);
    __stvx(__vrlimi(zero, __lvx(&mTorque, 0), 1, 0), &mTorque, 0);
    return this;
}

} // namespace physics
} // namespace rw

#include "rw/physics/rigidbody.h"
#include "rw/physics/simulation.h"   // the owning Simulation's parameters
#include "JointFrames.hpp"           // rw::math::vpu::QuaternionFromMatrix33 (its current home) -- SetTransform

#include <cmath>     // std::sqrt, std::fmaf (the fused vmaddfp / vnmsubfp of DynamicUpdate's normalise)
#include <cstring>   // std::memcpy (RecipSqrtEstimate's significand cut)

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
    lResult.xAxis = rw::math::vpu::Vector3{ mRi.x,  mRi.y,  mRi.z,  mRi.w  };   // +0x40
    lResult.yAxis = rw::math::vpu::Vector3{ mUp.x,  mUp.y,  mUp.z,  mUp.w  };   // +0x50
    lResult.zAxis = rw::math::vpu::Vector3{ mAt.x,  mAt.y,  mAt.z,  mAt.w  };   // +0x60
    lResult.wAxis = rw::math::vpu::Vector3{ mCom.x, mCom.y, mCom.z, mCom.w };   // +0x10
    return lResult;
}

rw::math::vpu::Vector3 RigidBody::GetLinearVelocity() const
{
    return rw::math::vpu::Vector3{ mVel.x, mVel.y, mVel.z, mVel.w };            // +0x20
}

rw::math::vpu::Vector3 RigidBody::GetAngularVelocity() const
{
    return rw::math::vpu::Vector3{ mOmega.x, mOmega.y, mOmega.z, mOmega.w };    // +0x30
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
    // Local scalar helpers over the vendored rw::math::vpu POD vectors. Deliberately
    // scalar rather than intrinsic: X360 is VMX128 and PC is SSE/AVX, neither ISA is
    // portable, and BurnoutPR's Hex-Rays `_mm_*` output is readable but not compilable.
    // The LANE ASSIGNMENTS below carry the fidelity, not the instruction set.
    struct V3 { f32 x, y, z; };

    inline V3 vLoad3(const rw::math::vpu::Vector4& lrV)
    { V3 lResult = { lrV.x, lrV.y, lrV.z }; return lResult; }

    inline V3 vLoad3(const rw::math::vpu::Vector3& lrV)
    { V3 lResult = { lrV.x, lrV.y, lrV.z }; return lResult; }

    // Store x/y/z, LEAVING w ALONE. Every vector store in DynamicUpdate does exactly this
    // on all three builds -- X360 `vrlimi128 vD,vOld,1,0`, BurnoutPR
    // `_mm_shuffle_ps(new, unpackhi(old,new), 148)`, Xbox One
    // `vshufps xmm2, new, vunpckhps(old,new), 94h` (148 == 0x94, the same immediate).
    // The w lane is arithmetically garbage during the computation and is restored from the
    // ORIGINAL value -- the signature of a non-float payload.
    inline void vStore3(rw::math::vpu::Vector4& lrDst, const V3& lrSrc)
    { lrDst.x = lrSrc.x; lrDst.y = lrSrc.y; lrDst.z = lrSrc.z; }   // w untouched

    inline V3 vMul(const V3& lrA, f32 lfS)
    { V3 r = { lrA.x * lfS, lrA.y * lfS, lrA.z * lfS }; return r; }

    inline V3 vAdd(const V3& lrA, const V3& lrB)
    { V3 r = { lrA.x + lrB.x, lrA.y + lrB.y, lrA.z + lrB.z }; return r; }

    inline f32 vDot(const V3& lrA, const V3& lrB)
    { return lrA.x * lrB.x + lrA.y * lrB.y + lrA.z * lrB.z; }

    inline V3 vCross(const V3& lrA, const V3& lrB)
    {
        V3 r = { lrA.y * lrB.z - lrA.z * lrB.y,
                 lrA.z * lrB.x - lrA.x * lrB.z,
                 lrA.x * lrB.y - lrA.y * lrB.x };
        return r;
    }

    const u32 KU_REACTION_BLOCK_BYTES = 64u;   // Xbox One `shl rdi,6`

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

    // vrsqrtefp (X360 0x82BC2C68). The console's estimate table is not reproducible on the PC;
    // the architecture bounds its relative error by 1/4096 and nothing more. The stand-in is the
    // reciprocal square root with its significand cut to 13 bits (error < 2^-12, inside that
    // bound) -- deliberately NOT the exact value, because what the console keeps is the result
    // of TWO fused refinements of the estimate (RefineRecipSqrt), and that result does not
    // depend on the estimate's exact bits but does depend on its being coarse. MEASURED on |q|^2
    // values of the kind DynamicUpdate normalises: for any estimate whose error lies between
    // 2^-20 and 2^-9 the refined value is the same float in 98.5-99.98% of cases, while an exact
    // seed lands on a different float in ~5% of them (and 1.0f/std::sqrt without refinement, the
    // old spelling, in ~48%). Evidence: the FX-RIGIDBODY lane log / H2-D1 commit message.
    inline f32 RecipSqrtEstimate(f32 lfValue)
    {
        const f32 lfExact = 1.0f / std::sqrt(lfValue);
        u32 luBits;
        std::memcpy(&luBits, &lfExact, sizeof(luBits));
        luBits &= 0xFFFFF800u;   // sign, exponent and the top 12 fraction bits
        f32 lfEstimate;
        std::memcpy(&lfEstimate, &luBits, sizeof(lfEstimate));
        return lfEstimate;
    }

    // One Newton-Raphson step towards 1/sqrt(n), in the console's order and fusion:
    //     vmulfp128 e*e ; vmulfp128 e*0.5 ; vnmsubfp r = 1 - n*(e*e) ; vmaddfp e' = (e*0.5)*r + e
    // X360 0x82BC2C6C..0x82BC2C78, and again 0x82BC2C7C..0x82BC2C88. vnmsubfp / vmaddfp round
    // ONCE (hence std::fmaf). The 0.5 and 1.0 are `vcfsx` of the `vspltisw 1` splat (0x82BC2BC8
    // scale 1, 0x82BC2C58 scale 0) -- built in registers, no data read.
    inline f32 RefineRecipSqrt(f32 lfValue, f32 lfEstimate)
    {
        const f32 lfResidual = std::fmaf(-lfValue, lfEstimate * lfEstimate, 1.0f);
        return std::fmaf(lfEstimate * 0.5f, lfResidual, lfEstimate);
    }

    // rwmath Normalize(Quaternion), X360 0x82BC2C5C..0x82BC2C8C:
    //     n = vmsum4fp128(q, q) ; e = vrsqrtefp(n) ; e = refine(e) ; e = refine(e) ; q * e
    // ⚠️ vmsum4fp128's internal rounding is not visible in the listing; it is lowered as the
    // plain left-to-right float sum, the tree's standing lowering (vector4_operation.h Magnitude).
    // No zero guard: the source's Normalize has none and neither do the words, so a zero
    // quaternion comes out NaN on both builds.
    inline rw::math::vpu::Quaternion Normalize(const rw::math::vpu::Quaternion& lrQ)
    {
        const f32 lfDot = lrQ.x * lrQ.x + lrQ.y * lrQ.y + lrQ.z * lrQ.z + lrQ.w * lrQ.w;
        const f32 lfRecipSqrt = RefineRecipSqrt(lfDot, RefineRecipSqrt(lfDot, RecipSqrtEstimate(lfDot)));
        const rw::math::vpu::Quaternion lResult = { lrQ.x * lfRecipSqrt, lrQ.y * lfRecipSqrt,
                                                    lrQ.z * lfRecipSqrt, lrQ.w * lfRecipSqrt };
        return lResult;
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
    // member's own w (vrlimi128 mask 1); they are written 0 here and never stored.
    inline rw::math::vpu::Matrix33 Matrix33FromQuaternion(const rw::math::vpu::Quaternion& lrQ)
    {
        const f32 lfSX = KF_SQRT2 * lrQ.x, lfSY = KF_SQRT2 * lrQ.y;
        const f32 lfSZ = KF_SQRT2 * lrQ.z, lfSW = KF_SQRT2 * lrQ.w;

        const f32 lfDX = std::fmaf(-lfSX, lfSX, 0.5f);
        const f32 lfDY = std::fmaf(-lfSY, lfSY, 0.5f);
        const f32 lfDZ = std::fmaf(-lfSZ, lfSZ, 0.5f);

        const f32 lfXY = lfSX * lfSY, lfYZ = lfSY * lfSZ, lfZX = lfSZ * lfSX;
        const f32 lfWZ = lfSW * lfSZ, lfWX = lfSW * lfSX, lfWY = lfSW * lfSY;

        rw::math::vpu::Matrix33 lResult;
        lResult.xAxis = { lfDY + lfDZ, lfXY + lfWZ, lfZX - lfWY, 0.0f };
        lResult.yAxis = { lfXY - lfWZ, lfDZ + lfDX, lfYZ + lfWX, 0.0f };
        lResult.zAxis = { lfZX + lfWY, lfYZ - lfWX, lfDX + lfDY, 0.0f };
        return lResult;
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
// The w lanes are NOT touched: both stores are `vrlimi128 vD,vOld,1,0` (mask 1 == the w
// word only), preserving the console's packed mInvm / mState. On the PC those are their own
// members, so vStore3 is the faithful spelling.
// ---------------------------------------------------------------------------
void RigidBody::InertiaUpdate(Inertia* lpInertia)
{
    const rw::math::vpu::Vector3& lrI = lpInertia->GetInverseInertia();
    const V3 lR0 = vLoad3(mRi), lR1 = vLoad3(mUp), lR2 = vLoad3(mAt);
    const V3 lI0 = vMul(lR0, lrI.x), lI1 = vMul(lR1, lrI.y), lI2 = vMul(lR2, lrI.z);

    const V3 lFull = { lR0.x * lI0.x + lR1.x * lI1.x + lR2.x * lI2.x,     // W00
                       lR0.x * lI0.y + lR1.x * lI1.y + lR2.x * lI2.y,     // W01
                       lR0.x * lI0.z + lR1.x * lI1.z + lR2.x * lI2.z };   // W02
    const V3 lSplt = { lR0.z * lI0.z + lR1.z * lI1.z + lR2.z * lI2.z,     // W22
                       lR0.y * lI0.y + lR1.y * lI1.y + lR2.y * lI2.y,     // W11
                       lR0.y * lI0.z + lR1.y * lI1.z + lR2.y * lI2.z };   // W12
    vStore3(mIfull, lFull);
    vStore3(mIsplt, lSplt);

    // `lwz r9,0x5C(r11)` + `lvlx v13,r9,r8(=0x10)` + `vspltw v13,v13,0` -> mIfull.w.
    mInvm = lpInertia->GetInverseMass();
}

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
//      On the PC the w payloads (mStasis/mInertia/mId lanes) are their own members, so
//      xyz-only stores ARE the faithful copy -- same rule as every mutator above.
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
    vStore3(mRi,  vLoad3(lrTransform.xAxis));   // +0x40, w (mStasis lane) untouched
    vStore3(mUp,  vLoad3(lrTransform.yAxis));   // +0x50, w (mInertia lane) untouched
    vStore3(mAt,  vLoad3(lrTransform.zAxis));   // +0x60, w (mTag lane) untouched
    vStore3(mCom, vLoad3(lrTransform.wAxis));   // +0x10, w (mId lane) untouched

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
    Simulation* const lpSim = mStasis;                      // `lwz r10,0x4C(r3)`
    const f32 lfDt = lpSim->GetTimeStep();                  // `lfs f0,0xA0(r10)`
    const Inertia* const lpInertia = mInertia;              // `lwz r30,0x5C(r3)`

    // The per-body 64-byte reaction-force block, four Vector4s. The contact/joint/drive
    // pipelines accumulate into it; we consume it and zero it at the end of this function.
    // X360 caches the RESOLVED address in the mId slot (`lwz r11,0x1C(r3)` is used straight
    // as a base: `addi r28,r11,0x10` / `addi r29,r11,0x20` / `addi r27,r11,0x30`). A 64-bit
    // pointer does not fit there, so -- exactly as the shipping x64 build does -- the PC
    // leaf resolves the INDEX against Simulation::GetReactionForces().
    rw::math::vpu::Vector4* const lpAccum = reinterpret_cast<rw::math::vpu::Vector4*>(
        static_cast<u8*>(lpSim->GetReactionForces()) + mId * KU_REACTION_BLOCK_BYTES);

    // ---- linear: acceleration -> velocity -> position delta -------------------------
    // X360 0x82BC2BDC `vmaddfp v12,v12,v10,v13` == mForce*dt + mVel, and 0x82BC2BEC.
    const V3 lVelStep   = vAdd(vMul(vLoad3(mForce), lfDt), vLoad3(mVel));
    const V3 lPosDelta  = vAdd(vMul(lVelStep, lfDt), vLoad3(lpAccum[0]));

    // ---- angular: same shape, using mTorque / mOmega / accum[2] ----------------------
    const V3 lOmegaStep = vAdd(vMul(vLoad3(mTorque), lfDt), vLoad3(mOmega));
    const V3 lAngDelta  = vAdd(vMul(lOmegaStep, lfDt), vLoad3(lpAccum[2]));

    // ---- integrate the centre of mass ------------------------------------------------
    // accum[1] is a direct POSITION correction (added with no dt), which is what makes this
    // a position-level solver. X360 0x82BC2BFC..0x82BC2C10.
    vStore3(mCom, vAdd(vAdd(vLoad3(mCom), vLoad3(lpAccum[1])), lPosDelta));

    // ---- integrate the orientation ----------------------------------------------------
    // The spin used for the QUATERNION is lAngDelta + accum[3]; the spin stored back as the
    // angular VELOCITY below is lAngDelta ALONE. That asymmetry is deliberate: BurnoutPR
    // forms v7 = v40 + v4[3] for the quaternion and v21 from v40 for the velocity, and on
    // X360 v6 is never clobbered between 0x82BC2BF4 and 0x82BC2E30.
    const V3 lSpin = vAdd(lAngDelta, vLoad3(lpAccum[3]));

    // qdot = 0.5 * (0, lSpin) (x) q -- the standard pure-quaternion product:
    //     vector part = 0.5 * ( lSpin*q.w + lSpin X q.xyz )
    //     scalar part = -0.5 * dot(lSpin, q.xyz)
    // X360 0x82BC2C24..0x82BC2C54: the cross via the `vpermwi128` 0x63 (.yzxw) double
    // permute, the dot via `vmsum3fp128`, the 0.5 via `vcfsx v11,v9,1`.
    {
        const V3 lQv = { mQuat.x, mQuat.y, mQuat.z };
        const V3 lVecPart = vAdd(vMul(lSpin, mQuat.w), vCross(lSpin, lQv));

        mQuat.x += 0.5f * lVecPart.x;
        mQuat.y += 0.5f * lVecPart.y;
        mQuat.z += 0.5f * lVecPart.z;
        mQuat.w -= 0.5f * vDot(lSpin, lQv);
    }

    // ---- renormalise the orientation, then rebuild the basis from it -----------------------
    // X360 0x82BC2C58..0x82BC2D38: rwmath's Normalize(Quaternion) and Matrix33FromQuaternion,
    // both INLINED (the helpers above).
    // ⛔ CORRECTED 2026-09-24 (H2-D1). This used to call Quaternion::UnitQuaternionToMatrix and
    // rely on THAT function normalising mQuat in place. The console never calls it here (its
    // only callers are the two jacobian builders), and 0x82BC3EC0 neither normalises nor writes
    // back: the normalise is this block's own -- `vmulfp128 v12,v10,v12` then
    // `stvx128 v12,r0,r3` (0x82BC2C8C / 0x82BC2C90), a full 16-byte store, w (the real part)
    // included. The basis is then built from that SAME normalised register (v12), in the
    // gSqrt2s form, not in 0x82BC3EC0's 2xy form. (BurnoutPR and Xbox One do call their
    // out-of-line helper here, and theirs normalises; X360 is the target.)
    mQuat = Normalize(mQuat);
    {
        const rw::math::vpu::Matrix33 lBasis = Matrix33FromQuaternion(mQuat);
        vStore3(mRi, vLoad3(lBasis.xAxis));   // +0x40 @0x82BC2D20; w (mStasis lane) kept, vrlimi128 mask 1
        vStore3(mUp, vLoad3(lBasis.yAxis));   // +0x50 @0x82BC2D2C; w (mInertia lane) kept
        vStore3(mAt, vLoad3(lBasis.zAxis));   // +0x60 @0x82BC2D38; w (mTag lane) kept
    }

    // ---- world inertia tensor -----------------------------------------------------------
    // X360 0x82BC2D60..0x82BC2DE8. ⭐ 2026-08-04 (task #140): this block USED TO BE WRITTEN
    // OUT HERE. It is the DWARF's `RigidBody::InertiaUpdate(Inertia*)` (rigidbody.h:387),
    // which the console inlines here AND in Simulation::AddRigidBody @0x82BC35A8, so it now
    // lives in one place and both callers use it. See InertiaUpdate below.
    //
    // ⚠️ THE ONE DIFFERENCE, AND IT IS AN INLINING ARTEFACT, NOT A SEMANTIC ONE: the copy
    // inlined here does not re-load mInvm (that value is already live in a register at this
    // point in DynamicUpdate's frame), whereas AddRigidBody's copy ends with the `lwz 0x5C` +
    // `lvlx +0x10` that reads it. mInvm is `mInertia->GetInverseMass()` either way, and
    // DynamicUpdate cannot change it, so recomputing it here stores the identical value.
    InertiaUpdate(mInertia);   // `lwz r30,0x5C(r3)` -- the same block lpInertia aliases

    // ---- back-derive the velocities, with drag ----------------------------------------
    // X360 0x82BC2DCC..0x82BC2E34: 1/dt via `vrefp` + two Newton-Raphson steps; BurnoutPR
    // and Xbox One both use a true divide. Same value.
    // ⚠️ THE DRAG SLOTS ARE +0x20 (LINEAR) AND +0x24 (ANGULAR), and the clamps are +0x18
    // (linear) and +0x1C (angular). Read directly off the asm: `li r8,0x20` / `li r9,0x24`
    // feed the two `lvlx` at 0x82BC2DF4 / 0x82BC2E04, and the registers they scale are the
    // ones that end up stored into mOmega (v13) and mVel (v12) at 0x82BC2F84/0x82BC2F90.
    const f32 lfInvDt = 1.0f / lfDt;
    V3 lLinear  = vMul(lPosDelta, (1.0f - lpInertia->GetLinearDrag())  * lfInvDt);
    V3 lAngular = vMul(lAngDelta, (1.0f - lpInertia->GetAngularDrag()) * lfInvDt);

    // ---- magnitude clamps -------------------------------------------------------------
    // ANGULAR IS CLAMPED FIRST (X360 0x82BC2E38 against mMaxOmega), LINEAR SECOND
    // (0x82BC2EA4 against mMaxVelocity). Both keep the CLAMPED squared magnitude for the
    // sleep test below -- `fmr f12,f13` then `fmr f12,f0` inside the clamp arm. Dropping
    // that assignment would leave a body that never sleeps once it has been clamped.
    f32 lfAngMeasure = vDot(lAngular, lAngular);
    const f32 lfMaxAngSq = lpInertia->GetMaxAngularVelocity() * lpInertia->GetMaxAngularVelocity();
    if (lfAngMeasure > lfMaxAngSq)
    {
        lAngular = vMul(lAngular, std::sqrt(lfMaxAngSq / lfAngMeasure));
        lfAngMeasure = lfMaxAngSq;
    }

    f32 lfLinMeasure = vDot(lLinear, lLinear);
    const f32 lfMaxLinSq = lpInertia->GetMaxLinearVelocity() * lpInertia->GetMaxLinearVelocity();
    if (lfLinMeasure > lfMaxLinSq)
    {
        lLinear = vMul(lLinear, std::sqrt(lfMaxLinSq / lfLinMeasure));
        lfLinMeasure = lfMaxLinSq;
    }

    // ---- sleep / deactivation hysteresis ----------------------------------------------
    // X360 0x82BC2F18..0x82BC2F6C, transcribed branch for branch:
    //   energy = inertia->mSpherical * body->mInvm * |w|^2 + |v|^2      (twice the kinetic
    //   energy per unit mass -- which is what the DWARF calls GetKineticEnergy()).
    // ⚠️ DIVERGENCE, X360 IS AUTHORITATIVE: X360 only READS mInvm here and never writes it,
    // so the cache is seeded when the body is added (AddRigidBody @0x82BC3318, not yet
    // reconstructed). BurnoutPR AND Xbox One both refresh it from the Inertia every tick
    // (`vmovss [rbx+9Ch], xmm8`). We follow X360; if a future AddRigidBody reconstruction
    // turns out not to seed mInvm, the BurnoutPR/Xbox One refresh is the safe interim.
    const f32 lfEnergy = lpInertia->GetSphericalInertia() * mInvm * lfAngMeasure + lfLinMeasure;

    if (lfEnergy >= lpSim->GetFreezingEnergy())              // sim +0xA8
    {
        mCool = 0u;
    }
    else
    {
        if (lfEnergy <= mKine)                               // the energy did not increase
            ++mCool;
        const u32 luMax = lpSim->GetCoolDown();              // sim +0xA4
        if (mCool > luMax)
            mCool = luMax;
    }
    mKine = lfEnergy;                                        // remember for next tick

    // ---- commit the velocities ---------------------------------------------------------
    vStore3(mVel,   lLinear);
    vStore3(mOmega, lAngular);

    // ---- clear the reaction-force accumulator (all 64 bytes) ---------------------------
    // X360: four `stvx128` of a zero vector at 0x82BC2F94..0x82BC2FA0. Xbox One: two 32-byte
    // `vmovups ymmword`.
    for (int liI = 0; liI < 4; ++liI)
    {
        lpAccum[liI].x = 0.0f; lpAccum[liI].y = 0.0f;
        lpAccum[liI].z = 0.0f; lpAccum[liI].w = 0.0f;
    }

    // ---- re-seed the acceleration accumulators for the next tick -----------------------
    // mForce := the simulation's gravity (sim +0x90), mTorque := 0. Both preserve w.
    vStore3(mForce, vLoad3(lpSim->GetGravity()));
    {
        const V3 lZero = { 0.0f, 0.0f, 0.0f };
        vStore3(mTorque, lZero);
    }

    return this;   // r3 unchanged -- BatchIntegrator walks ->mRight from the RETURNED body
}

} // namespace physics
} // namespace rw

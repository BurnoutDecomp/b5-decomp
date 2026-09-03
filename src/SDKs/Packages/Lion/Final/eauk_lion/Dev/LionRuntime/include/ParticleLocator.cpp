// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleLocator.cpp
//
// cParticleLocator runtime -- the animated locator transform node for the Lion
// (eauk_lion) particle system.
//
// Reconstructed store-for-store from the X360 asm for:
//   cParticleLocator::Init    @ 0x82909810
//   cParticleLocator::GetMat  @ 0x8290E288   (landed 2026-09-03)
//   cParticleLocator::Update  @ 0x829098D0   (landed 2026-09-03)
//
// ⭐ GetMat WAS PARKED ON A COMMENT, NOT ON A FACT. This file used to say the blend masked
// each rotated basis row "through the un-exported VMX mask blob unk_820FEBD0 ... an
// un-recoverable rodata permute/mask table". tools/re/x360rd.py reads that address in one
// command: { FFFFFFFF, FFFFFFFF, FFFFFFFF, 00000000 }, the ordinary xyz-keep / w-drop
// selector, with 1.0f x4 in the quadword straight after it -- the value vsel puts into the
// translation row's w. What was left was 158 instructions of keyframe blend.
// ============================================================================

#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleLocator.h"

namespace
{
    inline void SetVec(cVector& arV, f32 aX, f32 aY, f32 aZ, f32 aW)
    {
        arV.x = aX;
        arV.y = aY;
        arV.z = aZ;
        arV.w = aW;
    }

    inline void SetQuat(cQuat& arQ, f32 aX, f32 aY, f32 aZ, f32 aW)
    {
        arQ.x = aX;
        arQ.y = aY;
        arQ.z = aZ;
        arQ.w = aW;
    }
}

// ----------------------------------------------------------------------------
// cParticleLocator::Init  @ 0x82909810
//
// Resets the locator to its default state:
//   * mMat -> identity 4x4 (asm 0x82909828..0x82909870: f13=1.0 on the diagonal,
//     f0=0.0 elsewhere; flt_82001C98=1.0, flt_82001CC0=0.0),
//   * mVel -> 0 (asm 0x82909874..0x82909880),
//   * mTime / mIndex -> 0, mFlags -> 1, mpPosEvaluator -> null
//     (asm 0x82909884..0x82909890 -- see the ParticleLocator.h banner for why the last
//     two are NOT "mKeyCount = 1; mFlags = 0"),
//   * the two keyframes -> identity rotation (0,0,0,1) + zero translation, and both
//     keyframe timestamps -> 0 (the do-loop @0x82909894, 2 iterations).
// The two-iteration loop is re-rolled here over the keyframe arrays.
// ----------------------------------------------------------------------------
void cParticleLocator::Init()
{
    SetVec(mMat.xa, 1.0f, 0.0f, 0.0f, 0.0f);
    SetVec(mMat.ya, 0.0f, 1.0f, 0.0f, 0.0f);
    SetVec(mMat.za, 0.0f, 0.0f, 1.0f, 0.0f);
    SetVec(mMat.wa, 0.0f, 0.0f, 0.0f, 1.0f);
    SetVec(mVel,    0.0f, 0.0f, 0.0f, 0.0f);

    mTime.BuildZero();
    mIndex = 0;
    mFlags = 1;
    mpPosEvaluator = nullptr;

    for (u32 luKey = 0; luKey < 2; ++luKey)
    {
        SetQuat(mCacheQuat[luKey], 0.0f, 0.0f, 0.0f, 1.0f);
        SetVec(mCachePos[luKey], 0.0f, 0.0f, 0.0f, 0.0f);
        mCacheTime[luKey].BuildZero();
    }
}

// ----------------------------------------------------------------------------
// cParticleLocator::GetMat  @ 0x8290E288        (DWARF ParticleLocator.h:54)
//
// Sample the locator's animated frame at arKeyTime and hand back the cached matrix.
//
// THE TWO KEYFRAMES ARE A PING-PONG PAIR: mIndex names the FROM key and mIndex ^ 1
// (asm `xori r30, r11, 1` @0x8290E2B8) the TO key. Their addressing is what pinned this
// record's layout -- `16*(index+5) + this` for the rotation quaternion (-> mCacheQuat at
// +0x50) and `16*(index+7) + this` for the translation (-> mCachePos at +0x70).
//
// ⭐ THE `divw.` GUARD IS cTime::GetTimeMilliSeconds, NOT A MAGIC 3. Asm 0x8290E308 does
// `li r9, 3 ; divw. r9, r11, r9 ; beq <snap>` on the FROM key's tick count. The DWARF
// declares `const S32 msuTicksPerMilliSecond = 3` (cTime.h:115) and `S32
// GetTimeMilliSeconds() const` (cTime.h:39), and the tick rate is corroborated by the
// image float this very function multiplies by (flt_82F369A8 == 0x39AEC33E == 1/3000).
// So the guard reads "the from-key is at a whole millisecond past zero" -- i.e. the pair
// has actually been keyed -- and an un-keyed locator takes the snap path.
//
// ⭐⭐ THE TWO `fsel`s ARE A CLAMP, AND THE OPERAND ORDER WAS PROVED, NOT ASSUMED. Read in
// the raw ENCODING field order (D,A,B,C) that this project's notes flag for the VMX
// four-operand forms, `fsel f0, f13, f31, f0` would mean `(t-1 >= 0) ? t : 1.0` and
// `fsel f30, f0, f0, f12` would mean `(t >= 0) ? 0.0 : t` -- both nonsense. Decoding the
// raw words settles it: 0x8290E340 == FC0D07EE has frD=0, frA=13, frB=0, frC=31 and
// 0x8290E344 == FFC0602E has frD=30, frA=0, frB=12, frC=0, i.e. IDA is printing the
// ASSEMBLER order D,A,C,B here, and `frD = (frA >= 0) ? frC : frB` gives
// `t = min(t, 1)` then `t = max(t, 0)`. fsel is NOT one of the raw-field-order mnemonics.
//
// ⚠ ONE HOST DIVERGENCE, STATED: the console forms 1/lfSpan with `vrefp` + two
// Newton-Raphson steps (it is a `fdivs` here, so actually a true divide -- but the
// reciprocal pattern IS what PrecalculateParticleBuildData does for the frame count).
// This line is a real `fdivs`, so the host `/` is exact parity.
// ----------------------------------------------------------------------------
const cMatrix& cParticleLocator::GetMat(const cTime& arKeyTime) const
{
    // asm 0x8290E2A4..0x8290E2B0 -- the same time as last call: the cache stands.
    if (arKeyTime.GetTicks() == mTime.GetTicks())
    {
        return mMat;
    }

    const u32 luFrom = mIndex;
    const u32 luTo   = mIndex ^ 1u;

    const s32 lFromTicks = mCacheTime[luFrom].GetTicks();
    const s32 lToTicks   = mCacheTime[luTo].GetTicks();

    // asm 0x8290E2D4..0x8290E2FC -- the key interval, in seconds.
    const f32 lfSpan = static_cast<f32>(lToTicks - lFromTicks) * msfOneOverTicksPerSecond;

    if (lfSpan > 0.0f && mCacheTime[luFrom].GetTimeMilliSeconds() != 0)
    {
        // asm 0x8290E314..0x8290E344 -- normalised, clamped blend weight.
        const f32 lfElapsed =
            static_cast<f32>(arKeyTime.GetTicks() - lFromTicks) * msfOneOverTicksPerSecond;

        f32 lfWeight = lfElapsed / lfSpan;
        if ((lfWeight - 1.0f) >= 0.0f)      // fsel @0x8290E340
        {
            lfWeight = 1.0f;
        }
        if (lfWeight < 0.0f)                // fsel @0x8290E344
        {
            lfWeight = 0.0f;
        }
        const f32 lfInvWeight = 1.0f - lfWeight;   // fsubs @0x8290E390

        // asm 0x8290E34C..0x8290E374 -- each keyframe quaternion becomes a rotation matrix
        // in a stack temporary (var_C0 = FROM, var_80 = TO).
        cMatrix lFromMat;
        cMatrix lToMat;
        mCacheQuat[luFrom].ToMatrix(lFromMat);
        mCacheQuat[luTo].ToMatrix(lToMat);

        // asm 0x8290E3F4..0x8290E454 -- the translation row of each temporary is then
        // overwritten with that key's position, w forced to 1.0.
        lFromMat.wa.x = mCachePos[luFrom].x;
        lFromMat.wa.y = mCachePos[luFrom].y;
        lFromMat.wa.z = mCachePos[luFrom].z;
        lFromMat.wa.w = 1.0f;

        lToMat.wa.x = mCachePos[luTo].x;
        lToMat.wa.y = mCachePos[luTo].y;
        lToMat.wa.z = mCachePos[luTo].z;
        lToMat.wa.w = 1.0f;

        // asm 0x8290E440..0x8290E498 -- three `vmaddfp` row lerps whose w lane is then
        // ANDed off through unk_820FEBD0 { ~0, ~0, ~0, 0 }, and a fourth whose w lane is
        // vsel'd back to the 1.0f quadword at unk_820FEBD0+0x10.
        //
        // ⭐ `vmaddfp vD, vA, vB, vC` PRINTS IN RAW FIELD ORDER, so it is vD = vA*vC + vB:
        // `vmaddfp v0, v9, v11, v13` with v9 == FROM row 0, v11 == TO row 0 * weight and
        // v13 == splat(1 - weight) is FROM*(1-w) + TO*w, the ordinary lerp.
        mMat.xa.x = lFromMat.xa.x * lfInvWeight + lToMat.xa.x * lfWeight;
        mMat.xa.y = lFromMat.xa.y * lfInvWeight + lToMat.xa.y * lfWeight;
        mMat.xa.z = lFromMat.xa.z * lfInvWeight + lToMat.xa.z * lfWeight;
        mMat.xa.w = 0.0f;

        mMat.ya.x = lFromMat.ya.x * lfInvWeight + lToMat.ya.x * lfWeight;
        mMat.ya.y = lFromMat.ya.y * lfInvWeight + lToMat.ya.y * lfWeight;
        mMat.ya.z = lFromMat.ya.z * lfInvWeight + lToMat.ya.z * lfWeight;
        mMat.ya.w = 0.0f;

        mMat.za.x = lFromMat.za.x * lfInvWeight + lToMat.za.x * lfWeight;
        mMat.za.y = lFromMat.za.y * lfInvWeight + lToMat.za.y * lfWeight;
        mMat.za.z = lFromMat.za.z * lfInvWeight + lToMat.za.z * lfWeight;
        mMat.za.w = 0.0f;

        mMat.wa.x = lFromMat.wa.x * lfInvWeight + lToMat.wa.x * lfWeight;
        mMat.wa.y = lFromMat.wa.y * lfInvWeight + lToMat.wa.y * lfWeight;
        mMat.wa.z = lFromMat.wa.z * lfInvWeight + lToMat.wa.z * lfWeight;
        mMat.wa.w = 1.0f;
    }
    else
    {
        // asm 0x8290E4A0..0x8290E4E0 -- no usable interval: SNAP to the TO key. The console
        // passes `mr r4, r31`, i.e. ToMatrix's destination IS the locator's own mMat.
        mCacheQuat[luTo].ToMatrix(mMat);

        mMat.wa.x = mCachePos[luTo].x;
        mMat.wa.y = mCachePos[luTo].y;
        mMat.wa.z = mCachePos[luTo].z;
        mMat.wa.w = 1.0f;
    }

    // asm 0x8290E4E4..0x8290E4E8 -- both paths stamp the cache and hand back mMat (r3 is
    // still `this`, and mMat is at +0x00, which is why the console returns the locator).
    mTime = arKeyTime;
    return mMat;
}

// ----------------------------------------------------------------------------
// cParticleLocator::Update  @ 0x829098D0        (DWARF ParticleLocator.h:41)
//
// Push one new keyframe -- arMat's translation and rotation stamped at arTime -- into the
// ping-pong pair, flip mIndex to point at the OTHER slot, and derive mVel from the two
// stamps that now bracket the present.
//
// ⭐ THE INDEX FLIP IS WHY GetMat READS THE WAY IT DOES. Update writes slot mIndex and then
// sets mIndex ^= 1 (asm 0x8290992C / 0x82909938), so after a call mIndex names the OLDER
// key and mIndex^1 the one just written -- which is exactly the FROM / TO pairing GetMat
// interpolates between.
//
// ⭐ THE VELOCITY IS A BACKWARD DIFFERENCE, and the register trace has to be followed to see
// it: 0x829099AC..0x829099D0 build r10 = &mCachePos[newIndex] (the older key) and
// r9 = &mCachePos[oldIndex] (the one just written), and each `fsubs` is
// (just-written - older) / (nowTicks - olderTicks). Only x/y/z are scaled; mVel.w is left
// alone by the velocity path (it is only ever cleared on the reset path below).
//
// mFLAGS: the asm masks two bits, and what each does is read off the branch it guards.
//   bit 1 (0x2, `rlwinm r11,0,30,30` @0x82909930) -- when set, Update stops after storing
//     the key: no velocity is derived and mVel is not even cleared. An externally driven
//     locator.
//   bit 0 (0x1, `clrlwi r11,31` @0x82909940) -- when set, Update ZEROES mVel and clears the
//     bit (`clrrwi r11,r11,1` @0x82909A40). cParticleLocator::Init sets mFlags = 1, so the
//     first Update after a reset publishes no velocity rather than a velocity measured
//     across a discontinuity. The DWARF's Teleport(const cMatrix&, const cTime&) is the
//     obvious other setter; it is not reconstructed, so that is stated as inference, not
//     asserted.
//
// ⚠ THE SAME `divw.` GUARD AS GetMat, TWICE: 0x8290995C and 0x82909970 test both stamps'
// GetTimeMilliSeconds() and take the reset path if either is zero -- i.e. a pair that has
// not been keyed twice yet cannot produce a velocity.
// ----------------------------------------------------------------------------
void cParticleLocator::Update(const cMatrix& arMat, const cTime& arTime)
{
    const u32 luWritten = mIndex;
    const u32 luOther   = mIndex ^ 1u;

    // asm 0x829098E8..0x82909920 -- the new key: translation row, time stamp, and the
    // rotation distilled out of arMat's 3x3.
    mCachePos[luWritten]  = arMat.wa;
    mCacheTime[luWritten] = arTime;
    mCacheQuat[luWritten].FromMatrix(arMat);

    // asm 0x82909938 -- the flip, which happens before any of the tests below.
    mIndex = luOther;

    if ((mFlags & E_FLAG_EXTERNAL_VELOCITY) != 0)
    {
        return;
    }

    if ((mFlags & E_FLAG_VELOCITY_INVALID) == 0 &&
        mCacheTime[luWritten].GetTimeMilliSeconds() != 0 &&
        mCacheTime[luOther].GetTimeMilliSeconds()   != 0)
    {
        const f32 lfDeltaTime =
            static_cast<f32>(mCacheTime[luWritten].GetTicks() - mCacheTime[luOther].GetTicks())
            * msfOneOverTicksPerSecond;

        // asm 0x829099A4 -- a non-positive interval publishes nothing at all, and notably
        // does NOT fall through to the reset path.
        if (lfDeltaTime <= 0.0f)
        {
            return;
        }

        const f32 lfInvDeltaTime = 1.0f / lfDeltaTime;

        mVel.x = (mCachePos[luWritten].x - mCachePos[luOther].x) * lfInvDeltaTime;
        mVel.y = (mCachePos[luWritten].y - mCachePos[luOther].y) * lfInvDeltaTime;
        mVel.z = (mCachePos[luWritten].z - mCachePos[luOther].z) * lfInvDeltaTime;
        return;
    }

    // asm 0x82909A3C -- the reset path: drop the invalid-velocity bit and publish zero.
    mFlags &= ~static_cast<u32>(E_FLAG_VELOCITY_INVALID);
    mVel.x = 0.0f;
    mVel.y = 0.0f;
    mVel.z = 0.0f;
    mVel.w = 0.0f;
}

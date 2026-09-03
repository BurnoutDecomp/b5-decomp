// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleLocator.cpp
//
// cParticleLocator runtime -- the animated locator transform node for the Lion
// (eauk_lion) particle system.
//
// Reconstructed store-for-store from the X360 asm for:
//   cParticleLocator::Init   @ 0x82909810
//
// cParticleLocator::GetMat @ 0x8290E288 is declared in the header but not bodied here. THE
// REASON THIS FILE USED TO GIVE WAS WRONG and is corrected: it said the blend masks each
// rotated basis row "through the un-exported VMX mask blob unk_820FEBD0 ... an un-recoverable
// rodata permute/mask table". tools/re/x360rd.py reads that address straight out of the image
// as { FFFFFFFF, FFFFFFFF, FFFFFFFF, 00000000 } -- the ordinary xyz-keep / w-drop vsel
// selector, with 1.0f x4 in the quadword after it. What is left is 158 instructions of
// quaternion-to-matrix keyframe blend, and nothing about it is unrecoverable.
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
}

// ----------------------------------------------------------------------------
// cParticleLocator::Init  @ 0x82909810
//
// Resets the locator to its default state:
//   * mMatrix -> identity 4x4 (asm 0x82909828..0x82909880: f13=1.0 on the diagonal,
//     f0=0.0 elsewhere; flt_82001C98=1.0, flt_82001CC0=0.0),
//   * mReserved40 -> 0,
//   * the two keyframes -> identity rotation (0,0,0,1) + zero translation, and both
//     keyframe timestamps -> 0 (the do-loop @0x82909894, 2 iterations),
//   * mCachedTime / mKeyIndex -> 0, mKeyCount -> 1, mFlags -> 0.
// The two-iteration loop is re-rolled here over the keyframe arrays.
// ----------------------------------------------------------------------------
void cParticleLocator::Init()
{
    SetVec(mMatrix.xa, 1.0f, 0.0f, 0.0f, 0.0f);
    SetVec(mMatrix.ya, 0.0f, 1.0f, 0.0f, 0.0f);
    SetVec(mMatrix.za, 0.0f, 0.0f, 1.0f, 0.0f);
    SetVec(mMatrix.wa, 0.0f, 0.0f, 0.0f, 1.0f);
    SetVec(mReserved40, 0.0f, 0.0f, 0.0f, 0.0f);

    mCachedTime = 0;
    mKeyIndex = 0;
    mKeyCount = 1;
    mFlags = 0;

    for (u32 luKey = 0; luKey < 2; ++luKey)
    {
        SetVec(maKeyRotation[luKey], 0.0f, 0.0f, 0.0f, 1.0f);
        SetVec(maKeyTranslation[luKey], 0.0f, 0.0f, 0.0f, 0.0f);
        maKeyTime[luKey] = 0;
    }
}

#include "types.hpp"
#include "GameSource/Gui/PFX/BrnPfxHookBlender.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include <cstring>                                    // memcpy

// =============================================================================
// BrnPfxHookBlender.cpp
//
// The post-FX weighted blend accumulators' Add() bodies (X360 ARTIST). Each Add
// folds one source payload into the running accumulator:
//   - first Add (mfCount becomes 1.0f) copies the source straight in;
//   - every later Add blends src*lfWeight + accum*(1 - lfWeight) per member/lane.
// The X360 broadcasts each weight (vspltw) and runs a vmaddfp chain over the
// 16-byte vector members; the scalar members are folded in fp registers. Each
// vector lane (including the unused z/w lanes the 16-byte stores still touch) is
// blended so the result is byte-identical. Reconstructed by member name.
//
// The "lfWeight <= 1.0f" assert (file "...Gui/PFX/BrnPfxHookBlender.cpp" line 84)
// only fires when a newly-seen larger weight exceeds 1.0f; modelled with
// CGS_ASSERT inside the mfWeight-update branch to match the X360 control flow.
// =============================================================================

namespace BrnGui
{

// helper: lane-wise 2-way blend of a 16-byte vector member (Vector2 or Vector4)
template <typename V>
static inline void Blend2Lanes(V& lrOut, const V& lSrc, f32 lfWeight, f32 lfInvWeight)
{
    lrOut.x = lSrc.x * lfWeight + lrOut.x * lfInvWeight;
    lrOut.y = lSrc.y * lfWeight + lrOut.y * lfInvWeight;
    lrOut.z = lSrc.z * lfWeight + lrOut.z * lfInvWeight;
    lrOut.w = lSrc.w * lfWeight + lrOut.w * lfInvWeight;
}

// ---- BloomBlend::Add @ 0x824F6810 ----------------------------------------
BloomBlend* BloomBlend::Add(const BrnEffects::BloomData* lpSrc, f32 lfWeight)
{
    if (lpSrc == nullptr)
    {
        return this;
    }

    mfCount = mfCount + 1.0f;
    if (mfWeight < lfWeight)
    {
        CGS_ASSERT(lfWeight <= 1.0f, "lfWeight <= 1.0f");
        mfWeight = lfWeight;
    }

    if (mfCount == 1.0f)
    {
        mData.mfLuminance = lpSrc->mfLuminance;
        mData.mfThreshold = lpSrc->mfThreshold;
        mData.mv4Scale    = lpSrc->mv4Scale;
    }
    else
    {
        const f32 lfInvWeight = 1.0f - lfWeight;
        mData.mfLuminance = lpSrc->mfLuminance * lfWeight + mData.mfLuminance * lfInvWeight;
        mData.mfThreshold = lpSrc->mfThreshold * lfWeight + mData.mfThreshold * lfInvWeight;
        Blend2Lanes(mData.mv4Scale, lpSrc->mv4Scale, lfWeight, lfInvWeight);
    }

    return this;
}

// ---- DepthOfFieldBlend::Add @ 0x824F6C40 ---------------------------------
// The 1-weight copy moves five dwords (mfNearPlane..mfDofAmount); the blend
// path folds only the first four lanes (mfDofAmount is left as copied).
DepthOfFieldBlend* DepthOfFieldBlend::Add(const DepthOfFieldBlend* lpSrc, f32 lfWeight)
{
    if (lpSrc == nullptr)
    {
        return this;
    }

    mfCount = mfCount + 1.0f;
    if (mfWeight < lfWeight)
    {
        CGS_ASSERT(lfWeight <= 1.0f, "lfWeight <= 1.0f");
        mfWeight = lfWeight;
    }

    if (mfCount == 1.0f)
    {
        mfNearPlane   = lpSrc->mfNearPlane;
        mfFocalPlane  = lpSrc->mfFocalPlane;
        mfFocalPlane2 = lpSrc->mfFocalPlane2;
        mfFarPlane    = lpSrc->mfFarPlane;
        mfDofAmount   = lpSrc->mfDofAmount;
    }
    else
    {
        const f32 lfInvWeight = 1.0f - lfWeight;
        mfNearPlane   = lpSrc->mfNearPlane   * lfWeight + mfNearPlane   * lfInvWeight;
        mfFocalPlane  = lpSrc->mfFocalPlane  * lfWeight + mfFocalPlane  * lfInvWeight;
        mfFocalPlane2 = lpSrc->mfFocalPlane2 * lfWeight + mfFocalPlane2 * lfInvWeight;
        mfFarPlane    = lpSrc->mfFarPlane    * lfWeight + mfFarPlane    * lfInvWeight;
    }

    return this;
}

// ---- TintData2dBlend::Add @ 0x824F6D60 -----------------------------------
TintData2dBlend* TintData2dBlend::Add(const BrnEffects::TintData2d* lpSrc, f32 lfWeight)
{
    if (lpSrc == nullptr)
    {
        return this;
    }

    mfCount = mfCount + 1.0f;
    if (mfWeight < lfWeight)
    {
        CGS_ASSERT(lfWeight <= 1.0f, "lfWeight <= 1.0f");
        mfWeight = lfWeight;
    }

    if (mfCount == 1.0f)
    {
        mData.mv4Colour = lpSrc->mv4Colour;
    }
    else
    {
        const f32 lfInvWeight = 1.0f - lfWeight;
        Blend2Lanes(mData.mv4Colour, lpSrc->mv4Colour, lfWeight, lfInvWeight);
    }

    return this;
}

// ---- BlurBlend::Add @ 0x824F6B70 -----------------------------------------
// Blend path delegates to BrnEffects::BlurData::SetToBlend(dst, dst, lfWeight,
// src, 1-lfWeight) [arg order matches the X360 call: result, _R4=dst, a3=lfWeight,
// a4=1-lfWeight via SetToBlend's (lpResult, lpA, lfWa, lfWb, lpB)]. The X360
// passes f1=1-w, f2=w with r4=dst (lpA) and r6=src (lpB), so lpA weight is
// (1-w) and lpB weight is w.
BlurBlend* BlurBlend::Add(const BrnEffects::BlurData* lpSrc, f32 lfWeight)
{
    if (lpSrc == nullptr)
    {
        return this;
    }

    mfCount = mfCount + 1.0f;
    if (mfWeight < lfWeight)
    {
        CGS_ASSERT(lfWeight <= 1.0f, "lfWeight <= 1.0f");
        mfWeight = lfWeight;
    }

    if (mfCount == 1.0f)
    {
        memcpy(&mData, lpSrc, sizeof(BrnEffects::BlurData));
    }
    else
    {
        // SetToBlend(result=&mData, lpA=&mData, lfWa=1-w, lfWb=w, lpB=src)
        BrnEffects::BlurData::SetToBlend(&mData, &mData, 1.0f - lfWeight, lfWeight, lpSrc);
    }

    return this;
}

} // namespace BrnGui

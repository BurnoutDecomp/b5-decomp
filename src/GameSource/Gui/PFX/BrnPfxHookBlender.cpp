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

// ---- BlurBlend::Construct @ 0x824F6AD0 -----------------------------------
// Seeds a fresh accumulator. The X360 body stores, store-for-store:
//   mfOpacity@0x00, mfVelocity@0x04, mfSharpness@0x08, mfNoise@0x0C,
//   mfAngle@0x10  -- the five BlurData scalar defaults (kfDefOpacity..kfDefAngle);
//   mv2BlendAmount@0x20, mv2BlurAmount@0x30, mv2BlendCentre@0x40,
//   mv2BlurCentre@0x50 -- the four 16-byte Vector2 defaults (kv2DefBlendAmount
//   .. kv2DefBlurCentre); and finally 0.0f to mfWeight@0x60 and mfCount@0x64.
// The 0..0x50 store set is exactly BrnEffects::BlurData::Construct's; delegate
// to it so the payload defaults stay homed in one place, then zero the two
// trailing bookkeeping fields. Returns this (the X360 body returns its result).
BlurBlend* BlurBlend::Construct()
{
    mData.Construct();   // 5 scalar defaults @0..0x10 + 4 Vector2 defaults @0x20..0x50
    mfWeight = 0.0f;     // +0x60
    mfCount  = 0.0f;     // +0x64
    return this;
}

// ---- BlurBlend::Add @ 0x824F6B70 -----------------------------------------
// Blend path delegates to BrnEffects::BlurData::SetToBlend. The X360 call passes
// r3=this(dst), r4=dst, f1=1-w, r6=src, f2=w -- which is the DWARF's non-static
// `void SetToBlend(const BlurData& lA, f32 lfWa, const BlurData& lB, f32 lfWb)`
// (BrnEffectsData.h:260), NOT the bunched-weight static this file assumed until
// 2026-08-15. r6 (not r7) holding lpB is the tell: a float argument skips its GPR
// slot, so an interleaved list lands the second source in r6. See the SetToBlend
// banner in SharedClasses/Graphics/BrnEffectsData.h for the full evidence.
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
        // mData.SetToBlend(lA=mData, lfWa=1-w, lB=*src, lfWb=w)
        mData.SetToBlend(mData, 1.0f - lfWeight, *lpSrc, lfWeight);
    }

    return this;
}

// ---- VignetteBlend::Construct @ 0x824F6968 -------------------------------
// Seeds a fresh accumulator. The X360 body stores mData.mfAngle@0x00 (0.0f) +
// mData.mfSharpness@0x04 (0.33f) then the four 16-byte vector members
// (mv2Amount@0x10, mv2Centre@0x20, mv4InnerColour@0x30, mv4OuterColour@0x40) --
// exactly BrnEffects::VignetteData::Construct's store set -- and finally 0.0f to
// mfWeight@0x50 and mfCount@0x54. Delegate to keep the payload defaults homed in
// one place, then zero the two trailing bookkeeping fields. Returns this.
VignetteBlend* VignetteBlend::Construct()
{
    mData.Construct();   // 2 scalar defaults @0x00/0x04 + 4 vector defaults @0x10..0x40
    mfWeight = 0.0f;     // +0x50
    mfCount  = 0.0f;     // +0x54
    return this;
}

// ---- VignetteBlend::Add @ 0x824F69E8 -------------------------------------
// Copy path (mfCount becomes 1.0f) is an 80-byte payload copy; the blend path
// delegates to BrnEffects::VignetteData::SetToBlend [X360 call: r3=this=&mData,
// r4=lA=&mData, f1=lfWa=1-w, r6=lB=src, f2=lfWb=w -- the DWARF's non-static
// `void SetToBlend(const VignetteData&, f32, const VignetteData&, f32)`
// (BrnEffectsData.h:161)].
VignetteBlend* VignetteBlend::Add(const BrnEffects::VignetteData* lpSrc, f32 lfWeight)
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
        memcpy(&mData, lpSrc, sizeof(BrnEffects::VignetteData));
    }
    else
    {
        // mData.SetToBlend(lA=mData, lfWa=1-w, lB=*src, lfWb=w)
        mData.SetToBlend(mData, 1.0f - lfWeight, *lpSrc, lfWeight);
    }

    return this;
}

} // namespace BrnGui

#include "types.hpp"
#include "GameSource/Gui/PFX/BrnPfxHookBlender.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT_AT
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysCollectionKey.h"  // Attrib::StringToKey
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"    // CgsResource::NULLResourcePtr
#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxcolourcube.h"  // rw::graphics::postfx::ColourCube (complete)
#include "rw/math/fpu/scalar_operation.h"                                 // rw::math::fpu::Min<float>

#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Gui/PFX/BrnPfxHookBlender.cpp"

// =============================================================================
// BrnPfxHookBlender.cpp -- the post-FX blend containers, BrnGui::PFXNodeFader and
// BrnGui::PFXHookNodeBlender, in the original file's order (DWARF lines: containers
// cpp:57..212, fader cpp:230..456, blender cpp:503..645).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   PfxContainer<BloomData>::Add           @0x824F6810
//   PfxContainer<VignetteData>::Construct  @0x824F6968, ::Add @0x824F69E8
//   PfxContainer<BlurData>::Construct      @0x824F6AD0, ::Add @0x824F6B70
//   PfxContainer<DepthOfFieldData>::Add    @0x824F6C40
//   PfxContainer<TintData2d>::Add          @0x824F6D60
//   PFXNodeFader::Initialise        @0x82504378
//   PFXNodeFader::Update            @0x824F5C40
//   PFXNodeFader::StartFadeOut      @0x824F5F90
//   PFXNodeFader::LookupColourCube  @0x825042A0
//   PFXHookNodeBlender::Initialise  @0x82504510
//   PFXHookNodeBlender::Update      @0x824FB598
//   PFXHookNodeBlender::StartFadeOut@0x824F6058
//   PFXHookNodeBlender::IsActive    @0x824ECFC0
//
// Every fsel in the console bodies is spelled as the compare it encodes:
// fsel(a, b, c) == (a >= 0) ? b : c.
// =============================================================================

namespace BrnGui
{

    // ====================================================================================
    // PfxContainer
    // ====================================================================================

    template <typename T>
    void PfxContainer<T>::Construct()
    {
        mData.Construct();
        mfWeight = 0.0f;
        mfCount  = 0.0f;
    }

    // BrnPfxHookBlender.cpp:57 (inlined at every ARTIST site)
    void PfxContainer<BrnEffects::TintData>::Construct()
    {
        mData.Construct();
        mfWeight = 0.0f;
        mfCount  = 0.0f;
    }

    // ------------------------------------------------------------------------------------
    // The first payload is copied in; each later one is blended in at its weight against the
    // running mix at the complement (`SetToBlend(mData, 1 - w, *src, w)`, ARTIST 0x824F6A98:
    // r3 = r4 = this). The weight tracks the largest seen; its assert is cpp:84.
    // ------------------------------------------------------------------------------------
    template <typename T>
    void PfxContainer<T>::Add(const T* lpEffectsData, float lfWeight)
    {
        if (lpEffectsData)
        {
            mfCount += 1.0f;
            if (mfWeight < lfWeight)
            {
                CGS_ASSERT_AT(lfWeight <= 1.0f, "lfWeight <= 1.0f", CGS_ARTIST_FILE, 84);
                mfWeight = lfWeight;
            }

            if (mfCount == 1.0f)
            {
                mData = *lpEffectsData;
            }
            else
            {
                mData.SetToBlend(mData, 1.0f - lfWeight, *lpEffectsData, lfWeight);
            }
        }
    }

    // BrnPfxHookBlender.cpp:114 -- the last enabled node's cube wins outright. ARTIST inlines
    // it (PFXHookNodeBlender::Update 0x824FB95C) and keeps no copy.
    void PfxContainer<BrnEffects::TintData>::Add(const BrnEffects::TintData* lpEffectsData, f32 lfWeight)
    {
        if (lpEffectsData != NULL)
        {
            mfCount  = 1.0f;
            mData    = *lpEffectsData;
            mfWeight = lfWeight;
        }
    }

    template <typename T>
    f32 PfxContainer<T>::GetCount() const
    {
        return mfCount;
    }

    // BrnPfxHookBlender.cpp:150
    f32 PfxContainer<BrnEffects::TintData>::GetCount() const
    {
        return mfCount;
    }

    template <typename T>
    f32 PfxContainer<T>::GetWeight() const
    {
        return mfWeight;
    }

    // BrnPfxHookBlender.cpp:181
    f32 PfxContainer<BrnEffects::TintData>::GetWeight() const
    {
        return mfWeight;
    }

    template <typename T>
    const T& PfxContainer<T>::GetData() const
    {
        return mData;
    }

    // BrnPfxHookBlender.cpp:212
    const BrnEffects::TintData& PfxContainer<BrnEffects::TintData>::GetData() const
    {
        return mData;
    }

    // ====================================================================================
    // PFXNodeFader
    // ====================================================================================


    // ------------------------------------------------------------------------------------
    // PFXNodeFader::LookupColourCube @0x825042A0 (BrnPfxHookBlender.cpp:230; DWARF local
    // luIndex cpp:232). The first cube whose resource id matches; it must be loaded.
    // ------------------------------------------------------------------------------------
    rw::graphics::postfx::ColourCube* PFXNodeFader::LookupColourCube(ColourCubeInfo* lpaColourCubes,
                                                                      uint32_t luCubeCount,
                                                                      uint64_t luResourceId)
    {
        for (uint32_t luIndex = 0; luIndex < luCubeCount; ++luIndex)
        {
            if (lpaColourCubes[luIndex].muResourceId == luResourceId)
            {
                CGS_ASSERT_AT(lpaColourCubes[luIndex].mpColourCube != CgsResource::NULLResourcePtr,
                              "lpaColourCubes[luIndex].mpColourCube != CgsResource::NULLResourcePtr",
                              CGS_ARTIST_FILE, 238);
                return lpaColourCubes[luIndex].mpColourCube.GetMemoryResource();
            }
        }
        return NULL;
    }

    // ------------------------------------------------------------------------------------
    // PFXNodeFader::Initialise @0x82504378 (BrnPfxHookBlender.cpp:259; DWARF local lpGroup
    // cpp:263). The group comes from the node (`lwz r30, 4(r29)`).
    // ------------------------------------------------------------------------------------
    void PFXNodeFader::Initialise(ColourCubeInfo* lpaColourCubes, uint32_t luCubeCount, PFXHookNode* lpPfxNode)
    {
        CGS_ASSERT_AT(lpPfxNode, "lpPfxNode", CGS_ARTIST_FILE, 261);
        PFXGroup* lpGroup = lpPfxNode->mpGroup;
        CGS_ASSERT_AT(lpGroup, "PfxNode has no PfxGroup", CGS_ARTIST_FILE, 265);

        mpPfxNode           = lpPfxNode;
        mfWeight            = 0.0f;
        meState             = E_WAITING;
        mfForcedFadeoutTime = 0.0f;
        mfExpiredTime       = 0.0f;

        // Each enabled effect is built from its AttribSys asset, keyed by the group's
        // collection name (Attrib::StringToKey on the 32-char id). The console's order:
        // bloom, vignette, blur, depth of field, 2D tint, then the 3D tint lookup.
        if (lpGroup->GetUseBloom())
        {
            mBloomData.Construct(Attrib::StringToKey(lpGroup->macBloomDataID));
        }
        if (lpGroup->GetUseVignette())
        {
            mVignetteData.Construct(Attrib::StringToKey(lpGroup->macVignetteDataID));
        }
        if (lpGroup->GetUseBlur())
        {
            mBlurData.Construct(Attrib::StringToKey(lpGroup->macBlurDataID));
        }
        if (lpGroup->GetUseDepthOfField())
        {
            mDepthOfFieldData.Construct(Attrib::StringToKey(lpGroup->macDepthOfFieldDataID));
        }
        if (lpGroup->GetUseTint2D())
        {
            mTint2DData.Construct(Attrib::StringToKey(lpGroup->macTint2DDataID));
        }
        if (lpGroup->GetUseTint3D())
        {
            mTint3DData.Construct();
            mTint3DData.mpColourCube = LookupColourCube(lpaColourCubes, luCubeCount, lpGroup->muTint3DResourceId);
            CGS_ASSERT_AT(mTint3DData.mpColourCube != NULL, "mTint3DData.mpColourCube != NULL", CGS_ARTIST_FILE, 297);
        }
    }

    // ------------------------------------------------------------------------------------
    // PFXNodeFader::Update @0x824F5C40 (BrnPfxHookBlender.cpp:311; DWARF locals lfStartTime,
    // lfFadeIn, lfDuration, lfFadeOut cpp:316..319, the PFXGroup Get* accessors).
    //
    // Two regimes. While no fade-out has been forced (mfForcedFadeoutTime within 0.01 of
    // zero) the node is placed by ABSOLUTE hook time against its start time and the group's
    // fade-in / duration / fade-out envelope -- so a hook that starts late still lands on
    // the right phase. Once a fade-out has been forced the node advances by TIMESTEP through
    // the phase state machine instead, with the forced time standing in for the group's
    // fade-out (when it is positive).
    // ------------------------------------------------------------------------------------
    void PFXNodeFader::Update(f32 lfTime, f32 lfTimeStep)
    {
        CGS_ASSERT_AT(mpPfxNode, "mpPfxNode", CGS_ARTIST_FILE, 313);
        CGS_ASSERT_AT(mpPfxNode->mpGroup, "PfxNode has no PfxGroup", CGS_ARTIST_FILE, 314);

        // The forced fade-out is tested against the double 0.01 (ARTIST 0x824F5CD4 `lfd`
        // 0x82072738); the near-zero tests below use the float pair 0.01f / -0.01f
        // (0x82002138 / 0x8201FDB8) as strict compares (`bge` / `ble`).
        f32 lfStartTime = mpPfxNode->mfStartTime;
        f32 lfFadeIn    = mpPfxNode->mpGroup->GetFadeIn();
        f32 lfDuration  = mpPfxNode->mpGroup->GetDuration();
        f32 lfFadeOut   = (mfForcedFadeoutTime <= 0.01) ? mpPfxNode->mpGroup->GetFadeOut() : mfForcedFadeoutTime;

        if (mfForcedFadeoutTime < 0.01f && mfForcedFadeoutTime > -0.01f)
        {
            // No forced fade-out: place the node by the hook's absolute time.
            if (lfTime < lfStartTime)
            {
                mfExpiredTime = lfTime;
                mfWeight      = 0.0f;
                meState       = E_WAITING;
            }
            else if (lfTime < lfFadeIn + lfStartTime)
            {
                mfExpiredTime = lfTime - lfStartTime;
                meState       = E_FADEIN;
                mfWeight      = mfExpiredTime / lfFadeIn;
            }
            else if ((lfDuration < 0.01f && lfDuration > -0.01f) || lfTime < (lfDuration + lfFadeIn) + lfStartTime)
            {
                mfWeight      = 1.0f;
                meState       = E_HOLD;
                mfExpiredTime = (lfTime - lfStartTime) - lfFadeIn;
            }
            else if (lfTime < ((lfFadeOut + lfDuration) + lfFadeIn) + lfStartTime)
            {
                meState       = E_FADEOUT;
                mfExpiredTime = ((lfTime - lfStartTime) - lfFadeIn) - lfDuration;
                mfWeight      = 1.0f - (mfExpiredTime / lfFadeOut);
            }
            else
            {
                mfExpiredTime = 0.0f;
                meState       = E_FINISHED;
                mfWeight      = 0.0f;
            }
        }
        else
        {
            // A forced fade-out: advance through the phases by the timestep.
            switch (meState)
            {
            case E_WAITING:
                mfExpiredTime += lfTimeStep;
                if (mfExpiredTime > lfStartTime)
                {
                    mfExpiredTime = 0.0f;
                    meState       = E_FADEIN;
                    mfWeight      = 0.0f;
                }
                break;
            case E_FADEIN:
                mfExpiredTime += lfTimeStep;
                mfWeight      += lfTimeStep / lfFadeIn;
                if (mfExpiredTime > lfFadeIn || mfWeight >= 1.0f)
                {
                    mfWeight      = 1.0f;
                    mfExpiredTime = 0.0f;
                    meState       = E_HOLD;
                }
                break;
            case E_HOLD:
                mfExpiredTime += lfTimeStep;
                if (mfExpiredTime > lfDuration && !(lfDuration < 0.01f && lfDuration > -0.01f))
                {
                    mfExpiredTime = 0.0f;
                    meState       = E_FADEOUT;
                    mfWeight      = 1.0f;
                }
                break;
            case E_FADEOUT:
                mfExpiredTime += lfTimeStep;
                mfWeight      -= lfTimeStep / lfFadeOut;
                if (mfExpiredTime > lfFadeOut || mfWeight <= 0.0f)
                {
                    mfExpiredTime = 0.0f;
                    mfWeight      = 0.0f;
                    meState       = E_FINISHED;
                }
                break;
            case E_FINISHED:
                break;
            default:
                CGS_FAIL_AT("Bad state in PFXNodeFader", CGS_ARTIST_FILE, 438);
                break;
            }
        }
    }

    // ------------------------------------------------------------------------------------
    // PFXNodeFader::StartFadeOut @0x824F5F90 (BrnPfxHookBlender.cpp:456)
    // ------------------------------------------------------------------------------------
    void PFXNodeFader::StartFadeOut(f32 lfFadeOutTimeOverride)
    {
        // The double 0.01 again (ARTIST 0x824F5FA8 `lfd` 0x82072738), two stores.
        if (lfFadeOutTimeOverride > 0.01)
        {
            mfForcedFadeoutTime = lfFadeOutTimeOverride;
        }
        else
        {
            mfForcedFadeoutTime = -1.0f;
        }
        switch (meState)
        {
        case E_WAITING:
        case E_FINISHED:
            meState = E_FINISHED;
            break;
        case E_FADEIN:
        case E_HOLD:
        case E_FADEOUT:
            meState = E_FADEOUT;
            break;
        default:
            CGS_FAIL_AT("Bad state in node fader.", CGS_ARTIST_FILE, 486);
            break;
        }
        mfExpiredTime = 0.0f;
    }

    // ====================================================================================
    // PFXHookNodeBlender
    // ====================================================================================

    // ------------------------------------------------------------------------------------
    // PFXHookNodeBlender::Construct (BrnPfxHookBlender.cpp:503): no hook bound. ARTIST inlines
    // it into EffectsArbitrator::Construct (BrnGuiUnity) and keeps no copy.
    // ------------------------------------------------------------------------------------
    void PFXHookNodeBlender::Construct()
    {
        mpHook = NULL;
    }

    // ------------------------------------------------------------------------------------
    // PFXHookNodeBlender::Initialise @0x82504510 (BrnPfxHookBlender.cpp:527; DWARF local
    // liIndex cpp:531)
    // ------------------------------------------------------------------------------------
    void PFXHookNodeBlender::Initialise(const PFXHook* lpHook, ColourCubeInfo* lpaColourCubes,
                                        uint32_t luCubeCount,
                                        f32 lfMaximumBloomWeight, f32 lfMaximumVignetteWeight,
                                        f32 lfMaximumBlurWeight, f32 lfMaximumDepthOfFieldWeight,
                                        f32 lfMaximum2DTintWeight, f32 lfMaximum3DTintWeight)
    {
        CGS_ASSERT_AT(lpHook, "lpHook", CGS_ARTIST_FILE, 529);

        mBlendedBloomData.Construct();
        mBlendedVignetteData.Construct();
        mBlendedBlurData.Construct();
        mBlendedDepthOfFieldData.Construct();
        mBlendedTint2DData.Construct();
        mBlendedTint3DData.Construct();

        mpHook                      = lpHook;
        mfMaximumBloomWeight        = lfMaximumBloomWeight;
        mfMaximumVignetteWeight     = lfMaximumVignetteWeight;
        mfMaximumBlurWeight         = lfMaximumBlurWeight;
        mfMaximumDepthOfFieldWeight = lfMaximumDepthOfFieldWeight;
        mfMaximum2DTintWeight       = lfMaximum2DTintWeight;
        mfMaximum3DTintWeight       = lfMaximum3DTintWeight;
        mfTime                      = 0.0f;

        for (int32_t liIndex = 0; liIndex < mpHook->miNodeCount; ++liIndex)
        {
            maNodeStates[liIndex].Initialise(lpaColourCubes, luCubeCount, mpHook->mpaNodes[liIndex]);
        }
    }

    // ------------------------------------------------------------------------------------
    // PFXHookNodeBlender::Update @0x824FB598 (BrnPfxHookBlender.cpp:567; DWARF locals liIndex
    // cpp:571, lMyFader cpp:583, lfWeight cpp:593). Each type folds in at
    // rw::math::fpu::Min(node weight, the per-type maximum); the fader's payload accessor
    // answers NULL when the group does not enable the type (the accumulators' no-op).
    // ------------------------------------------------------------------------------------
    void PFXHookNodeBlender::Update(f32 lfTimeStep)
    {
        if (mpHook != NULL)
        {
            mBlendedBloomData.Construct();
            mBlendedVignetteData.Construct();
            mBlendedBlurData.Construct();
            mBlendedDepthOfFieldData.Construct();
            mBlendedTint2DData.Construct();
            mBlendedTint3DData.Construct();

            for (int32_t liIndex = 0; liIndex < mpHook->miNodeCount; ++liIndex)
            {
                maNodeStates[liIndex].Update(mfTime, lfTimeStep);

                const PFXNodeFader& lMyFader = maNodeStates[liIndex];
                if (lMyFader.IsActive())
                {
                    f32 lfWeight = lMyFader.GetWeight();

                    mBlendedBloomData.Add(lMyFader.GetBloom(), rw::math::fpu::Min(lfWeight, mfMaximumBloomWeight));
                    mBlendedVignetteData.Add(lMyFader.GetVignette(), rw::math::fpu::Min(lfWeight, mfMaximumVignetteWeight));
                    mBlendedBlurData.Add(lMyFader.GetBlur(), rw::math::fpu::Min(lfWeight, mfMaximumBlurWeight));
                    mBlendedDepthOfFieldData.Add(lMyFader.GetDepthOfField(),
                                                 rw::math::fpu::Min(lfWeight, mfMaximumDepthOfFieldWeight));
                    mBlendedTint2DData.Add(lMyFader.Get2DTint(), rw::math::fpu::Min(lfWeight, mfMaximum2DTintWeight));
                    mBlendedTint3DData.Add(lMyFader.Get3DTint(), rw::math::fpu::Min(lfWeight, mfMaximum3DTintWeight));
                }
            }

            mfTime += lfTimeStep;
        }
    }

    // PFXHookNodeBlender::ForceTime (BrnPfxHookBlender.cpp:610; inlined by ARTIST, no copy)
    void PFXHookNodeBlender::ForceTime(f32 lfTime)
    {
        mfTime = lfTime;
    }

    // ------------------------------------------------------------------------------------
    // PFXHookNodeBlender::StartFadeOut @0x824F6058 (BrnPfxHookBlender.cpp:625; DWARF local
    // liIndex cpp:627)
    // ------------------------------------------------------------------------------------
    void PFXHookNodeBlender::StartFadeOut(f32 lfFadeOutTimeOverride)
    {
        for (int32_t liIndex = 0; liIndex < mpHook->miNodeCount; ++liIndex)
        {
            maNodeStates[liIndex].StartFadeOut(lfFadeOutTimeOverride);
        }
    }

    // PFXHookNodeBlender::Stop (BrnPfxHookBlender.cpp:645; inlined by ARTIST, no copy)
    void PFXHookNodeBlender::Stop()
    {
        mpHook = NULL;
    }

    // ------------------------------------------------------------------------------------
    // PFXHookNodeBlender::IsActive @0x824ECFC0 (DWARF local liIndex BrnPfxHookBlender.h:516):
    // true while any node of the bound hook has not finished.
    // ------------------------------------------------------------------------------------
    bool PFXHookNodeBlender::IsActive() const
    {
        if (mpHook != NULL)
        {
            for (int32_t liIndex = 0; liIndex < mpHook->miNodeCount; ++liIndex)
            {
                if (!maNodeStates[liIndex].IsFinished())
                {
                    return true;
                }
            }
        }
        return false;
    }
}

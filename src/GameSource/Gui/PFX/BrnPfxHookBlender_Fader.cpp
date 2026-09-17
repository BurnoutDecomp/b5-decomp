#include "GameSource/Gui/PFX/BrnPfxHookBlender.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysCollectionKey.h"  // Attrib::StringToKey
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"    // CgsResource::NULLResourcePtr
#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxcolourcube.h"  // rw::graphics::postfx::ColourCube (complete)

// =============================================================================
// BrnPfxHookBlender_Fader.cpp -- BrnGui::PFXNodeFader and BrnGui::PFXHookNodeBlender.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   PFXNodeFader::Initialise        @0x82504378
//   PFXNodeFader::Update            @0x824F5C40
//   PFXNodeFader::StartFadeOut      @0x824F5F90
//   PFXNodeFader::LookupColourCube  @0x825042A0
//   PFXHookNodeBlender::Initialise  @0x82504510
//   PFXHookNodeBlender::Update      @0x824FB598
//   PFXHookNodeBlender::StartFadeOut@0x824F6058
//   PFXHookNodeBlender::IsActive    @0x824ECFC0
// The accumulator Add bodies live in BrnPfxHookBlender.cpp.
//
// Every fsel in the console bodies is spelled as the compare it encodes:
// fsel(a, b, c) == (a >= 0) ? b : c.
// =============================================================================

namespace BrnGui
{
    namespace
    {
        // The console compares the fade times against 0.01 (flt_82002138) with the two-sided
        // `>= 0.0099999998 || <= -0.0099999998` idiom; spelled once.
        inline bool IsNonZeroTime(f32 lfTime)
        {
            return lfTime >= 0.0099999998f || lfTime <= -0.0099999998f;
        }
    }

    // ------------------------------------------------------------------------------------
    // PFXNodeFader::LookupColourCube @0x825042A0 (BrnPfxHookBlender.cpp:238)
    // Walk the cube table for the matching resource id; the cube must be a loaded resource.
    // ------------------------------------------------------------------------------------
    rw::graphics::postfx::ColourCube* PFXNodeFader::LookupColourCube(ColourCubeInfo* lpaColourCubes,
                                                                      u32 luNumColourCubes,
                                                                      u64 luResourceId)
    {
        if (luNumColourCubes == 0)
        {
            return 0;
        }
        u32 luIndex = 0;
        while (lpaColourCubes[luIndex].muResourceId != luResourceId)
        {
            if (++luIndex >= luNumColourCubes)
            {
                return 0;
            }
        }
        // The console's 12-byte compare against NULLResourcePtr is BaseResourcePtr::IsEqual;
        // the assert is non-gating (it returns the cube either way).
        CGS_ASSERT(!lpaColourCubes[luIndex].mpColourCube.IsEqual(&CgsResource::NULLResourcePtr),
                   "lpaColourCubes[luIndex].mpColourCube != CgsResource::NULLResourcePtr");
        return lpaColourCubes[luIndex].mpColourCube.operator->();
    }

    // ------------------------------------------------------------------------------------
    // PFXNodeFader::Initialise @0x82504378 (BrnPfxHookBlender.cpp:261)
    // ------------------------------------------------------------------------------------
    void PFXNodeFader::Initialise(ColourCubeInfo* lpaColourCubes, u32 luNumColourCubes,
                                  const PFXHookNode* lpPfxNode, const PFXGroup* lpPfxGroup)
    {
        CGS_ASSERT(lpPfxNode != 0, "lpPfxNode");                        // :261
        CGS_ASSERT(lpPfxGroup != 0, "PfxNode has no PfxGroup");         // :265

        mpPfxNode           = lpPfxNode;
        mpPfxGroup          = lpPfxGroup;
        mfWeight            = 0.0f;
        meState             = E_WAITING;
        mfForcedFadeoutTime = 0.0f;
        mfExpiredTime       = 0.0f;

        // Each enabled effect is built from its AttribSys asset, keyed by the group's
        // collection name (Attrib::StringToKey on the 32-char id). The console's order:
        // bloom, vignette, blur, depth of field, 2D tint, then the 3D tint lookup.
        if (lpPfxGroup->mbUseBloom)
        {
            mBloomData.Construct(Attrib::StringToKey(lpPfxGroup->macBloomDataID));
        }
        if (lpPfxGroup->mbUseVignette)
        {
            mVignetteData.Construct(Attrib::StringToKey(lpPfxGroup->macVignetteDataID));
        }
        if (lpPfxGroup->mbUseBlur)
        {
            mBlurData.Construct(Attrib::StringToKey(lpPfxGroup->macBlurDataID));
        }
        if (lpPfxGroup->mbUseDepthOfField)
        {
            mDepthOfFieldData.Construct(Attrib::StringToKey(lpPfxGroup->macDepthOfFieldDataID));
        }
        if (lpPfxGroup->mbUseTint2D)
        {
            mTint2DData.Construct(Attrib::StringToKey(lpPfxGroup->macTint2DDataID));
        }
        if (lpPfxGroup->mbUseTint3D)
        {
            mTint3DData.mpColourCube = 0;
            mTint3DData.mpColourCube =
                LookupColourCube(lpaColourCubes, luNumColourCubes, lpPfxGroup->muTint3DResourceId);
            CGS_ASSERT(mTint3DData.mpColourCube != 0, "mTint3DData.mpColourCube != NULL");   // :297
        }
    }

    // ------------------------------------------------------------------------------------
    // PFXNodeFader::Update @0x824F5C40 (BrnPfxHookBlender.cpp:313)
    //
    // Two regimes. While no fade-out has been forced (mfForcedFadeoutTime within 0.01 of
    // zero) the node is placed by ABSOLUTE hook time against its start time and the group's
    // fade-in / duration / fade-out envelope -- so a hook that starts late still lands on
    // the right phase. Once a fade-out has been forced the node advances by TIMESTEP through
    // the phase state machine instead, with the forced time standing in for the group's
    // fade-out (when it is positive).
    // ------------------------------------------------------------------------------------
    void PFXNodeFader::Update(f32 lfTimeNow, f32 lfTimestep)
    {
        CGS_ASSERT(mpPfxNode != 0, "mpPfxNode");                        // :313
        CGS_ASSERT(mpPfxGroup != 0, "PfxNode has no PfxGroup");         // :314

        const f32 lfStartTime = mpPfxNode->mfStartTime;
        const f32 lfFadeIn    = mpPfxGroup->mfFadeIn;
        const f32 lfDuration  = mpPfxGroup->mfDuration;
        const f32 lfFadeOut   = (mfForcedFadeoutTime > 0.01f) ? mfForcedFadeoutTime
                                                              : mpPfxGroup->mfFadeOut;

        if (IsNonZeroTime(mfForcedFadeoutTime))
        {
            switch (meState)
            {
            case E_WAITING:
                mfExpiredTime += lfTimestep;
                if (mfExpiredTime > lfStartTime)
                {
                    mfExpiredTime = 0.0f;
                    meState       = E_FADEIN;
                    mfWeight      = 0.0f;
                }
                break;
            case E_FADEIN:
                mfExpiredTime += lfTimestep;
                mfWeight      += lfTimestep / lfFadeIn;
                if (mfExpiredTime > lfFadeIn || mfWeight >= 1.0f)
                {
                    mfWeight      = 1.0f;
                    mfExpiredTime = 0.0f;
                    meState       = E_HOLD;
                }
                break;
            case E_HOLD:
                mfExpiredTime += lfTimestep;
                if (mfExpiredTime > lfDuration && IsNonZeroTime(lfDuration))
                {
                    mfExpiredTime = 0.0f;
                    meState       = E_FADEOUT;
                    mfWeight      = 1.0f;
                }
                break;
            case E_FADEOUT:
                mfExpiredTime += lfTimestep;
                mfWeight      -= lfTimestep / lfFadeOut;
                if (mfExpiredTime > lfFadeOut || mfWeight <= 0.0f)
                {
                    mfExpiredTime = 0.0f;
                    mfWeight      = 0.0f;
                    meState       = E_FINISHED;
                }
                break;
            case E_FINISHED:
                return;
            default:
                CGS_ASSERT(false, "Bad state in PFXNodeFader");       // :438
                break;
            }
            return;
        }

        // ---- the free-running placement by absolute time --------------------------------
        if (lfTimeNow < lfStartTime)
        {
            mfExpiredTime = lfTimeNow;
            mfWeight      = 0.0f;
            meState       = E_WAITING;
            return;
        }
        if (lfTimeNow < lfFadeIn + lfStartTime)
        {
            mfExpiredTime = lfTimeNow - lfStartTime;
            meState       = E_FADEIN;
            mfWeight      = mfExpiredTime / lfFadeIn;
            return;
        }
        if (!(IsNonZeroTime(lfDuration) && lfTimeNow >= (lfDuration + lfFadeIn) + lfStartTime))
        {
            mfWeight      = 1.0f;
            meState       = E_HOLD;
            mfExpiredTime = (lfTimeNow - lfStartTime) - lfFadeIn;
            return;
        }
        if (lfTimeNow >= ((lfFadeOut + lfDuration) + lfFadeIn) + lfStartTime)
        {
            mfExpiredTime = 0.0f;
            meState       = E_FINISHED;
            mfWeight      = 0.0f;
            return;
        }
        meState       = E_FADEOUT;
        mfExpiredTime = ((lfTimeNow - lfStartTime) - lfFadeIn) - lfDuration;
        mfWeight      = 1.0f - (mfExpiredTime / lfFadeOut);
    }

    // ------------------------------------------------------------------------------------
    // PFXNodeFader::StartFadeOut @0x824F5F90 (BrnPfxHookBlender.cpp:486)
    // ------------------------------------------------------------------------------------
    void PFXNodeFader::StartFadeOut(f32 lfFadeOutTime)
    {
        mfForcedFadeoutTime = (lfFadeOutTime <= 0.01f) ? -1.0f : lfFadeOutTime;
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
            CGS_ASSERT(false, "Bad state in node fader.");             // :486
            break;
        }
        mfExpiredTime = 0.0f;
    }

    // ====================================================================================
    // PFXHookNodeBlender
    // ====================================================================================

    // The six accumulator resets the console inlines into Initialise @0x82504510 (+2464..+2792)
    // and Update @0x824FB598 (the same stores at v1[616..698]).
    void PFXHookNodeBlender::ConstructBlends()
    {
        mBlendedBloomData.Construct();
        mBlendedVignetteData.Construct();
        mBlendedBlurData.Construct();
        mBlendedDepthOfFieldData.Construct();
        mBlendedTint2DData.Construct();
        mBlendedTint3DData.Construct();
    }

    // ------------------------------------------------------------------------------------
    // PFXHookNodeBlender::Initialise @0x82504510 (BrnPfxHookBlender.cpp:529)
    // ------------------------------------------------------------------------------------
    void PFXHookNodeBlender::Initialise(const PFXHook* lpHook, const PFXHookBundle* lpBundle,
                                        ColourCubeInfo* lpaColourCubes, u32 luNumColourCubes,
                                        f32 lfMaximumBloomWeight, f32 lfMaximumVignetteWeight,
                                        f32 lfMaximumBlurWeight, f32 lfMaximumDepthOfFieldWeight,
                                        f32 lfMaximum2DTintWeight, f32 lfMaximum3DTintWeight)
    {
        CGS_ASSERT(lpHook != 0, "lpHook");                              // :529
        ConstructBlends();
        mpHook                      = lpHook;
        mfMaximumBloomWeight        = lfMaximumBloomWeight;
        mfMaximumVignetteWeight     = lfMaximumVignetteWeight;
        mfMaximumBlurWeight         = lfMaximumBlurWeight;
        mfMaximumDepthOfFieldWeight = lfMaximumDepthOfFieldWeight;
        mfMaximum2DTintWeight       = lfMaximum2DTintWeight;
        mfMaximum3DTintWeight       = lfMaximum3DTintWeight;
        mfTime                      = 0.0f;
        for (s32 liNode = 0; liNode < lpHook->miNodeCount; ++liNode)
        {
            const PFXHookNode* lpNode = lpBundle->GetHookNode(*lpHook, liNode);
            maNodeStates[liNode].Initialise(lpaColourCubes, luNumColourCubes,
                                            lpNode, lpBundle->GetNodeGroup(*lpNode));
        }
    }

    // ------------------------------------------------------------------------------------
    // PFXHookNodeBlender::Update @0x824FB598 (BrnPfxHookBlender.h:376..491)
    // ------------------------------------------------------------------------------------
    void PFXHookNodeBlender::Update(f32 lfTimestep)
    {
        if (mpHook == 0)
        {
            return;
        }
        ConstructBlends();
        for (s32 liNode = 0; liNode < mpHook->miNodeCount; ++liNode)
        {
            PFXNodeFader& lrFader = maNodeStates[liNode];
            lrFader.Update(mfTime, lfTimestep);
            if (!lrFader.IsActive())
            {
                continue;
            }
            // Each type folds in at min(node weight, the per-type maximum) -- the console's
            // `fsel (w - max), max, w` -- and only when the node's group enables the type
            // (a null source is the accumulators' own no-op).
            const f32       lfWeight = lrFader.GetWeight();
            const PFXGroup* lpGroup  = lrFader.GetGroup();
            CGS_ASSERT(lpGroup != 0, "mpPfxNode");                      // .h:376 / :399 / :422 / :445 / :468 / :491

            const f32 lfBloom = (lfWeight - mfMaximumBloomWeight >= 0.0f) ? mfMaximumBloomWeight : lfWeight;
            mBlendedBloomData.Add(lpGroup->mbUseBloom ? lrFader.GetBloom() : 0, lfBloom);

            const f32 lfVignette = (lfWeight - mfMaximumVignetteWeight >= 0.0f) ? mfMaximumVignetteWeight : lfWeight;
            mBlendedVignetteData.Add(lpGroup->mbUseVignette ? lrFader.GetVignette() : 0, lfVignette);

            const f32 lfBlur = (lfWeight - mfMaximumBlurWeight >= 0.0f) ? mfMaximumBlurWeight : lfWeight;
            mBlendedBlurData.Add(lpGroup->mbUseBlur ? lrFader.GetBlur() : 0, lfBlur);

            const f32 lfDepthOfField = (lfWeight - mfMaximumDepthOfFieldWeight >= 0.0f) ? mfMaximumDepthOfFieldWeight : lfWeight;
            mBlendedDepthOfFieldData.Add(lpGroup->mbUseDepthOfField ? lrFader.GetDepthOfField() : 0, lfDepthOfField);

            const f32 lf2DTint = (lfWeight - mfMaximum2DTintWeight >= 0.0f) ? mfMaximum2DTintWeight : lfWeight;
            mBlendedTint2DData.Add(lpGroup->mbUseTint2D ? lrFader.Get2DTint() : 0, lf2DTint);

            const f32 lf3DTint = (lfWeight - mfMaximum3DTintWeight >= 0.0f) ? mfMaximum3DTintWeight : lfWeight;
            if (lpGroup->mbUseTint3D)
            {
                // No Add for the 3D tint: the console stores the three fields outright
                // (+2784 cube, +2788 weight, +2792 count = 1.0), the last node winning.
                mBlendedTint3DData.mfCount            = 1.0f;
                mBlendedTint3DData.mfWeight           = lf3DTint;
                mBlendedTint3DData.mData.mpColourCube = lrFader.Get3DTint()->mpColourCube;
            }
        }
        mfTime += lfTimestep;
    }

    // ------------------------------------------------------------------------------------
    // PFXHookNodeBlender::StartFadeOut @0x824F6058
    // ------------------------------------------------------------------------------------
    void PFXHookNodeBlender::StartFadeOut(f32 lfFadeOutTime)
    {
        for (s32 liNode = 0; liNode < mpHook->miNodeCount; ++liNode)
        {
            maNodeStates[liNode].StartFadeOut(lfFadeOutTime);
        }
    }

    // ------------------------------------------------------------------------------------
    // PFXHookNodeBlender::IsActive @0x824ECFC0 -- `lwz r11,0(r3)` (the hook), miNodeCount at
    // +0x38, then each fader's leading state word compared against E_FINISHED (`cmpwi r8,4`).
    // ------------------------------------------------------------------------------------
    bool PFXHookNodeBlender::IsActive() const
    {
        if (mpHook == 0 || mpHook->miNodeCount <= 0)
        {
            return false;
        }
        for (s32 liNode = 0; liNode < mpHook->miNodeCount; ++liNode)
        {
            if (maNodeStates[liNode].GetState() != PFXNodeFader::E_FINISHED)
            {
                return true;
            }
        }
        return false;
    }
}

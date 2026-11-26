#ifndef BRN_PFX_HOOK_BLENDER_H
#define BRN_PFX_HOOK_BLENDER_H

#include "types.hpp"
#include "SharedClasses/Graphics/BrnEffectsData.h"                 // BrnEffects::BloomData / VignetteData / ...
#include "GameSource/Gui/PFX/BrnGuiPFXHooks.h"                     // BrnGui::PFXHook / PFXHookNode / PFXGroup / PFXHookBundle
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h" // CgsResource::ResourcePtr<ColourCube>
#include "GameShared/GameClasses/Core/CgsAssert.h"                   // CGS_ASSERT_AT (the fader payload accessors)

namespace rw { namespace graphics { namespace postfx { struct ColourCube; } } }

// =============================================================================
// BrnPfxHookBlender.h
//
// The runtime half of the GUI post-FX hook system (DecFIGS DWARF BrnPfxHookBlender.h):
//
//   ColourCubeInfo      -- one loaded 3D-tint colour cube and the resource id it answers to
//   PfxContainer<T>     -- the per-type weighted blend accumulator
//   PFXNodeFader        -- ONE hook node's fade envelope (wait / fade in / hold / fade out /
//                          finished) and the five asset-keyed effect payloads it carries
//   PFXHookNodeBlender  -- ONE playing hook: up to KU_MAX_CONCURRENT_NODES faders and the six
//                          accumulators they fold into every frame
//
// The containers' members, the fader's and the blender's out-of-line members are defined in
// BrnPfxHookBlender.cpp, the original single file (DWARF lines cpp:57..645).
// =============================================================================

namespace BrnGui
{
    const u8 KU_MAX_CONCURRENT_NODES = 8;   // BrnPfxHookBlender.h:27

    // BrnPfxHookBlender.h:42
    struct ColourCubeInfo
    {
        CgsResource::ResourcePtr<rw::graphics::postfx::ColourCube> mpColourCube;   // :44
        u64                                                        muResourceId;   // :45
    };

    // BrnPfxHookBlender.h:60 -- the per-type weighted blend accumulator one hook (or the
    // arbitrator's final mix) folds its nodes' payloads into. The members are defined in
    // BrnPfxHookBlender.cpp, as ARTIST's assert path for Add shows (cpp:84); ARTIST keeps
    // out-of-line copies of the ones it does not inline (Add 0x824F6810/0x824F69E8/
    // 0x824F6B70/0x824F6C40/0x824F6D60, Construct 0x824F6968/0x824F6AD0).
    template <typename T>
    struct PfxContainer
    {
        void Construct();                                   // :64
        void Add(const T* lpEffectsData, float lfWeight);   // :69
        f32 GetWeight() const;                              // :73
        const T& GetData() const;                           // :77
        f32 GetCount() const;                               // :81

    private:
        T   mData;      // :84
        f32 mfWeight;   // :85
        f32 mfCount;    // :86
    };

    // BrnPfxHookBlender.h:91 -- the 3D tint is a colour cube, not a blend: the last enabled
    // node wins outright (BrnPfxHookBlender.cpp:57/114/150/181/212).
    template <>
    struct PfxContainer<BrnEffects::TintData>
    {
        void Construct();                                                       // :93
        void Add(const BrnEffects::TintData* lpEffectsData, f32 lfWeight);      // :95
        f32 GetWeight() const;                                                  // :96
        const BrnEffects::TintData& GetData() const;                            // :97
        f32 GetCount() const;                                                   // :98

    private:
        BrnEffects::TintData mData;      // :100
        f32                  mfWeight;   // :101
        f32                  mfCount;    // :102
    };

    // BrnPfxHookBlender.h:115 -- one node of a playing hook.
    struct PFXNodeFader
    {
        // BrnPfxHookBlender.h:169
        enum ENodePhase
        {
            E_WAITING  = 0,
            E_FADEIN   = 1,
            E_HOLD     = 2,
            E_FADEOUT  = 3,
            E_FINISHED = 4,
        };

        // :121 @0x82504378 -- bind a node: zero the envelope, then construct each effect
        // payload the node's group enables from its AttribSys asset key (the collection
        // name hashed by Attrib::StringToKey) and, for the 3D tint, look the colour cube up.
        void Initialise(ColourCubeInfo* lpaColourCubes, u32 luCubeCount, PFXHookNode* lpPfxNode);
        // :126 @0x824F5C40 -- advance the envelope: by absolute hook time while the node runs
        // free, by timestep through the state machine once a fade-out was forced.
        void Update(f32 lfTimeNow, f32 lfTimestep);
        f32  GetWeight() const   { return mfWeight; }                      // :130
        bool IsActive() const    { return meState != E_FINISHED && meState != E_WAITING; } // :134
        bool IsFinished() const  { return meState == E_FINISHED; }         // :138
        // :142 @0x824F5F90 -- force the fade-out (time <= 0.01 means "the group's own").
        void StartFadeOut(f32 lfFadeOutTimeOverride);
        // :146..:166 -- the payload when the node's group enables the effect, else NULL;
        // defined below the class (their asserts name this header, lines 376..491).
        const BrnEffects::BloomData*        GetBloom() const;
        const BrnEffects::VignetteData*     GetVignette() const;
        const BrnEffects::BlurData*         GetBlur() const;
        const BrnEffects::DepthOfFieldData* GetDepthOfField() const;
        const BrnEffects::TintData2d*       Get2DTint() const;
        const BrnEffects::TintData*         Get3DTint() const;

    private:
        // :182 @0x825042A0 -- the cube whose resource id matches, or 0.
        rw::graphics::postfx::ColourCube* LookupColourCube(ColourCubeInfo* lpaColourCubes,
                                                           u32 luCubeCount,
                                                           u64 luResourceId);

        PFXHookNode*                 mpPfxNode;             // :184  +0x00
        ENodePhase                   meState;               // :185  +0x04
        f32                          mfWeight;              // :186  +0x08
        f32                          mfExpiredTime;         // :187  +0x0C
        f32                          mfForcedFadeoutTime;   // :188  +0x10
        BrnEffects::BloomData        mBloomData;            // :190  +0x20
        BrnEffects::VignetteData     mVignetteData;         // :191  +0x40
        BrnEffects::BlurData         mBlurData;             // :192  +0x90
        BrnEffects::DepthOfFieldData mDepthOfFieldData;     // :193  +0xF0
        BrnEffects::TintData2d       mTint2DData;           // :194  +0x110
        BrnEffects::TintData         mTint3DData;           // :195  +0x120  (304 bytes per fader on the console)
    };

    // BrnPfxHookBlender.h:296 -- one PLAYING hook.
    class PFXHookNodeBlender
    {
    public:
        // :213 (BrnPfxHookBlender.cpp:503) -- no hook bound.
        void Construct();
        // :225 @0x82504510 -- bind a hook and initialise one fader per node.
        void Initialise(const PFXHook* lpHook, ColourCubeInfo* lpaColourCubes, u32 luCubeCount,
                        f32 lfMaximumBloomWeight, f32 lfMaximumVignetteWeight,
                        f32 lfMaximumBlurWeight, f32 lfMaximumDepthOfFieldWeight,
                        f32 lfMaximum2DTintWeight, f32 lfMaximum3DTintWeight);
        // :229 @0x824FB598 -- reset the six accumulators, advance every fader, fold each
        // live one in at min(its weight, the per-type maximum), then advance the clock.
        void Update(f32 lfTimestep);
        void Stop();                     // :233 (BrnPfxHookBlender.cpp:645)
        void ForceTime(f32 lfTime);      // :236 (BrnPfxHookBlender.cpp:610)
        // :240 @0x824ECFC0 -- true while the blender is still working: false if it has no
        // hook, the hook has no nodes, or every live node slot has reached the finished
        // state; otherwise true (some node is still blending).
        bool IsActive() const;
        f32  GetTime() const             { return mfTime; }            // :244
        const PFXHook* GetHook() const   { return mpHook; }            // :248
        // :252 @0x824F6058 -- every fader.
        void StartFadeOut(f32 lfFadeOutTimeOverride);
        const PfxContainer<BrnEffects::BloomData>&        GetBloom() const        { return mBlendedBloomData; }        // :256
        const PfxContainer<BrnEffects::VignetteData>&     GetVignette() const     { return mBlendedVignetteData; }     // :260
        const PfxContainer<BrnEffects::BlurData>&         GetBlur() const         { return mBlendedBlurData; }         // :264
        const PfxContainer<BrnEffects::DepthOfFieldData>& GetDepthOfField() const { return mBlendedDepthOfFieldData; } // :268
        const PfxContainer<BrnEffects::TintData2d>&       Get2DTint() const       { return mBlendedTint2DData; }       // :272
        const PfxContainer<BrnEffects::TintData>&         Get3DTint() const       { return mBlendedTint3DData; }       // :276

    private:
        const PFXHook*    mpHook;                                   // :302  +0x000
        PFXNodeFader      maNodeStates[KU_MAX_CONCURRENT_NODES];    // :303  +0x010 (stride 0x130)
        f32               mfTime;                                   // :304  +0x990
        PfxContainer<BrnEffects::BloomData>        mBlendedBloomData;         // :306  +0x9A0
        PfxContainer<BrnEffects::VignetteData>     mBlendedVignetteData;      // :307  +0x9D0
        PfxContainer<BrnEffects::BlurData>         mBlendedBlurData;          // :308  +0xA30
        PfxContainer<BrnEffects::DepthOfFieldData> mBlendedDepthOfFieldData;  // :309  +0xAA0
        PfxContainer<BrnEffects::TintData2d>       mBlendedTint2DData;        // :310  +0xAC0
        PfxContainer<BrnEffects::TintData>         mBlendedTint3DData;        // :311  +0xAE0
        f32               mfMaximumBloomWeight;                     // :313  +0xAEC
        f32               mfMaximumVignetteWeight;                  // :314  +0xAF0
        f32               mfMaximumBlurWeight;                      // :315  +0xAF4
        f32               mfMaximumDepthOfFieldWeight;              // :316  +0xAF8
        f32               mfMaximum2DTintWeight;                    // :317  +0xAFC
        f32               mfMaximum3DTintWeight;                    // :318  +0xB00  (0xB10 per blender on the console)
    };

    // The fader's payload accessors, header-inline: ARTIST inlines them into
    // PFXHookNodeBlender::Update with their asserts on this header's lines (0x824FB774..).
#define CGS_ARTIST_BRNPFXHOOKBLENDER_H_FILE "..\\..\\..\\GameSource\\Gui/pfx/BrnPfxHookBlender.h"
    inline const BrnEffects::BloomData* PFXNodeFader::GetBloom() const
    {
        CGS_ASSERT_AT(mpPfxNode, "mpPfxNode", CGS_ARTIST_BRNPFXHOOKBLENDER_H_FILE, 376);
        return mpPfxNode->mpGroup->GetUseBloom() ? &mBloomData : NULL;
    }

    inline const BrnEffects::VignetteData* PFXNodeFader::GetVignette() const
    {
        CGS_ASSERT_AT(mpPfxNode, "mpPfxNode", CGS_ARTIST_BRNPFXHOOKBLENDER_H_FILE, 399);
        return mpPfxNode->mpGroup->GetUseVignette() ? &mVignetteData : NULL;
    }

    inline const BrnEffects::BlurData* PFXNodeFader::GetBlur() const
    {
        CGS_ASSERT_AT(mpPfxNode, "mpPfxNode", CGS_ARTIST_BRNPFXHOOKBLENDER_H_FILE, 422);
        return mpPfxNode->mpGroup->GetUseBlur() ? &mBlurData : NULL;
    }

    inline const BrnEffects::DepthOfFieldData* PFXNodeFader::GetDepthOfField() const
    {
        CGS_ASSERT_AT(mpPfxNode, "mpPfxNode", CGS_ARTIST_BRNPFXHOOKBLENDER_H_FILE, 445);
        return mpPfxNode->mpGroup->GetUseDepthOfField() ? &mDepthOfFieldData : NULL;
    }

    inline const BrnEffects::TintData2d* PFXNodeFader::Get2DTint() const
    {
        CGS_ASSERT_AT(mpPfxNode, "mpPfxNode", CGS_ARTIST_BRNPFXHOOKBLENDER_H_FILE, 468);
        return mpPfxNode->mpGroup->GetUseTint2D() ? &mTint2DData : NULL;
    }

    inline const BrnEffects::TintData* PFXNodeFader::Get3DTint() const
    {
        CGS_ASSERT_AT(mpPfxNode, "mpPfxNode", CGS_ARTIST_BRNPFXHOOKBLENDER_H_FILE, 491);
        return mpPfxNode->mpGroup->GetUseTint3D() ? &mTint3DData : NULL;
    }
}

#endif

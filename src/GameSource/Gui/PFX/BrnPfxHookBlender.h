#ifndef BRN_PFX_HOOK_BLENDER_H
#define BRN_PFX_HOOK_BLENDER_H

#include "types.hpp"
#include "SharedClasses/Graphics/BrnEffectsData.h"                 // BrnEffects::BloomData / VignetteData / ...
#include "GameSource/Gui/PFX/BrnGuiPFXHooks.h"                     // BrnGui::PFXHook / PFXHookNode / PFXGroup / PFXHookBundle
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h" // CgsResource::ResourcePtr<ColourCube>

namespace rw { namespace graphics { namespace postfx { class ColourCube; } } }

// =============================================================================
// BrnPfxHookBlender.h
//
// The runtime half of the GUI post-FX hook system (DecFIGS DWARF BrnPfxHookBlender.h):
//
//   ColourCubeInfo      -- one loaded 3D-tint colour cube and the resource id it answers to
//   PfxContainer<T>     -- the per-type weighted blend accumulator (BloomBlend etc. below)
//   PFXNodeFader        -- ONE hook node's fade envelope (wait / fade in / hold / fade out /
//                          finished) and the five asset-keyed effect payloads it carries
//   PFXHookNodeBlender  -- ONE playing hook: up to KU_MAX_CONCURRENT_NODES faders and the six
//                          accumulators they fold into every frame
//
// The accumulators' Add bodies are the ARTIST ones (asserts cite
// "..\\..\\..\\GameSource\\Gui/PFX/BrnPfxHookBlender.cpp", line 84):
//   if (!lpSrc) return this;                       // null source is a no-op
//   mfCount += 1.0f;
//   if (mfWeight < lfWeight) { ASSERT(lfWeight<=1.0f); mfWeight = lfWeight; }
//   if (mfCount == 1.0f) copy the payload straight from the source;
//   else                 payload = src*lfWeight + payload*(1 - lfWeight)
//                                  (per scalar member and per vector lane).
//
// NOTE: the accumulator layouts are proven by the Add bodies' load/store offsets, not by the
// post-FX *Data structs (those carry SIMD alignment padding that these tightly packed blend
// payloads do not). Members are accessed by name only.
//
// [FLAG PC bring-up] PFXNodeFader / PFXHookNodeBlender are RUNTIME objects (carved inside
// BrnGui::EffectsArbitrator, never deserialised), so their pointer members widen on x64 and
// no console byte offset is asserted on them -- the offsets quoted are provenance for the
// asm reads only. One widening the console never needed: a fader keeps the PFXGroup it was
// initialised from (the console re-derives it from the node's 32-bit link each read; on PC
// that link is a blob-relative offset only the bundle can resolve -- see BrnGuiPFXHooks.h).
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

    // ---- BloomData accumulator (Add @ 0x824F6810) ----------------------------
    // payload == BrnEffects::BloomData (32B) then weight @0x20, count @0x24.
    struct BloomBlend
    {
        BrnEffects::BloomData mData;   // +0x00 .. +0x1F
        f32                   mfWeight; // +0x20
        f32                   mfCount;  // +0x24

        // Inlined at every console site (PFXHookNodeBlender::Initialise/Update @0x82504510 /
        // @0x824FB598, EffectsArbitrator::GenerateEffectFrameEvents): the BloomData defaults
        // (0.98, 0.77, unk_82FFAED0) then the two bookkeeping fields at 0.0f.
        BloomBlend* Construct() { mData.Construct(); mfWeight = 0.0f; mfCount = 0.0f; return this; }
        BloomBlend* Add(const BrnEffects::BloomData* lpSrc, f32 lfWeight);
        f32 GetWeight() const { return mfWeight; }
        f32 GetCount() const  { return mfCount; }
        const BrnEffects::BloomData& GetData() const { return mData; }
    };

    // ---- DepthOfFieldData accumulator (Add @ 0x824F6C40) ---------------------
    // Tightly packed 5-float payload (the X360 1-weight copy moves exactly 5
    // dwords and the blend touches the first four lanes); weight @0x14, count
    // @0x18. This payload is NOT the padded BrnEffects::DepthOfFieldData; the
    // five fields are spelled here by name so member access stays honest.
    struct DepthOfFieldBlend
    {
        f32 mfNearPlane;   // +0x00
        f32 mfFocalPlane;  // +0x04
        f32 mfFocalPlane2; // +0x08
        f32 mfFarPlane;    // +0x0C
        f32 mfDofAmount;   // +0x10  (copied at count==1, NOT blended)
        f32 mfWeight;      // +0x14
        f32 mfCount;       // +0x18

        // Inlined at the console sites: {0, 0, 0, 0, 1.0} then the two bookkeeping 0.0f
        // (PFXHookNodeBlender::Initialise @0x82504510 stores +2720..+2744 exactly so).
        DepthOfFieldBlend* Construct()
        {
            mfNearPlane = 0.0f; mfFocalPlane = 0.0f; mfFocalPlane2 = 0.0f; mfFarPlane = 0.0f;
            mfDofAmount = 1.0f; mfWeight = 0.0f; mfCount = 0.0f; return this;
        }
        DepthOfFieldBlend* Add(const DepthOfFieldBlend* lpSrc, f32 lfWeight);
        // The fader carries a padded BrnEffects::DepthOfFieldData; its five leading floats ARE
        // this payload (same names, same order), which is what the console's Add reads.
        DepthOfFieldBlend* Add(const BrnEffects::DepthOfFieldData* lpSrc, f32 lfWeight)
        {
            return Add(reinterpret_cast<const DepthOfFieldBlend*>(lpSrc), lfWeight);
        }
        f32 GetWeight() const { return mfWeight; }
        f32 GetCount() const  { return mfCount; }
    };

    // ---- TintData2d accumulator (Add @ 0x824F6D60) ---------------------------
    // payload == BrnEffects::TintData2d (one Vector4, 16B) then weight @0x10,
    // count @0x14.
    struct TintData2dBlend
    {
        BrnEffects::TintData2d mData;    // +0x00 .. +0x0F
        f32                    mfWeight; // +0x10
        f32                    mfCount;  // +0x14

        // Inlined at the console sites: the default colour (unk_82FFB230) then 0.0f / 0.0f.
        TintData2dBlend* Construct() { mData.Construct(); mfWeight = 0.0f; mfCount = 0.0f; return this; }
        TintData2dBlend* Add(const BrnEffects::TintData2d* lpSrc, f32 lfWeight);
        f32 GetWeight() const { return mfWeight; }
        f32 GetCount() const  { return mfCount; }
        const BrnEffects::TintData2d& GetData() const { return mData; }
    };

    // ---- BlurData accumulator (Add @ 0x824F6B70) -----------------------------
    // payload == BrnEffects::BlurData (96B) then weight @0x60, count @0x64. The
    // blend path delegates to BrnEffects::BlurData::SetToBlend; the copy path is a
    // straight 96-byte memcpy of the payload.
    struct BlurBlend
    {
        BrnEffects::BlurData mData;    // +0x00 .. +0x5F
        f32                  mfWeight; // +0x60
        f32                  mfCount;  // +0x64

        // Construct @ 0x824F6AD0 -- seed the payload with the BlurData defaults
        // (the same 5 scalar @0..0x10 + 4 Vector2 @0x20/0x30/0x40/0x50 store set
        // BrnEffects::BlurData::Construct writes) then zero the two bookkeeping
        // fields mfWeight@0x60 / mfCount@0x64 (both 0.0f). Returns this (the X360
        // body returns its result pointer).
        BlurBlend* Construct();

        BlurBlend* Add(const BrnEffects::BlurData* lpSrc, f32 lfWeight);
        f32 GetWeight() const { return mfWeight; }
        f32 GetCount() const  { return mfCount; }
        const BrnEffects::BlurData& GetData() const { return mData; }
    };

    // ---- VignetteData accumulator (Add @ 0x824F69E8) -------------------------
    // payload == BrnEffects::VignetteData (80B) then weight @0x50, count @0x54. The
    // blend path delegates to BrnEffects::VignetteData::SetToBlend; the copy path is
    // a straight 80-byte memcpy of the payload.
    struct VignetteBlend
    {
        BrnEffects::VignetteData mData;    // +0x00 .. +0x4F (80B)
        f32                      mfWeight; // +0x50
        f32                      mfCount;  // +0x54

        // Construct @ 0x824F6968 -- seed the payload with the VignetteData defaults
        // (mfAngle@0x00, mfSharpness@0x04, and the four 16-byte vector members
        // mv2Amount@0x10 / mv2Centre@0x20 / mv4InnerColour@0x30 / mv4OuterColour@0x40
        // that BrnEffects::VignetteData::Construct writes) then zero the two
        // bookkeeping fields mfWeight@0x50 / mfCount@0x54 (both 0.0f). Returns this.
        VignetteBlend* Construct();

        VignetteBlend* Add(const BrnEffects::VignetteData* lpSrc, f32 lfWeight);
        f32 GetWeight() const { return mfWeight; }
        f32 GetCount() const  { return mfCount; }
        const BrnEffects::VignetteData& GetData() const { return mData; }
    };

    // ---- TintData (3D colour cube) accumulator --------------------------------
    // PfxContainer<BrnEffects::TintData>: the cube pointer, then weight, then count. The
    // console never calls an Add for this one -- PFXHookNodeBlender::Update stores the
    // three fields directly (@0x824FB598: +2784 cube, +2788 weight, +2792 count = 1.0).
    struct TintDataBlend
    {
        BrnEffects::TintData mData;    // the cube pointer
        f32                  mfWeight;
        f32                  mfCount;

        TintDataBlend* Construct() { mData.Construct(); mfWeight = 0.0f; mfCount = 0.0f; return this; }
        f32 GetWeight() const { return mfWeight; }
        f32 GetCount() const  { return mfCount; }
        const BrnEffects::TintData& GetData() const { return mData; }
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
        void Initialise(ColourCubeInfo* lpaColourCubes, u32 luNumColourCubes,
                        const PFXHookNode* lpPfxNode, const PFXGroup* lpPfxGroup);
        // :126 @0x824F5C40 -- advance the envelope: by absolute hook time while the node runs
        // free, by timestep through the state machine once a fade-out was forced.
        void Update(f32 lfTimeNow, f32 lfTimestep);
        f32  GetWeight() const   { return mfWeight; }                      // :130
        bool IsActive() const    { return meState != E_WAITING && meState != E_FINISHED; } // :134
        bool IsFinished() const  { return meState == E_FINISHED; }         // :138
        // :142 @0x824F5F90 -- force the fade-out (time <= 0.01 means "the group's own").
        void StartFadeOut(f32 lfFadeOutTime);
        const BrnEffects::BloomData*        GetBloom() const        { return &mBloomData; }        // :146
        const BrnEffects::VignetteData*     GetVignette() const     { return &mVignetteData; }     // :150
        const BrnEffects::BlurData*         GetBlur() const         { return &mBlurData; }         // :154
        const BrnEffects::DepthOfFieldData* GetDepthOfField() const { return &mDepthOfFieldData; } // :158
        const BrnEffects::TintData2d*       Get2DTint() const       { return &mTint2DData; }       // :162
        const BrnEffects::TintData*         Get3DTint() const       { return &mTint3DData; }       // :166
        ENodePhase GetState() const { return meState; }
        const PFXGroup* GetGroup() const { return mpPfxGroup; }

    private:
        // :182 @0x825042A0 -- the cube whose resource id matches, or 0.
        static rw::graphics::postfx::ColourCube* LookupColourCube(ColourCubeInfo* lpaColourCubes,
                                                                   u32 luNumColourCubes,
                                                                   u64 luResourceId);

        const PFXHookNode*           mpPfxNode;             // :184  +0x00
        const PFXGroup*              mpPfxGroup;            // PC widening: the node's group, resolved once
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
        // :213 -- inlined everywhere the console constructs one (EffectsArbitrator::Construct
        // stores the five blenders' first word 0): no hook bound.
        void Construct() { mpHook = 0; }
        // :225 @0x82504510 -- bind a hook and initialise one fader per node.
        void Initialise(const PFXHook* lpHook, const PFXHookBundle* lpBundle,
                        ColourCubeInfo* lpaColourCubes, u32 luNumColourCubes,
                        f32 lfMaximumBloomWeight, f32 lfMaximumVignetteWeight,
                        f32 lfMaximumBlurWeight, f32 lfMaximumDepthOfFieldWeight,
                        f32 lfMaximum2DTintWeight, f32 lfMaximum3DTintWeight);
        // :229 @0x824FB598 -- reset the six accumulators, advance every fader, fold each
        // live one in at min(its weight, the per-type maximum), then advance the clock.
        void Update(f32 lfTimestep);
        void Stop()                      { mpHook = 0; }               // :233 (inlined; no export)
        void ForceTime(f32 lfTime)       { mfTime = lfTime; }          // :236 (inlined; no export)
        // :240 @0x824ECFC0 -- true while the blender is still working: false if it has no
        // hook, the hook has no nodes, or every live node slot has reached the finished
        // state; otherwise true (some node is still blending).
        bool IsActive() const;
        f32  GetTime() const             { return mfTime; }            // :244
        const PFXHook* GetHook() const   { return mpHook; }            // :248
        // :252 @0x824F6058 -- every fader.
        void StartFadeOut(f32 lfFadeOutTime);
        const BloomBlend&        GetBloom() const        { return mBlendedBloomData; }        // :256
        const VignetteBlend&     GetVignette() const     { return mBlendedVignetteData; }     // :260
        const BlurBlend&         GetBlur() const         { return mBlendedBlurData; }         // :264
        const DepthOfFieldBlend& GetDepthOfField() const { return mBlendedDepthOfFieldData; } // :268
        const TintData2dBlend&   Get2DTint() const       { return mBlendedTint2DData; }       // :272
        const TintDataBlend&     Get3DTint() const       { return mBlendedTint3DData; }       // :276

    private:
        void ConstructBlends();   // the six accumulator resets Initialise and Update share (inlined on the console)

        const PFXHook*    mpHook;                                   // :302  +0x000
        PFXNodeFader      maNodeStates[KU_MAX_CONCURRENT_NODES];    // :303  +0x010 (stride 0x130)
        f32               mfTime;                                   // :304  +0x990
        BloomBlend        mBlendedBloomData;                        // :306  +0x9A0
        VignetteBlend     mBlendedVignetteData;                     // :307  +0x9D0
        BlurBlend         mBlendedBlurData;                         // :308  +0xA30
        DepthOfFieldBlend mBlendedDepthOfFieldData;                 // :309  +0xAA0
        TintData2dBlend   mBlendedTint2DData;                       // :310  +0xAC0
        TintDataBlend     mBlendedTint3DData;                       // :311  +0xAE0
        f32               mfMaximumBloomWeight;                     // :313  +0xAEC
        f32               mfMaximumVignetteWeight;                  // :314  +0xAF0
        f32               mfMaximumBlurWeight;                      // :315  +0xAF4
        f32               mfMaximumDepthOfFieldWeight;              // :316  +0xAF8
        f32               mfMaximum2DTintWeight;                    // :317  +0xAFC
        f32               mfMaximum3DTintWeight;                    // :318  +0xB00  (0xB10 per blender on the console)
    };
}

#endif

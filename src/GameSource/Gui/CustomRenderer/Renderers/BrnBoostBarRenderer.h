#ifndef BRN_BOOST_BAR_RENDERER_H
#define BRN_BOOST_BAR_RENDERER_H

// ============================================================================
// b5-decomp/src/GameSource/Gui/CustomRenderer/Renderers/BrnBoostBarRenderer.h
//
// ⭐⭐ BrnGui::BoostBarRenderer -- THE BOOST BAR (manager slot 4, E_BOOSTBAR): the
// in-game HUD gauge with the tiled background, the fire-body fill, the boosting
// flame/fireball/black-smoke billboard effects, the chained-boost ("x2"/"x3")
// multiplier flame, the earn-flame flicker, the danger-boost end glow, and the
// chunk-loss shatter (the bar tearing into 4x6 shards). Reconstructed WHOLE
// 2026-08-24, replacing the boot-trace minimal slice (ctor + the two debug colour
// getters), from:
//   - the DecFIGS DWARF layout + method set (references/DecFIGS/dwarfdump/
//     GameSource/Gui/CustomRenderer/Renderers/BrnBoostBarRenderer.h) -- every
//     member name below is the DWARF's;
//   - the X360 ARTIST bodies (addresses below) -- the behavioural spine;
//   - the PS3 DecFIGS bodies (every method is a named standalone export there,
//     including the six the X360 set lacks: Destruct, GetRenderLayer,
//     DetermineBoostBarMultiplier, CalculateBoostShardLifetime/Transformation,
//     ForceSetBoostBarColours(EBoostType)) -- shape/naming, X360-gated;
//   - the class constants' VALUES, recovered from the X360 dynamic-initialiser
//     region (0x82C48F70..0x82C5A44C) by emulating its straight-line stores into
//     the 0x82FBxxxx constant block, cross-validated against the PS3
//     __static_initialization_and_destruction_0 literals.
//
// The X360 function set (ledger names):
//   ctor 0x827DF4F0            Construct 0x8245A9A0      Prepare 0x82451B28
//   Release 0x82446818         InitResources 0x8244A508  Update 0x82451C78
//   RecvEvent 0x8244A218       HandleFirstEvent 0x824468D8
//   ForceSetBoostBarColours(Vector3,Vector3) 0x82446970
//   ForceSetBoostBarColours(EBoostType) 0x8244B450 (unnamed sub_ in the export)
//   GetID 0x824468C0           GetInner/GetOuterBoostBarColour 0x824EC750/0x824EC7D0
//   RenderComponent 0x82466638 RenderQuad 0x8245AE30     RenderFire 0x82452AD8
//   RenderBillboardBar 0x82453318                        RenderShatteredBar 0x82460630
//   CalculateShardVertices 0x8244B248                    CalculateBoostShardAlpha 0x8244B3B0
//   SetChainedInactiveMask 0x824536A8                    SetBackground 0x8245B040
//   ShowDebugScreen 0x82461250 RenderDebugFireBody/Overlay/EndCap/Glow
//   0x82453758 / 0x82453B60 / 0x82454060 / 0x8245B2C0
//
// LAYOUT: the X360 object spans ~53 KB (the ctor/Construct byte map is annotated
// per member below as "guest +N"); all access here is by name, x64 offsets differ.
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                                        // Vector3/Vector4 (rw::math::vpu), Matrix44Affine
#include "GameShared/GameClasses/Gui/View/CustomRenderer/CgsCustomRenderer.h"      // CustomRenderComponentInterface base, ImRendererSet, eCustomRenderLayer
#include "GameShared/GameClasses/Gui/View/ParticleSystem2d/CgsBillboardRenderer.h" // BillboardRenderer (six by-value members)
#include "GameShared/GameClasses/Gui/View/CustomRenderer/CgsGuiBillboardInfo.h"    // BillboardInfo (the 32-slot collect array)
#include "GameShared/GameClasses/Containers/CgsArray.h"                            // Array<BillboardInfo,32> (global-namespace template)
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasic2dColouredTexturedVertex.h" // CgsGraphics::Vector2 (the shard lattice)
#include "GameShared/GameClasses/Numeric/CgsRandom.h"                              // CgsNumeric::Random (shard velocity/rotation rolls)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                                    // GuiEventBoostInfo (two by-value members)
#include "GameSource/Gui/CustomRenderer/Renderers/BrnInterpolator.h"               // Interpolator<f32> / DeltaInterpolator
#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostType.h" // BrnWorld::EBoostType
#include "rw/rwcore_structs.h"                                                     // rw::Resource / rw::IResourceAllocator

namespace renderengine { class TextureState; class BlendState; }
namespace CgsGraphics  { struct Im2d; template <typename V> struct ImRenderBuffer; }
// The shard transform is FPU-side on the console (RenderShatteredBar's DWARF); the affine
// matrix template is referenced by reference only here.
namespace rw { namespace math { namespace fpu {
template <typename T> class Vector4Template;
} } }

namespace BrnGui
{
    class GuiCache;

    class BoostBarRenderer : public CgsGui::CustomRenderComponentInterface
    {
    public:
        // DWARF h:178 / h:186 -- the prepare/release stage machines.
        enum EPrepareStage
        {
            E_PREPARESTAGE_START = 0,
            E_PREPARESTAGE_LOAD  = 1,
            E_PREPARESTAGE_INIT  = 2,
            E_PREPARESTAGE_DONE  = 3,
        };
        enum EReleaseStage
        {
            E_RELEASESTAGE_START = 0,
            E_RELEASESTAGE_DONE  = 1,
        };

        // DWARF h:357 -- the show/hide fade state HandleFirstEvent seeds and Update drives.
        enum EVisibilityStatus
        {
            E_VISIBILITY_NONE       = 0,
            E_VISIBILITY_FADING_OUT = 1,
            E_VISIBILITY_FADING_IN  = 2,
            E_VISIBILITY_FULL       = 3,
            E_VISIBILITY_COUNT      = 4,
        };

        // DWARF h:367 -- which bar look the current boost type/state selects.
        enum EBoostBarStatus
        {
            E_STATUS_INVALID               = 0,
            E_STATUS_DANGER_BOOST_INACTIVE = 1,
            E_STATUS_DANGER_BOOST_ACTIVE   = 2,
            E_STATUS_AGGRESSION_BOOST      = 3,
            E_STATUS_STUNT_BOOST           = 4,
        };

        // DWARF h:376 -- the chained-boost multiplier flame (x1 none / x2 / x3).
        enum EBoostBarMultiplier
        {
            E_MULTIPLIER_1X    = 0,
            E_MULTIPLIER_2X    = 1,
            E_MULTIPLIER_3X    = 2,
            E_MULTIPLIER_COUNT = 3,
        };

        // X360 ctor @0x827DF4F0: the interpolators construct (sentinel time keys), the thirteen
        // texture Resource slots seed the shared empty-resource sentinel / zero, the six billboard
        // renderers clear their bookkeeping tails, and miBoostBarPM = -1.
        BoostBarRenderer();

        // ---- the CustomRenderComponentInterface virtuals -------------------------------
        virtual void  Construct();                                                  // 0x8245A9A0
        virtual bool  Prepare(CgsGui::GuiEventQueueSmall* lpEventQueue,
                              rw::IResourceAllocator* lpHeapAllocator,
                              rw::IResourceAllocator* lpTextureAllocator);          // 0x82451B28
        virtual bool  Release();                                                    // 0x82446818
        virtual void  Destruct();                                                   // PS3 0x3F9F84 (base Destruct only)
        virtual void  RecvEvent(const CgsModule::Event* lpEvent, s32 liEventType);  // 0x8244A218
        virtual void  Update();                                                     // 0x82451C78
        virtual CgsID GetID() const;                                                // 0x824468C0
        // The bar draws in LAYER 2, over the movie (PS3 0x43F138; the X360 body is the ICF'd
        // `return 2` the component vtable's +0x20 slot points at).
        virtual CgsGui::eCustomRenderLayer GetRenderLayer() const { return CgsGui::E_CUSTOMRENDERLAYER_2; }

        // ---- the colour override API (the GuiCustRendererDebugComponent drives these) ---
        // 0x8244B450 -- select the per-type constant pair and set ALL THREE slots of each array.
        void ForceSetBoostBarColours(BrnWorld::EBoostType leType);
        // 0x82446970 -- set all three outer slots to lv3OuterColour and all three inner slots to
        // lv3InnerColour (the X360 stores arg2 to +32/48/64 [outer] and arg1 to +80/96/112
        // [inner]; PS3 passes (outer, inner) -- so arg order is (inner, outer)? No: the X360
        // vector args are v1=first,v2=second and it stores v1->inner, v2->outer, while the PS3
        // EBoostType wrapper loads OUTER into the first slot. The X360 asm arbitrates: the FIRST
        // vector parameter lands in the INNER slots).
        void ForceSetBoostBarColours(Vector3 lv3InnerColour, Vector3 lv3OuterColour);

        // 0x824EC750 / 0x824EC7D0 -- the boost-type-indexed colour, by value; both assert
        // meCurrentBoostType is a real type first. Non-const per the DWARF (h:639/h:656).
        Vector3 GetInnerBoostBarColour();
        Vector3 GetOuterBoostBarColour();

        // h:670 -- flip the debug screen (the GuiCustRendererDebugComponent's menu toggle).
        void ToggleDebugScreen() { mbShowDebugScreen = !mbShowDebugScreen; }

    protected:
        // ---- the render spine (all driven from RenderComponent) -------------------------
        virtual void RenderComponent(CgsGui::ImRendererSet* lpRendererSet);         // 0x82466638

    private:
        // The 2D command buffer every render path records into -- the ImRendererSet's slot-0
        // buffer (the console reaches it as **(this+1468); the PS3 signatures spell it
        // CgsGraphics::Im2dRenderBuffer*, whose command stream on this build is the
        // ImRenderBuffer<Basic2dColouredTexturedVertex> the Apt/GUI dispatch consumes).
        typedef CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex> Im2dCommandBuffer;

        void InitResources();                                                       // 0x8244A508
        // PS3 export: RenderFire(Vector4 const& rect, Vector4 const& colour, float timeNow).
        void RenderFire(const Vector4& lv4Rect, const Vector4& lv4Colour,
                        f32 lfTimeNow);                                             // 0x82452AD8
        // PS3 export: RenderQuad(Vector4 const& rect, Vector4 const& colour,
        // TextureState const*, BlendState const*, Vector4 uvRect BY VALUE) -- the UV window
        // {u0,v0,u1,v1} rides the vector-register argument, the colour is the r5 reference.
        void RenderQuad(const Vector4& lv4Rect, const Vector4& lv4Colour,
                        const renderengine::TextureState* lpTextureState,
                        const renderengine::BlendState* lpBlendState,
                        Vector4 lv4UVRect);                                         // 0x8245AE30
        void RenderBillboardBar(const Vector4& lv4Rect, f32 lfProportion,
                                const Vector4& lv4Colour,
                                CgsGui::BillboardRenderer* lpBillboardRenderer,
                                f32 lfTime);                                        // 0x82453318
        void RenderShatteredBar(const rw::math::fpu::Vector4Template<f32>& lv4Rect,
                                f32 lfTime, const Vector4& lv4Colour);              // 0x82460630
        void CalculateShardVertices();                                              // 0x8244B248
        // (CalculateBoostShardTransformation, PS3 0x401668, is NOT declared here: the X360
        // target has no standalone -- RenderShatteredBar open-codes its own per-triangle
        // transform (centroid-relative rotate + velocity offset, transcribed below), and the
        // PS3 standalone is an uncalled sibling whose column derivation even divides by 48
        // where the open-coded path divides by 4. Decode preserved in the .cpp note.)
        f32  CalculateBoostShardLifetime(s32 liShard, f32 lfTime);                  // PS3 0x3FA070
        u8   CalculateBoostShardAlpha(f32 lfLifetime, f32 lfAlpha);                 // 0x8244B3B0
        void SetChainedInactiveMask(Im2dCommandBuffer* lpRenderBuffer,
                                    Vector4 lv4Rect);                               // 0x824536A8
        // PS3 export: SetBackground(Im2dRenderBuffer*, Vector4 rect, float alpha, float timeNow).
        void SetBackground(Im2dCommandBuffer* lpRenderBuffer, Vector4 lv4Rect,
                           f32 lfAlpha, f32 lfTimeNow);                             // 0x8245B040
        void DetermineBoostBarMultiplier();                                         // PS3 0x3FA100
        void HandleFirstEvent(const GuiEventBoostInfo* lpBoostBarInfo);             // 0x824468D8
        void ShowDebugScreen();                                                     // 0x82461250
        // The four debug-screen fire tiles (PS3 manglings: (rect, colour, time) for the first
        // three; Glow takes (rect, colour) only -- and its colour parameter is dead on both
        // consoles).
        void RenderDebugFireBody(const Vector4& lv4FireRect, const Vector4& lv4FireColour,
                                 f32 lfTimeNow);                                    // 0x82453758
        void RenderDebugFireOverlay(const Vector4& lv4FireRect, const Vector4& lv4FireColour,
                                    f32 lfTimeNow);                                 // 0x82453B60
        void RenderDebugFireEndCap(const Vector4& lv4FireRect, const Vector4& lv4FireColour,
                                   f32 lfTimeNow);                                  // 0x82454060
        void RenderDebugFireGlow(const Vector4& lv4FireRect,
                                 const Vector4& lv4FireColour);                     // 0x8245B2C0

        // DWARF h:273-275 -- the chunk-loss shard grid.
        static const s8  KI_CHUNK_LOSS_NUM_OF_SHARD_ROWS    = 4;
        static const s8  KI_CHUNK_LOSS_NUM_OF_SHARD_COLUMNS = 6;
        static const s32 KI_CHUNK_LOSS_MAX_NUM_SHARDS       = 48;
        // DWARF h:593 -- the billboard collect array capacity.
        static const s32 KI_NUM_BILLBOARDS = 32;

        // ---- the render-half class constants (DWARF class statics; the consoles initialise
        // them dynamically -- every VALUE below is the PS3 unity static-init literal, with the
        // four multiplier UV windows additionally verified against the X360 BSS-initialiser
        // emulation; definitions at the top of the .cpp) --------------------------------------
        static const f32 KF_BOOSTING_FLAME_X_SCALE;       // 0.1
        static const f32 KF_BOOSTING_FLAME_Y_SCALE;       // 3.0
        static const f32 KF_BOOSTING_FLAME_X_OFFSET;      // 0.04
        static const f32 KF_BOOSTING_FLAME_Y_OFFSET;      // -0.03
        static const f32 KF_FIRE_MASK_WIDTH;              // 1.0
        static const f32 KF_GROW_FIREBALL_X_SIZE;         // 0.12
        static const f32 KF_GROW_FIREBALL_Y_SCALE;        // 3.5
        static const f32 KF_GROW_FIREBALL_X_OFFSET;       // 0.0
        static const f32 KF_BLACK_SMOKE_X_SIZE;           // 0.12
        static const f32 KF_BLACK_SMOKE_Y_SCALE;          // 2.8
        static const f32 KF_BLACK_SMOKE_X_OFFSET;         // 0.12
        static const f32 KF_BLACK_SMOKE_Y_OFFSET;         // -0.01
        static const f32 KF_EARN_FLAME_WIDTH;             // 0.2
        static const f32 KF_EARN_FLAME_X_OFFSET;          // -0.015
        static const f32 KF_EARN_FLAME_Y_SCALE;           // 2.5
        static const f32 KF_EARN_FLAME_FLICKER_PROP;      // 0.5
        static const f32 KF_BACKGROUND_ENDCAP_WIDTH;      // 0.02
        static const f32 KF_BACKGROUND_ENDCAP_YSCALE;     // 1.1
        static const f32 KF_BACKGROUND_ENDCAP_XOFFSET;    // -0.013
        static const f32 KF_FIRE_BODY_X_SIZE;             // 0.12
        static const f32 KF_FIRE_BODY_X_OFFSET;           // -0.01
        static const f32 KF_FIRE_BODY_Y_SCALE;            // 3.1
        static const f32 KF_FIRE_BODY_Y_OFFSET;           // -0.008
        static const f32 KF_FIRE_BODY_MASK_WIDTH;         // 1.0
        static const f32 KF_FIRE_BODY_ENDCAP_X_SIZE;      // 0.07
        static const f32 KF_FIRE_BODY_ENDCAP_OFFSET;      // -0.015
        static const f32 KF_FIRE_BODY_ENDCAP_FEATHER;     // 0.02
        static const f32 KF_DANGER_END_GLOW_WIDTH;        // 0.1
        static const f32 KF_AGRESSION_BOOST_TRANSPARENCY; // 0.75 (DWARF's own spelling)
        static const f32 KF_BOOSTING_GLOW_INTENSITY;      // 0.3 (X360 rodata 0x82054EE4)
        static const f32 KF_SHAKE_X;                      // 0.003 (RenderComponent's boosting jitter)
        static const f32 KF_SHAKE_Y;                      // 0.005
        static const f32 KF_BACKGROUND_TILE_WIDTH;        // 0.05 (the background cell / multiplier strip width)
        static const Vector4 KV4_GLOW_COLOUR;             // {0.2, 0.2, 0.2, 1}
        static const Vector4 KV4_OVERLAY_COLOUR;          // {1, 1, 1, 1}
        static const Vector4 KV4_FIREBALL_COLOUR;         // {1, 1, 1, 1}
        // The multiplier texture is a 2x2 atlas: image frames on the top row, mask frames on
        // the bottom; "x2" left column, "x3" right (X360 BSS 0x82FB3210/0x82FB3460/
        // 0x82FB34D0/0x82FB2FE0).
        static const Vector4 KV4_MULTIPLIER_2X_IMAGE_UV;  // {0,   0,   0.5, 0.5}
        static const Vector4 KV4_MULTIPLIER_3X_IMAGE_UV;  // {0.5, 0,   1,   0.5}
        static const Vector4 KV4_MULTIPLIER_2X_MASK_UV;   // {0,   0.5, 0.5, 1}
        static const Vector4 KV4_MULTIPLIER_3X_MASK_UV;   // {0.5, 0.5, 1,   1}

        // ---- members (DWARF order; guest byte offsets from the ctor/Construct maps) -----
        EPrepareStage        mePrepareStage;                 // guest +8
        EReleaseStage        meReleaseStage;                 // guest +12
        EBoostBarStatus      meBoostBarStatus;               // guest +16
        EBoostBarMultiplier  meBoostBarMultiplier;           // guest +20
        f32                  mfLastTime;                     // guest +24

        Vector3              mav3BoostOuterColours[3];       // guest +32  (danger/aggression/stunt)
        Vector3              mav3BoostInnerColours[3];       // guest +80
        BrnWorld::EBoostType meCurrentBoostType;             // guest +128 (Construct seeds AGGRESSION)

        GuiEventBoostInfo    mGuiEventBoostInfo;             // guest +132 (the live event-206 payload)
        GuiEventBoostInfo    mPreviousGuiEventBoostInfo;     // guest +160 (last frame's payload)
        f32                  mfIsEarningBoostStartTime;      // guest +188
        f32                  mfIsBoostingProp;               // guest +192
        f32                  mfSlamGainStartTime;            // guest +196
        f32                  mfSlamLossStartTime;            // guest +200

        Interpolator<f32>    mChunkGainInterpolator;         // guest +204
        f32                  mfChunkGainPreviousMaxBoost;    // guest +220
        f32                  mfChunkGainShakeStartTime;      // guest +224

        f32                  mfChunkLossStartTime;           // guest +228
        f32                  mfChunkLossEndTime;             // guest +232
        f32                  mfChunkLossPreviousMaxBoost;    // guest +236
        // The shatter grid: 5x7 vertex lattice (4x6 shards) positions/UVs + per-shard motion.
        // (DWARF spells the element "Basic2dColouredVertex::Vector2" == the CgsGraphics::Vector2
        // the 2D coloured vertex embeds.)
        CgsGraphics::Vector2 mv2VertexPos[5][7];              // guest +240
        CgsGraphics::Vector2 mv2VertexTex[5][7];              // guest +520
        CgsGraphics::Vector2 mav2ChunkLossShardVelocities[48];// guest +800
        f32                  mafChunkLossShardRotations[48]; // guest +1184

        DeltaInterpolator    mBoostFlameInterpolator;        // guest +1376 (Construct SetRange(0,1))
        Interpolator<f32>    mBoostAmountInterpolator;       // guest +1396
        Interpolator<f32>    mBoostGainInterpolator;         // guest +1412
        Interpolator<f32>    mChainedBoostInterpolator;      // guest +1428
        Interpolator<f32>    mVisibilityInterpolator;        // guest +1444
        EVisibilityStatus    meVisibilityFadeState;          // guest +1460

        rw::IResourceAllocator* mpHeapAllocator;             // guest +1464
        CgsGui::ImRendererSet*  mpImRenderers;               // guest +1468
        bool                 mbFirstFrame;                   // guest +1472
        bool                 mbCameraTransitionInProgress;   // guest +1473
        f32                  mfCameraTransitionStopTime;     // guest +1476

        // The thirteen texture states InitResources builds (guest 24-byte {Resource, ptr} pairs
        // from +1480; the ctor seeds the first four Resources with the shared empty-resource
        // sentinel and zero-fills the other nine).
        rw::Resource               mWhiteTextureStateResource;          // guest +1480
        renderengine::TextureState* mpWhiteTextureState;                // guest +1500
        rw::Resource               mMaskTextureStateResource;           // guest +1504
        renderengine::TextureState* mpMaskTextureState;                 // guest +1524
        rw::Resource               mBackgroundTextureStateResource;     // guest +1528
        renderengine::TextureState* mpBackgroundTextureState;           // guest +1548
        rw::Resource               mBackgroundEndCapTextureStateResource; // guest +1552
        renderengine::TextureState* mpBackgroundEndCapTextureState;     // guest +1572
        rw::Resource               mFireBodyTextureStateResource;       // guest +1576
        renderengine::TextureState* mpFireBodyTextureState;             // guest +1596
        rw::Resource               mFireOverTextureStateResource;       // guest +1600
        renderengine::TextureState* mpFireOverTextureState;             // guest +1620
        rw::Resource               mEndCapTextureStateResource;         // guest +1624
        renderengine::TextureState* mpEndCapTextureState;               // guest +1644
        rw::Resource               mEarnFlameTextureStateResource;      // guest +1648
        renderengine::TextureState* mpEarnFlameTextureState;            // guest +1668
        rw::Resource               mEndGlowTextureStateResource;        // guest +1672
        renderengine::TextureState* mpEndGlowTextureState;              // guest +1692
        rw::Resource               mBoostingFlameTextureStateResource;  // guest +1696
        renderengine::TextureState* mpBoostingFlameTextureState;        // guest +1716
        rw::Resource               mGrowFireballTextureStateResource;   // guest +1720
        renderengine::TextureState* mpGrowFireballTextureState;         // guest +1740
        rw::Resource               mMultiplierTextureStateResource;     // guest +1744
        renderengine::TextureState* mpMultiplierTextureState;           // guest +1764
        rw::Resource               mGlowTextureStateResource;           // guest +1768
        renderengine::TextureState* mpGlowTextureState;                 // guest +1788

        // DWARF h:594 -- the six billboard effect renderers (boosting flame, grow fireball,
        // black smoke, earn flame, multiplier flame, end glow -- bound by InitResources).
        CgsGui::BillboardRenderer mBillboardRenderer[6];     // guest +1808 (stride 8268)

        // DWARF h:595 -- the per-pass billboard collect array (global-namespace Array<T,N>;
        // the explicit instantiation lives in Array_BillboardInfo_32.cpp).
        Array<CgsGui::BillboardInfo, 32> maBillboards;       // guest +51416

        // DWARF h:597 -- the shard velocity/rotation roll source (Construct seeds it).
        CgsNumeric::Random   mRandom;                        // guest +53456

        GuiCache*            mpGuiCache;                     // guest +53520 (event 64 publishes it)
        bool                 mbShowDebugScreen;              // guest +53524
        s32                  miBoostBarPM;                   // guest +53528 ("BoostBar" perfmon handle)
    };

// BrnGui::GuiCustRendererDebugComponent -- the debug-menu component that lets a developer override
// the boost-bar colours at runtime. It edits the colours of the live BoostBarRenderer and mirrors
// the chosen inner/outer colours into its own cached fields so the debug menu can display them.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   GuiCustRendererDebugComponent::UpdateBoostColours @ 0x824F7C48
//
// MINIMAL-SLICE class. Only UpdateBoostColours is in scope; the X360 object is large (the full
// CgsDev::DebugComponent base, the per-boost-type menu-variable registrations, callbacks, etc. are
// all uncommitted and OMITTED). The guest reaches the live BoostBarRenderer through a renderer/cache
// pointer at +0xC plus the +0xE520 BoostBarRenderer subobject offset; on the 64-bit host the offset
// is not load-bearing, so the resolved BoostBarRenderer pointer is held directly and accessed BY NAME
// (no raw +0xE520 cast). The two cached colour triples are the destination fields the asm writes:
// outer RGB at guest +0x18/+0x1C/+0x20, inner RGB at guest +0x24/+0x28/+0x2C.  FLAG: minimal-slice
// class + member-pointer models the resolved BoostBarRenderer (not the +0xC owner + +0xE520 offset).
class GuiCustRendererDebugComponent
{
public:
    // 0x824F7C48 -- pull the boost-type-indexed inner/outer colours out of the live BoostBarRenderer
    // and cache them into the component's own colour fields. Returns the GetOuterBoostBarColour result
    // (the X360 leaves the outer-getter's return value in r3).
    int UpdateBoostColours();

private:
    // The live boost-bar renderer this component edits (guest: a renderer/cache pointer at +0xC whose
    // BoostBarRenderer subobject is at +0xE520; held resolved here for by-name access).
    BoostBarRenderer* mpBoostBarRenderer;   // guest +0x0C (resolved to the +0xE520 subobject)

    // Cached colour triples mirrored from the renderer (the asm's tail stores).
    Vector3 mv3OuterColour;   // guest +0x18/+0x1C/+0x20 (x/y/z)
    Vector3 mv3InnerColour;   // guest +0x24/+0x28/+0x2C (x/y/z)
};
}

#endif // BRN_BOOST_BAR_RENDERER_H

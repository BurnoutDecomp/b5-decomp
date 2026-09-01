// ============================================================================
// BrnCrashNavIconRenderer.cpp -- BrnGui::CrashNavIconRenderer, the CORE half:
// lifecycle (ctor / Construct / Prepare / Release / Destruct / SetRenderEnabled / GetID),
// resources (InitResources / InitEventTypeUvs) and the event drain (RecvEvent).
//
// The RENDER half (RenderComponent + the Render* family, GetNumIcons /
// GetIconInformation / CalculateUVsForIndex / GetActiveIconType / IsIgnoredIcon) lives in
// the sibling TU BrnCrashNavIconRenderer_wK_01.cpp against the SAME header -- see the
// HEADER CONTRACT block at the top of BrnCrashNavIconRenderer.h.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (Jan-2008):
//   CrashNavIconRenderer (ctor) @0x827E0B28   Construct        @0x82463520
//   Prepare                     @0x82463848   Release          @0x824470A8
//   Destruct                    @0x824470C8   GetID            @0x824470D8
//   SetRenderEnabled            @0x827E0CA0   RecvEvent        @0x82456168
//   InitResources               @0x8245CD20   InitEventTypeUvs @0x824566F0
//
// SOURCE OF TRUTH: the X360 pseudocode + asm is authoritative for behaviour; the DWARF
// (references/DecFIGS/.../BrnCrashNavIconRenderer.h) gives the declaration shapes/names.
//
// ASSERTS: the console builds several of these through CgsDev::Assert::gpcMessageBuffer +
// StrStream. Per the tree convention they are lowered to CGS_ASSERT with the recovered
// literal expression; the console's own BrnCrashNavIconRenderer.cpp line number is kept in
// a trailing comment (CGS_ASSERT stamps __FILE__/__LINE__ of THIS file).
//
// SEMANTIC-LEVEL SIMD: Construct's Im2dTransform seed is built on the X360 with
// lvx128/vperm/vsldoi/stvx128 into four 16-byte lanes. Those intrinsics have no portable
// PC equivalent, so the four lanes are written by name with the same values (the lane
// contents are unambiguous -- see the comment on the seed).
// ============================================================================

#include "GameSource/Gui/CustomRenderer/Renderers/BrnCrashNavIconRenderer.h"

#include "GameSource/Gui/BrnGuiCache.h"                          // BrnGui::GuiCache (resource pump + GetTime)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h" // sResourceTuple / GuiEventLoadNotification
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"  // CgsLanguage::LanguageManager
#include "GameShared/GameClasses/Fonts/CgsFont.h"                // CgsResource::Font (macTypefaceFamilyName)
#include "GameShared/GameClasses/Core/CgsAssert.h"               // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                   // CgsID
#include "GameShared/GameClasses/Development/Log/CgsLog.h"       // CgsDev::Log / CgsDev::Message
#include "pc/gcm/renderengine/renderstates.h"                    // renderengine::TextureState (+Parameters) / Texture

#include <cstring>   // _strnicmp / strlen

namespace BrnGui
{
namespace
{
    // -----------------------------------------------------------------------
    // File-local data tables (X360 .rdata / cinit).
    // -----------------------------------------------------------------------

    // maResourcesToLoad / muNumResourcesToLoad (DWARF cpp:151 / cpp:164), X360
    // @0x82F25868 (six 8-byte tuples) with the count word immediately after at
    // @0x82F25898 -- which is exactly &maResourcesToLoad[6], and which is what
    // InitResources' ">= E_CRASHNAVICON_NUM" assert reads.
    //
    // The IDS are attested, not guessed: InitResources walks the first TWO tuples for the
    // two icon atlases (the loop's end pointer is the constant-folded &table[2]) and then
    // fetches 206 / 203 / 202 / 235 by literal id, and Prepare's stage-LOAD arm hands the
    // WHOLE six-entry table to GuiCache::EnsureResourcesAreLoaded -- so the six ids the
    // pump is asked for are exactly the six the initialiser consumes. Type 11 is
    // E_GUI_RESOURCETYPE_TEXTURE (GUITEXTURES.BIN), matching every id below:
    //   204 Icons_EventIcon_NotAttempted_Anim   205 Icons_EventIcon_Completed_Anim
    //   206 Icons_CrashNavIcon                  203 MainMapBackgroundMask
    //   202 PreRaceBackgroundMask               235 the road-sign plate atlas
    // FLAG: the ORDER of the four trailing tuples is not independently attested (only the
    // set is); nothing reads them by index, so the order is inert.
    const u32 KU_RESOURCETYPE_TEXTURE = 11u;   // E_GUI_RESOURCETYPE_TEXTURE

    const CgsGui::sResourceTuple KA_RESOURCES_TO_LOAD[6] =
    {
        { 204u, static_cast<CgsGui::ResourceRequestTypes>(KU_RESOURCETYPE_TEXTURE) },
        { 205u, static_cast<CgsGui::ResourceRequestTypes>(KU_RESOURCETYPE_TEXTURE) },
        { 206u, static_cast<CgsGui::ResourceRequestTypes>(KU_RESOURCETYPE_TEXTURE) },
        { 203u, static_cast<CgsGui::ResourceRequestTypes>(KU_RESOURCETYPE_TEXTURE) },
        { 202u, static_cast<CgsGui::ResourceRequestTypes>(KU_RESOURCETYPE_TEXTURE) },
        { 235u, static_cast<CgsGui::ResourceRequestTypes>(KU_RESOURCETYPE_TEXTURE) },
    };
    const u32 KU_NUM_RESOURCES_TO_LOAD = 6u;   // dword_82F25898

    // The five texture ids InitResources fetches by literal.
    const u32 KU_TEXTURE_ID_CRASHNAV_ICONS   = 206u;
    const u32 KU_TEXTURE_ID_BACKGROUND_MASK  = 203u;
    const u32 KU_TEXTURE_ID_PRERACE_MASK     = 202u;
    const u32 KU_TEXTURE_ID_ROAD_SIGNS       = 235u;

    // -----------------------------------------------------------------------
    // Atlas geometry (X360 .rdata flt_8205502C..flt_8205505C, DWARF cpp:29/35/41/47/85/91).
    // Indexed by ECrashNavIconType (0 = not-attempted atlas id 204, 1 = completed id 205).
    //
    // ⚠️ FLAG (values, not algorithm): the .rdata float CONTENTS are not in the symbol
    // export (the data segment is not dumped), so the six pairs below are carried over
    // from the sibling BrnSatNavRenderer's H3b image read of ITS atlas constants. That
    // carry-over is justified -- the sat-nav renderer indexes THE SAME two texture ids
    // (204/205) through the same 2-entry table -- and it is self-consistent with this
    // renderer's own usage (InitEventTypeUvs addresses frame indices up to 11, which needs
    // a 4x4 grid of 64px cells in a 256px page). The INDEXING/zoom algorithm below is
    // X360-proven; only these twelve floats are unverified against this build's .rdata.
    // -----------------------------------------------------------------------
    const f32 KAF_TEXTURE_WIDTH   [CrashNavIconRenderer::E_CRASHNAVICON_NUM] = { 256.0f, 256.0f }; // flt_8205502C
    const f32 KAF_TEXTURE_HEIGHT  [CrashNavIconRenderer::E_CRASHNAVICON_NUM] = { 256.0f, 256.0f }; // flt_82055034
    const f32 KAF_ICON_WIDTH      [CrashNavIconRenderer::E_CRASHNAVICON_NUM] = {  64.0f,  64.0f }; // flt_8205503C
    const f32 KAF_ICON_HEIGHT     [CrashNavIconRenderer::E_CRASHNAVICON_NUM] = {  64.0f,  64.0f }; // flt_82055044
    const f32 KAF_MINI_ICON_WIDTH [CrashNavIconRenderer::E_CRASHNAVICON_NUM] = {  32.0f,  32.0f }; // flt_82055054
    const f32 KAF_MINI_ICON_HEIGHT[CrashNavIconRenderer::E_CRASHNAVICON_NUM] = {  32.0f,  32.0f }; // flt_8205505C

    // DWARF cpp:134 -- the V offset the mini-icon rows sit at inside the same page. The
    // X360 adds this literal to each generated mini-icon V (InitEventTypeUvs @0x824566F0).
    const f32 KF_MINI_ICON_TEXTURE_OFFSET = 0.75f;

    // The event-type column -> atlas frame map InitEventTypeUvs' switch encodes. Columns 4
    // and 5 are DELIBERATELY crossed on the console (column 4 -> frame 6, column 5 ->
    // frame 5); everything else is column + 1.
    const s32 KAI_EVENTTYPE_TO_ICON_FRAME[CrashNavIconRenderer::KU_ICON_EVENT_TYPE_COUNT] =
    { 1, 2, 3, 4, 6, 5, 7, 8, 9, 10, 11 };

    // -----------------------------------------------------------------------
    // Event-type ids the renderer drains (the switch arms of RecvEvent @0x82456168).
    // -----------------------------------------------------------------------
    enum
    {
        KI_EVENT_LOAD_NOTIFICATION      = 14,   // 0x00E  adopt the road-sign font
        KI_EVENT_SET_CACHE              = 64,   // 0x040  latch the GuiCache pointer
        KI_EVENT_RENDER_MAIN_MAP        = 223,  // 0x0DF  the per-frame map view record
        KI_EVENT_DRAW_EVENT_ICONS       = 554,  // 0x22A  display type + fade + ignore list
        KI_EVENT_FILTER_EVENT_ICONS     = 557,  // 0x22D  game-mode filter
        KI_EVENT_SET_INSPECTED_ICON     = 558,  // 0x22E  the inspected event id
        KI_EVENT_SET_HOVERED_ICON       = 559,  // 0x22F  the hovered icon triple
        KI_EVENT_MAP_CURSOR_STATUS      = 560,  // 0x230  cursor position / states
        KI_EVENT_MAP_ICON_STATUS        = 561,  // 0x231  the live rival-icon bank
        KI_EVENT_ROAD_SIGN_ICON_STATUS  = 562   // 0x232  the road-sign bank + scale
    };

    // The load-notification request type the console accepts (FONTDATA); the ticker's
    // case-14 arm uses the identical gate.
    const s32 KI_REQUESTTYPE_FONTDATA = 16;

    // Build one renderengine texture state over the given backing resource. This is the PC
    // fold of the X360's `TextureState::GetResourceDescriptor -> allocator vtable +0x10 ->
    // copy 5 dwords -> TextureState::Initialize` sequence; the sampler words are the
    // console's own (InitResources' stack descriptor: clamp/clamp, linear/linear, mip 2,
    // aniso 13, lod bias 0, trailing flags 0,0,0,1,1). Same helper shape as
    // BrnSatNavRenderer::InitResources.
    renderengine::TextureState* CreateIconTextureState(rw::Resource* lpBacking, const void* lpTexture)
    {
        renderengine::TextureState::Parameters lParams;
        lParams.muAddressU      = 2u;   // v35
        lParams.muAddressV      = 2u;   // v36
        lParams.muAddressW      = 0u;   // v37
        lParams.muMagFilter     = 1u;   // v38
        lParams.muMinFilter     = 1u;   // v39
        lParams.muMipFilter     = 2u;   // v40
        lParams.muField6        = 0u;   // v41
        lParams.muField7        = 0u;   // v42
        lParams.muMaxAnisotropy = 13u;  // v43
        lParams.muField9        = 0u;   // v44
        lParams.muField10       = 1u;   // v45
        lParams.mfMipLodBias    = 0.0f; // v46
        lParams.mfField12       = 0.0f; // v47
        lParams.muField13       = 0u;   // v48
        lParams.muField14       = 0u;   // v49
        lParams.muField15       = 0u;   // v50
        lParams.mu8Field40      = 0u;   // v51
        lParams.mu8Field41      = 0u;   // v52
        lParams.mu8Field42      = 0u;   // v53
        lParams.mu8Field43      = 1u;   // v54
        lParams.mu8Field44      = 1u;   // v55
        lParams.mpTexture       = reinterpret_cast<renderengine::Texture*>(
                                      const_cast<void*>(lpTexture));   // v56
        return renderengine::TextureState::Initialize(lpBacking, &lParams);
    }
}

// ---------------------------------------------------------------------------
// @0x827E0B28 -- the C++ constructor. On the console this is entirely compiler-generated:
// it installs the class vtable (off_820D0034), default-constructs the five renderengine
// resource descriptors (five 5-dword zero runs, one of them a 2-iteration loop over
// maIconResources), marks the Array<u32,5> unconstructed (`stw -1, 0x144`), and runs the
// ten embedded CrashNavMapIcon constructors (eight rivals at +0x1D0 stride 0x1F0, two
// start/finish icons at +0x1160) -- which is what installs off_820CEB64 / off_820CEB40 /
// off_82072F8C into each icon's three vtable slots.
//
// Nothing here is hand-written game logic: on the host the compiler emits the icon
// sub-object constructions and the vtable itself, so the body only has to reproduce the
// two zero-seeds the console's generated ctor performs on POD members.
// ---------------------------------------------------------------------------
CrashNavIconRenderer::CrashNavIconRenderer()
{
    for (s32 li = 0; li < 5; ++li)
    {
        mBackgroundMaskTextureStateResource[li] = 0;   // +0x09C
        mPreRaceMaskTextureStateResource[li]    = 0;   // +0x0B4
        maIconResources[0][li]                  = 0;   // +0x0CC (the 2 x 5 loop)
        maIconResources[1][li]                  = 0;
        mIconsTextureStateResource[li]          = 0;   // +0x0FC
        mRoadSignsTextureStateResource[li]      = 0;   // +0x114
    }

    // `stw r5(-1), 0x144(r3)` -- the Array<u32,5> count word starts on the
    // KI_UNCONSTRUCTED sentinel; Construct() below flips it to 0 before the Appends.
    mOnlineStartpointsToIgnore.MarkUnconstructed();
}

// ---------------------------------------------------------------------------
// @0x82463520 -- Construct. Chains the base, seeds the whole record, builds the five
// online start-points to ignore, constructs the road-sign table and the text object, and
// finally seeds the screen-space text transform.
// ---------------------------------------------------------------------------
void CrashNavIconRenderer::Construct()
{
    CustomRenderComponentInterface::Construct();

    mePrepareStage = E_PREPARESTAGE_START;   // stw 0, 0x54
    meReleaseStage = E_RELEASESTAGE_DONE;    // stw 1, 0x58 -- nothing to release yet

    // `stw r11(0), 0x80(r31)` @0x82463554 -- the one word of the 48-byte mRenderMainMapEvent
    // the console seeds; the rest is overwritten wholesale by the first event 223.
    // ⭐ MEMBER CORRECTED 2026-08-29 (FIX2). This mapped to mbIsActive, but with
    // GuiEventRenderMainMap's committed layout (mv4MapRect +0x60, mv4ViewRect +0x70) the byte
    // at +0x80 is mpActiveTextures and mbIsActive is +0x8C -- the byte RenderIcons reads for
    // its half-alpha arm and which the console never seeds here. As written, a live POINTER
    // member was left as ctor garbage while an unrelated flag got zeroed.
    mRenderMainMapEvent.mpActiveTextures = 0;

    mpGuiCache                   = 0;   // 0x90
    mpHeapAllocator              = 0;   // 0x94
    mpBackgroundMaskTextureState = 0;   // 0xB0
    mpPreRaceMaskTextureState    = 0;   // 0xC8
    mpIconsTextureState          = 0;   // 0x110
    mpRoadSignsTextureState      = 0;   // 0x128

    // The ten-word clear loop from +0xCC: maIconResources[2][5].
    for (s32 li = 0; li < 5; ++li)
    {
        maIconResources[0][li] = 0;
        maIconResources[1][li] = 0;
    }

    mapIconTextureStates[0] = 0;   // 0xF4
    mapIconTextureStates[1] = 0;   // 0xF8

    // stw 5, 0x12C -- "no icon set displayed" until an event 554 arrives.
    meIconDisplayType = GuiEventDrawEventIcons::E_ICON_DISPLAY_TYPE_COUNT;

    // stw 6, 0x3034 -- the game-mode filter's "no filter" seed (RaceEventData::EModeType).
    meGameModeFilter = 6;

    mbRenderEventStarts = false;   // stb 0, 0x34A4

    // The four big UV tables: the console zeroes the FIRST FIVE (u,v) pairs of each with
    // `std r30` (0x3038/0x3040/0x3048/0x3050/0x3058 and the matching runs at +0x30E8 /
    // +0x3198 / +0x3248). It does NOT clear the remaining 17 entries of each table --
    // InitEventTypeUvs overwrites all 22 before the first draw. Reproduced store-for-store.
    for (s32 li = 0; li < 5; ++li)
    {
        mav2IconUvTopLeft[0][li].x     = 0.0f;  mav2IconUvTopLeft[0][li].y     = 0.0f;
        mav2IconUvBottomLeft[0][li].x  = 0.0f;  mav2IconUvBottomLeft[0][li].y  = 0.0f;
        mav2IconUvTopRight[0][li].x    = 0.0f;  mav2IconUvTopRight[0][li].y    = 0.0f;
        mav2IconUvBottomRight[0][li].x = 0.0f;  mav2IconUvBottomRight[0][li].y = 0.0f;
    }

    mbOldTypeFading          = false;                                          // stb 0, 0x34A5
    meFadingIconDisplayType  = GuiEventDrawEventIcons::E_ICON_DISPLAY_TYPE_COUNT; // stw 5, 0x34A8
    mfIconFadeStartTime      = 0.0f;                                           // stfs f31, 0x34AC
    mfIconFadeEndTime        = 0.0f;                                           // stfs f31, 0x34B0

    muInspectedEventID = 0;   // stw 0, 0x148

    // The two 24-byte hover records (std/std/stw/stw at +0x150.. and +0x168..).
    mHoveredEventIcon.mHoveredDriveThroughID = 0;
    mHoveredEventIcon.mHoveredPlayerID       = 0;
    mHoveredEventIcon.muHoveredEventID       = 0;
    mHoveredEventIcon.mpcHoveredRoadName     = 0;
    mHoveredEventIconLastFrame.mHoveredDriveThroughID = 0;
    mHoveredEventIconLastFrame.mHoveredPlayerID       = 0;
    mHoveredEventIconLastFrame.muHoveredEventID       = 0;
    mHoveredEventIconLastFrame.mpcHoveredRoadName     = 0;

    miSelectedIndex           = 0;      // stw 0,    0x180
    mfHoveredIconScaleFactor  = 1.0f;   // stfs f30, 0x184
    mfHoveredIconScaleEndTime = 0.0f;   // stfs f31, 0x188
    mfHoveredIconGrowing      = false;  // stb  0,   0x18C
    mfPlayerIconPulseScale    = 1.0f;   // stfs f30, 0x190
    mfPlayerIconPulseEndTime  = 0.0f;   // stfs f31, 0x194

    // BrnGui::RoadSignList::Construct(this + 0x154C) -- the 65-entry authored table.
    mRoadSignList.Construct();

    mRoadSignIconStatus.mpRoadSignIcons = 0;   // stw 0, 0x1544
    mpTextRenderer                      = 0;   // stw 0, 0x3030

    // CgsGraphics::TextObject::Construct(this + 0x2FB4, 0, 0) -- no alternate colours.
    mTextObject.Construct(0, 0);

    // `stvx128 v0(zero), r31, 0x1A0` clears the cursor record's leading 16-byte lane, then
    // the two state words are seeded. (mfCursorScaleFactor at +0x1C0 is NOT written by the
    // console's Construct -- see the header note.)
    mGuiEventMapCursorStatus.mv2Position.SetZero();
    mGuiEventMapCursorStatus.miDisplayState   = 2;   // stw 2, 0x1B0
    mGuiEventMapCursorStatus.miAnimationState = 4;   // stw 4, 0x1B4

    mGuiEventMapIconStatus.liNumberOfIcons = 0;   // stw 0, 0x1C8
    mGuiEventMapIconStatus.lpSatNavIcons   = 0;   // stw 0, 0x1C4

    // The five online start-point event ids the crash-nav map must never draw an icon for
    // (`stw 0, 0x144` = Array::Construct, then five int_5_::Append calls with the literals
    // 0x514F4 / 0x54B50 / 0x54B51 / 0x54C6E / 0x74AC0).
    mOnlineStartpointsToIgnore.Construct();
    mOnlineStartpointsToIgnore.Append(333044);   // 0x000514F4
    mOnlineStartpointsToIgnore.Append(346960);   // 0x00054B50
    mOnlineStartpointsToIgnore.Append(346961);   // 0x00054B51
    mOnlineStartpointsToIgnore.Append(347246);   // 0x00054C6E
    mOnlineStartpointsToIgnore.Append(477888);   // 0x00074AC0

    // ---- the text transform seed (X360 @0x82463744..0x82463830) ---------------------
    // Four 16-byte lanes are built on the stack and stored to this+0x10/+0x20/+0x30/+0x40
    // (mOriginXYZ / mRightUp / mColourShift / mColourScale), then TransformByAspectRatio
    // folds the display aspect in. The lane contents are unambiguous:
    //   +0x00 origin       (-1, 1, 0, 0)                 flt_820037C8 = -1.0, f30 = 1.0
    //   +0x10 right/up     (0.0015625, -0.0027777778, 0, 0)
    //                      flt_8203A474 = 1/640, flt_8203A478 = -1/360 -- i.e. the
    //                      1280x720 logical screen -> NDC basis (x/640 - 1, 1 - y/360).
    //                      The vperm/vsldoi pair merges lane 0 of one temp with lane 1 of
    //                      the other; that is the ONLY merge producing a usable basis.
    //   +0x20 colour shift (0, 0, 0, 0)
    //   +0x30 colour scale (1, 1, 1, 1)
    const f32 KF_SCREEN_TO_NDC_X =  0.0015625f;     // flt_8203A474 == 1/640
    const f32 KF_SCREEN_TO_NDC_Y = -0.0027777778f;  // flt_8203A478 == -1/360
    mTextTransform.mOriginXYZ.x = -1.0f;
    mTextTransform.mOriginXYZ.y =  1.0f;
    mTextTransform.mOriginXYZ.z =  0.0f;
    mTextTransform.mOriginXYZ.w =  0.0f;
    mTextTransform.mRightUp.x   = KF_SCREEN_TO_NDC_X;
    mTextTransform.mRightUp.y   = KF_SCREEN_TO_NDC_Y;
    mTextTransform.mRightUp.z   = 0.0f;
    mTextTransform.mRightUp.w   = 0.0f;
    mTextTransform.mColourShift.SetZero();
    mTextTransform.mColourScale.x = 1.0f;
    mTextTransform.mColourScale.y = 1.0f;
    mTextTransform.mColourScale.z = 1.0f;
    mTextTransform.mColourScale.w = 1.0f;
    mTextTransform.TransformByAspectRatio();
}

// ---------------------------------------------------------------------------
// @0x82463848 -- the staged bring-up the CustomRendererManager pumps each frame.
// Returns true only once the whole chain is done.
// ---------------------------------------------------------------------------
bool CrashNavIconRenderer::Prepare(CgsGui::GuiEventQueueSmall* lpOutputEventQueue,
                                   rw::IResourceAllocator* lpHeapAllocator,
                                   rw::IResourceAllocator* lpTextureAllocator)
{
    (void)lpTextureAllocator;   // the console never latches the texture allocator here

    switch (mePrepareStage)
    {
    case E_PREPARESTAGE_START:
        mpHeapAllocator = lpHeapAllocator;   // stw r5, 0x94
        CGS_ASSERT(lpOutputEventQueue != 0, "lpOutputEventQueue");   // console line 288
        mpOutputEventQueue = lpOutputEventQueue;   // stw r4, 0x98
        mePrepareStage     = E_PREPARESTAGE_LOAD;
        return false;

    case E_PREPARESTAGE_LOAD:
        // The console tests the cache pointer first and falls through to "not ready" when
        // it is still null -- an assert here would fire every frame before the cache bind.
        if (mpGuiCache != 0 &&
            mpGuiCache->EnsureResourcesAreLoaded(KA_RESOURCES_TO_LOAD, KU_NUM_RESOURCES_TO_LOAD))
        {
            mePrepareStage = E_PREPARESTAGE_INIT;
        }
        return false;

    case E_PREPARESTAGE_INIT:
        InitResources();
        mePrepareStage = E_PREPARESTAGE_DONE;
        return false;

    case E_PREPARESTAGE_DONE:
        mePrepareStage    = E_PREPARESTAGE_DONE;
        meIconDisplayType = GuiEventDrawEventIcons::E_ICON_DISPLAY_TYPE_COUNT;   // stw 5, 0x12C
        return true;

    default:
        // The console streams "Unhandled prepare stage <n> in CrashNavIconRenderer::Prepare"
        // through StrStream; lowered to the static message (console line 321).
        CGS_ASSERT(false, "Unhandled prepare stage in CrashNavIconRenderer::Prepare");
        return false;
    }
}

// ---------------------------------------------------------------------------
// @0x824470A8 -- Release. Drops the cache + allocator bindings and parks the display type.
// Four instructions on the console; it does NOT chain the base.
// ---------------------------------------------------------------------------
bool CrashNavIconRenderer::Release()
{
    mpGuiCache        = 0;   // stw 0, 0x90
    mpHeapAllocator   = 0;   // stw 0, 0x94
    meIconDisplayType = GuiEventDrawEventIcons::E_ICON_DISPLAY_TYPE_COUNT;   // stw 5, 0x12C
    return true;
}

// ---------------------------------------------------------------------------
// @0x824470C8 -- Destruct. A single store; the console does not chain the (empty) base.
// ---------------------------------------------------------------------------
void CrashNavIconRenderer::Destruct()
{
    meIconDisplayType = GuiEventDrawEventIcons::E_ICON_DISPLAY_TYPE_COUNT;   // stw 5, 0x12C
}

// ---------------------------------------------------------------------------
// @0x824470D8 -- the component's CgsID, constant-folded in the asm:
//   lis 0x5BCA / ori 0xAE62 / sldi 32 / oris 0x1C55 / ori 0x22EF
// ---------------------------------------------------------------------------
CgsID CrashNavIconRenderer::GetID() const
{
    return 0x5BCAAE621C5522EFull;
}

// ---------------------------------------------------------------------------
// The two DWARF overrides recovered 2026-08-29 (FIX2) from the component vtable
// off_820D0034 (the ctor @0x827E0B28 names it), read out of the raw image. Neither had an
// addressable ledger symbol because BOTH slots are ICF-folded leaves shared with other
// classes -- which is exactly why the header-first pass could not see them, and why they
// must be recovered from the vtable rather than from func_index. See the slot map in
// BrnCrashNavIconRenderer.h.
//
// Update -- vtable slot 6 -> 0x8284CB38, whose four bytes are `4E 80 00 20` == a bare
// `blr`. An EMPTY body, folded together with this class's own StartFade (slot 11) and
// ClearFadeState (slot 12). Reproduced as written: nothing to do per frame. The
// hover / pulse animation members this class owns are advanced inside the Render* bodies
// that read them (RenderEventIcon / RenderCursor / RenderDriveThrough / RenderRivals), not
// from here -- so the animation is frame-driven by drawing, on the console too.
// ---------------------------------------------------------------------------
void CrashNavIconRenderer::Update()
{
}

// ---------------------------------------------------------------------------
// GetRenderLayer -- vtable slot 8 -> 0x82C296C8, whose eight bytes are
// `38 60 00 01 / 4E 80 00 20` == `li r3,1 ; blr`. The crash-nav map furniture draws in
// LAYER 1, not layer 2 like BrnInGameMessageRenderer / BrnBoostBarRenderer.
//
// This is the same numeric value as the base default, so declaring it changes no behaviour
// today -- but the DWARF declares the override, the slot is a distinct (ICF'd) body, and
// leaving it undeclared is what let the header claim "no opinion" about a value that
// BrnCustomRenderer.cpp:599 gates every draw on.
// ---------------------------------------------------------------------------
CgsGui::eCustomRenderLayer CrashNavIconRenderer::GetRenderLayer() const
{
    return CgsGui::E_CUSTOMRENDERLAYER_1;
}

// ---------------------------------------------------------------------------
// @0x827E0CA0 -- SetRenderEnabled. Beyond the base flag it RESETS the hover/selection
// state so a renderer that is switched back on does not draw a stale highlight.
//
// NOTE for the record: Hex-Rays renders the two `std r11` at +0x150/+0x158 as
// `*(result + 336) = 0x400000000LL`. That is a misread -- r11 is `li r11,0` two
// instructions earlier and every clearing store below uses it; the ONLY non-zero store is
// `stw r10,0x1B4` with `li r10,4`.
// ---------------------------------------------------------------------------
void CrashNavIconRenderer::SetRenderEnabled(bool lbRenderEnabled)
{
    mbRenderEnabled = lbRenderEnabled;   // stb r4, 4(r3)

    mHoveredEventIcon.mHoveredDriveThroughID = 0;   // std r11, 0x150
    mHoveredEventIcon.mHoveredPlayerID       = 0;   // std r11, 0x158
    mHoveredEventIcon.mpcHoveredRoadName     = 0;   // stw r11, 0x164
    mHoveredEventIcon.muHoveredEventID       = 0;   // stw r11, 0x160

    mGuiEventMapIconStatus.liNumberOfIcons = 0;     // stw r11, 0x1C8
    mGuiEventMapIconStatus.lpSatNavIcons   = 0;     // stw r11, 0x1C4
    mRoadSignIconStatus.mpRoadSignIcons    = 0;     // stw r11, 0x1544

    mGuiEventMapCursorStatus.miAnimationState = 4;  // stw r10(=4), 0x1B4
}

// ---------------------------------------------------------------------------
// @0x8245CD20 -- InitResources. Creates the six texture states the renderer draws with,
// in the console's order (the two icon atlases, the crash-nav icon page, the UV tables,
// then the two masks and the road-sign page).
// ---------------------------------------------------------------------------
void CrashNavIconRenderer::InitResources()
{
    CGS_ASSERT(KU_NUM_RESOURCES_TO_LOAD >= static_cast<u32>(E_CRASHNAVICON_NUM),
               "muNumResourcesToLoad >= E_CRASHNAVICON_NUM");   // console line 621

    // The loop's console bound is the constant-folded end pointer &maResourcesToLoad[2],
    // i.e. exactly E_CRASHNAVICON_NUM entries -- NOT muNumResourcesToLoad.
    for (s32 liTextureIndex = 0; liTextureIndex < E_CRASHNAVICON_NUM; ++liTextureIndex)
    {
        const void* const lpTexture =
            mpGuiCache->GetLoadedResource(KA_RESOURCES_TO_LOAD[liTextureIndex].muId);
        CGS_ASSERT(lpTexture != 0, "lpTexture!=NULL");   // console line 631

        maIconResources[liTextureIndex][0] = 0;   // the console's 5-dword descriptor slot
        mapIconTextureStates[liTextureIndex] =
            CreateIconTextureState(&maIconTextureStateBacking[liTextureIndex], lpTexture);
        CGS_ASSERT(mapIconTextureStates[liTextureIndex] != 0,
                   "mapIconTextureStates[liTextureIndex]");   // console line 642
    }

    {
        const void* const lpTexture = mpGuiCache->GetLoadedResource(KU_TEXTURE_ID_CRASHNAV_ICONS);
        CGS_ASSERT(lpTexture != 0, "lpTexture != NULL");   // console line 647
        mIconsTextureStateResource[0] = 0;
        mpIconsTextureState = CreateIconTextureState(&mIconsTextureStateBacking, lpTexture);
        CGS_ASSERT(mpIconsTextureState != 0, "mpIconsTextureState");   // console line 658
    }

    // The console fills the UV tables here, BETWEEN the icon page and the masks.
    InitEventTypeUvs();

    {
        const void* const lpTexture = mpGuiCache->GetLoadedResource(KU_TEXTURE_ID_BACKGROUND_MASK);
        CGS_ASSERT(lpTexture != 0, "lpTexture != NULL");   // console line 665
        mBackgroundMaskTextureStateResource[0] = 0;
        mpBackgroundMaskTextureState =
            CreateIconTextureState(&mBackgroundMaskTextureStateBacking, lpTexture);
        CGS_ASSERT(mpBackgroundMaskTextureState != 0, "mpBackgroundMaskTextureState");   // line 676
    }

    {
        const void* const lpTexture = mpGuiCache->GetLoadedResource(KU_TEXTURE_ID_PRERACE_MASK);
        CGS_ASSERT(lpTexture != 0, "lpTexture != NULL");   // console line 680
        mPreRaceMaskTextureStateResource[0] = 0;
        mpPreRaceMaskTextureState =
            CreateIconTextureState(&mPreRaceMaskTextureStateBacking, lpTexture);
        CGS_ASSERT(mpPreRaceMaskTextureState != 0, "mpPreRaceMaskTextureState");   // line 691
    }

    {
        const void* const lpTexture = mpGuiCache->GetLoadedResource(KU_TEXTURE_ID_ROAD_SIGNS);
        CGS_ASSERT(lpTexture != 0, "lpTexture != NULL");   // console line 694
        mRoadSignsTextureStateResource[0] = 0;
        mpRoadSignsTextureState =
            CreateIconTextureState(&mRoadSignsTextureStateBacking, lpTexture);
        CGS_ASSERT(mpRoadSignsTextureState != 0, "mpRoadSignsTextureState");   // line 705
    }
}

// ---------------------------------------------------------------------------
// @0x824566F0 -- InitEventTypeUvs. Fills the four big UV corner tables (2 icon-type rows x
// KU_ICON_EVENT_TYPE_COUNT event-type columns) and the four mini-icon tables (2 x 6) from
// the atlas geometry.
//
// The console body is a straight cell walk over a uniform grid: for each icon-type row the
// column's atlas FRAME index comes from the switch (KAI_EVENTTYPE_TO_ICON_FRAME), the frame
// is split into (atlasRow, atlasCol) by the page's columns-per-row, and the four corners
// fall out as (col * w/W, row * h/H) .. plus one cell. The mini-icon pass uses the frame
// index directly and pushes every V down by KF_MINI_ICON_TEXTURE_OFFSET.
//
// (The X360 export of this function carries "local variable allocation has failed"; the
// arithmetic above is nonetheless unambiguous -- the four destination pointers are
// this+0x3038 / +0x30E8 / +0x3198 / +0x3248 stepped by 8 per column and 88 per row, and the
// mini pass writes this+0x32F8 / +0x3358 / +0x33B8 / +0x3418 stepped by 8. The two integer
// divides carry the console's `twllei/twlgei` divide-by-zero and overflow traps, which are
// compiler-inserted PPC guards with no PC equivalent.)
// ---------------------------------------------------------------------------
void CrashNavIconRenderer::InitEventTypeUvs()
{
    for (s32 liIconType = 0; liIconType < E_CRASHNAVICON_NUM; ++liIconType)
    {
        const f32 lfInvTextureWidth  = 1.0f / KAF_TEXTURE_WIDTH[liIconType];
        const f32 lfInvTextureHeight = 1.0f / KAF_TEXTURE_HEIGHT[liIconType];
        const f32 lfCellWidth        = KAF_ICON_WIDTH[liIconType];
        const f32 lfCellHeight       = KAF_ICON_HEIGHT[liIconType];
        // `(int)(KAF_TEXTURE_WIDTH / KAF_ICON_WIDTH)` -- the console truncates the float
        // ratio to an int and divides the frame index by it.
        const s32 liColumnsPerRow    = static_cast<s32>(KAF_TEXTURE_WIDTH[liIconType] / lfCellWidth);

        for (u32 luColumn = 0; luColumn < KU_ICON_EVENT_TYPE_COUNT; ++luColumn)
        {
            const s32 liFrame    = KAI_EVENTTYPE_TO_ICON_FRAME[luColumn];
            const s32 liAtlasRow = liFrame / liColumnsPerRow;
            const s32 liAtlasCol = liFrame % liColumnsPerRow;

            const f32 lfU0 = static_cast<f32>(liAtlasCol) * lfCellWidth  * lfInvTextureWidth;
            const f32 lfV0 = static_cast<f32>(liAtlasRow) * lfCellHeight * lfInvTextureHeight;
            const f32 lfU1 = lfU0 + lfCellWidth  * lfInvTextureWidth;
            const f32 lfV1 = lfV0 + lfCellHeight * lfInvTextureHeight;

            mav2IconUvTopLeft    [liIconType][luColumn].x = lfU0;
            mav2IconUvTopLeft    [liIconType][luColumn].y = lfV0;
            mav2IconUvBottomLeft [liIconType][luColumn].x = lfU0;
            mav2IconUvBottomLeft [liIconType][luColumn].y = lfV1;
            mav2IconUvTopRight   [liIconType][luColumn].x = lfU1;
            mav2IconUvTopRight   [liIconType][luColumn].y = lfV0;
            mav2IconUvBottomRight[liIconType][luColumn].x = lfU1;
            mav2IconUvBottomRight[liIconType][luColumn].y = lfV1;
        }
    }

    for (s32 liIconType = 0; liIconType < E_CRASHNAVICON_NUM; ++liIconType)
    {
        const f32 lfInvTextureWidth  = 1.0f / KAF_TEXTURE_WIDTH[liIconType];
        const f32 lfInvTextureHeight = 1.0f / KAF_TEXTURE_HEIGHT[liIconType];
        const f32 lfCellWidth        = KAF_MINI_ICON_WIDTH[liIconType];
        const f32 lfCellHeight       = KAF_MINI_ICON_HEIGHT[liIconType];
        const s32 liColumnsPerRow    = static_cast<s32>(KAF_TEXTURE_WIDTH[liIconType] / lfCellWidth);

        for (s32 liColumn = 0; liColumn < E_CRASHNAVICON_EVENTTYPE_MINI_INDEX_COUNT; ++liColumn)
        {
            // The mini pass indexes the atlas by the mini-icon index directly (no remap).
            const s32 liAtlasRow = liColumn / liColumnsPerRow;
            const s32 liAtlasCol = liColumn % liColumnsPerRow;

            const f32 lfU0 = static_cast<f32>(liAtlasCol) * lfCellWidth  * lfInvTextureWidth;
            const f32 lfV0 = static_cast<f32>(liAtlasRow) * lfCellHeight * lfInvTextureHeight;
            const f32 lfU1 = lfU0 + lfCellWidth  * lfInvTextureWidth;
            const f32 lfV1 = lfV0 + lfCellHeight * lfInvTextureHeight;

            // Every generated V is pushed into the mini-icon band of the same page.
            mav2MiniIconUvTopLeft    [liIconType][liColumn].x = lfU0;
            mav2MiniIconUvTopLeft    [liIconType][liColumn].y = lfV0 + KF_MINI_ICON_TEXTURE_OFFSET;
            mav2MiniIconUvBottomLeft [liIconType][liColumn].x = lfU0;
            mav2MiniIconUvBottomLeft [liIconType][liColumn].y = lfV1 + KF_MINI_ICON_TEXTURE_OFFSET;
            mav2MiniIconUvTopRight   [liIconType][liColumn].x = lfU1;
            mav2MiniIconUvTopRight   [liIconType][liColumn].y = lfV0 + KF_MINI_ICON_TEXTURE_OFFSET;
            mav2MiniIconUvBottomRight[liIconType][liColumn].x = lfU1;
            mav2MiniIconUvBottomRight[liIconType][liColumn].y = lfV1 + KF_MINI_ICON_TEXTURE_OFFSET;
        }
    }
}

// ---------------------------------------------------------------------------
// @0x82456168 -- RecvEvent. The renderer's whole input surface: the GuiCache bind, the
// road-sign font adopt, the per-frame map view record, and the five crash-nav status
// payloads the map screen publishes on channel 41.
// ---------------------------------------------------------------------------
void CrashNavIconRenderer::RecvEvent(const CgsModule::Event* lpEvent, s32 liEventType)
{
    switch (liEventType)
    {
    case KI_EVENT_SET_CACHE:
    {
        // The payload carries a NATIVE-WIDTH cache pointer on this host; a word-0 read
        // would truncate it to 32 bits (the SatNavRenderer H3b boot-AV lesson). Typed read.
        GuiCache* const* lppCache = reinterpret_cast<GuiCache* const*>(lpEvent);
        CGS_ASSERT(*lppCache != 0, "Invalid GUI cache pointer");   // console line 380
        mpGuiCache = *lppCache;   // stw, 0x90
        break;
    }

    case KI_EVENT_LOAD_NOTIFICATION:
    {
        // Adopt the road-sign font. The console's assert text names the ticker
        // ("InGameMessageRenderer::RecvEvent") -- a copy-paste in the shipped source that
        // is reproduced verbatim (console line 491).
        const CgsGui::GuiEventLoadNotification* lpcNotification =
            reinterpret_cast<const CgsGui::GuiEventLoadNotification*>(lpEvent);
        CGS_ASSERT(lpcNotification != 0 && lpcNotification->mResourceHandle.mpResourceMemory != 0,
                   "Invalid resource data sent InGameMessageRenderer::RecvEvent");
        if (lpcNotification == 0)
            break;
        if (static_cast<s32>(lpcNotification->meRequestType) != KI_REQUESTTYPE_FONTDATA)
            break;   // `lwz r11,8(r31); cmpwi 0x10; bne default`

        CgsResource::SafeResourceHandle<CgsResource::Font> lFont;
        lFont.mpResourceMemory = lpcNotification->mResourceHandle.mpResourceMemory;
        lFont.mpSourceEntry    = lpcNotification->mResourceHandle.mpSourceEntry;
        CGS_ASSERT(lFont.mpResourceMemory != 0,
                   "lpFont != CgsResource::NULLResourceHandle");   // console line 497
        if (lFont.mpResourceMemory == 0)
            break;

        CgsResource::Font* lpFont = static_cast<CgsResource::Font*>(lFont);
        const char* lpcFontName = lpFont->macTypefaceFamilyName;   // the console's Font+336 read

        // While the renderer still has NO font, adopt the language default (the console
        // compares the loaded typeface family against LanguageManager::GetDefaultFont()).
        // HasDefaultFont() gate: PC-only -- PrepareDefaultFont is unreconstructed, so an
        // unconditional GetDefaultFont() fires its own assert on every early font load
        // (the ticker's precedent, BrnInGameMessageRenderer.cpp).
        if (mTextObject.mpFont.mpResourceMemory == 0 && mpLanguageManager != 0 &&
            mpLanguageManager->HasDefaultFont())
        {
            const char* lpcDefault = mpLanguageManager->GetDefaultFont();
            if (lpcDefault != 0 &&
                _strnicmp(lpcFontName, lpcDefault, std::strlen(lpcDefault)) == 0)
            {
                mTextObject.mpFont = lFont;
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 &&
                    CgsDev::Log::gpDebugPrint != 0)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "CrashNavIconRenderer: using default font " << lpcFontName << "\n";
                }
            }
        }

        // ...and always prefer the dedicated road-sign face when it arrives. ARTIST
        // compares the first 11 characters against "B5EACONDISS"
        // (DWARF KAC_ROAD_SIGN_FONT_NAME, char[12]).
        if (_strnicmp(lpcFontName, "B5EACONDISS", 11) == 0)
        {
            mTextObject.mpFont = lFont;
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 &&
                CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "CrashNavIconRenderer: using font " << lpcFontName << "\n";
            }
        }
        break;
    }

    case KI_EVENT_RENDER_MAIN_MAP:
        // The console memcpy's 48 bytes into +0x60. On this host the record is
        // native-width (it carries a pointer), so a literal 48-byte copy would truncate it
        // -- typed assignment instead (the PlayAptMovie / SatNav-212 width precedent).
        mRenderMainMapEvent = *reinterpret_cast<const GuiEventRenderMainMap*>(lpEvent);
        break;

    case KI_EVENT_DRAW_EVENT_ICONS:
    {
        const GuiEventDrawEventIcons* lpcDrawIcons =
            reinterpret_cast<const GuiEventDrawEventIcons*>(lpEvent);

        // Early-out when nothing about the icon pass changed.
        if (lpcDrawIcons->GetDrawIcons() == mbRenderEventStarts &&
            lpcDrawIcons->GetDisplayType() == meIconDisplayType)
        {
            break;
        }

        // A positive fade time starts a cross-fade from the CURRENT display type. When the
        // renderer is currently parked on COUNT ("nothing shown") the console fades from
        // the INCOMING type instead, so the first reveal fades in rather than snapping.
        if (lpcDrawIcons->GetFadeTime() > 0.0f)
        {
            mbOldTypeFading = true;                               // stb 1, 0x34A5
            const f32 lfNow = mpGuiCache->GetTime();
            mfIconFadeStartTime = lfNow;                          // stfs, 0x34AC
            mfIconFadeEndTime   = lpcDrawIcons->GetFadeTime() + lfNow;   // stfs, 0x34B0

            GuiEventDrawEventIcons::EIconDisplayType leFadingFrom = meIconDisplayType;
            if (leFadingFrom == GuiEventDrawEventIcons::E_ICON_DISPLAY_TYPE_COUNT)
                leFadingFrom = lpcDrawIcons->GetDisplayType();
            meFadingIconDisplayType = leFadingFrom;               // stw, 0x34A8
        }

        mbRenderEventStarts = lpcDrawIcons->GetDrawIcons();       // stb, 0x34A4
        meIconDisplayType   = lpcDrawIcons->GetDisplayType();     // stw, 0x12C
        lpcDrawIcons->GetIgnoreIcons(mauIconsToIgnore, &miNumIconsToIgnore); // +0x3478 / +0x34A0
        break;
    }

    case KI_EVENT_FILTER_EVENT_ICONS:
        // GuiEventFilterEventIcons -- a single mode word into +0x3034.
        meGameModeFilter = static_cast<s32>(*reinterpret_cast<const u32*>(lpEvent));
        break;

    case KI_EVENT_SET_INSPECTED_ICON:
        // GuiEventSetInspectedEventIcon -- a single id word into +0x148.
        muInspectedEventID = *reinterpret_cast<const u32*>(lpEvent);
        break;

    case KI_EVENT_SET_HOVERED_ICON:
    {
        const GuiEventSetHoveredEventIcon* lpcHovered =
            reinterpret_cast<const GuiEventSetHoveredEventIcon*>(lpEvent);

        // The console's guard (@0x8245656C..0x824565B8) is a three-way "at most one of
        // them may be set" test over the drive-through id, the rival/player id and the
        // event id; it fires when two or more are non-zero. Written as the count the
        // branch lattice computes.
        {
            const s32 liActiveHovers =
                ((lpcHovered->mHoveredDriveThroughID != 0) ? 1 : 0) +
                ((lpcHovered->mHoveredPlayerID       != 0) ? 1 : 0) +
                ((lpcHovered->muHoveredEventID       != 0) ? 1 : 0);
            CGS_ASSERT(liActiveHovers <= 1,
                       "Invalid icon hover state - cannot hover over two icon types at once."); // line 451
        }

        // The console copies three doublewords (its whole 24-byte record) into +0x150;
        // typed assignment carries the host-width road-name pointer too.
        mHoveredEventIcon = *lpcHovered;
        break;
    }

    case KI_EVENT_MAP_CURSOR_STATUS:
        CGS_ASSERT(lpEvent != 0, "lpMapCursorStatus");   // console line 461
        if (lpEvent != 0)
        {
            // Four doublewords into +0x1A0 on the console == the whole record.
            mGuiEventMapCursorStatus = *reinterpret_cast<const GuiEventMapCursorStatus*>(lpEvent);
        }
        break;

    case KI_EVENT_MAP_ICON_STATUS:
        CGS_ASSERT(lpEvent != 0, "lpMapIconStatus");   // console line 471
        if (lpEvent != 0)
        {
            // Two words into +0x1C4 (bank pointer + count).
            mGuiEventMapIconStatus = *reinterpret_cast<const GuiEventMapIconStatus*>(lpEvent);
        }
        break;

    case KI_EVENT_ROAD_SIGN_ICON_STATUS:
        CGS_ASSERT(lpEvent != 0, "lRoadSignIconStatus");   // console line 481
        if (lpEvent != 0)
        {
            // Two words into +0x1544 (bank pointer + scale factor).
            mRoadSignIconStatus = *reinterpret_cast<const GuiEventRoadSignIconStatus*>(lpEvent);
        }
        break;

    default:
        break;
    }
}

} // namespace BrnGui

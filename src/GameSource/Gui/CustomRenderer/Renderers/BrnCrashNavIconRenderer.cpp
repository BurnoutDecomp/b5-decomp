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
#include <cstdlib>   // getenv ([cnav-diag])

#include "GameSource/Gui/BrnGuiCache.h"                          // BrnGui::GuiCache (resource pump + GetTime)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h" // sResourceTuple / GuiEventLoadNotification
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"  // CgsLanguage::LanguageManager
#include "GameShared/GameClasses/Fonts/CgsFont.h"                // CgsResource::Font (macTypefaceFamilyName)
#include "GameShared/GameClasses/Core/CgsAssert.h"               // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                   // CgsID
#include "GameShared/GameClasses/Development/Log/CgsLog.h"       // CgsDev::Log / CgsDev::Message
#include "pc/gcm/renderengine/renderstates.h"                    // renderengine::TextureState (+Parameters) / Texture

#include <cstring>   // _strnicmp / strlen

// includes folded in from the BrnCrashNavIconRenderer_w*.cpp partfiles (2026-09-15)
#include "GameSource/Gui/BrnGuiWorldDataController.h"      // BrnGui::WorldDataController::GetTotalNumberOfOnlineLandmarks
#include "GameSource/GameState/Progression/BrnProfile.h"   // BrnProgression::ProfileEvent (GetIconInformation)
#include "SharedClasses/Progression/BrnRaceEventData.h"    // BrnProgression::RaceEventData (GetMode / GetEventInstanceId)
#include "GameSource/Gui/SatNav/BrnMapIconManager.h"       // BrnGui::MapIconManager::GetEventIconManager
#include "GameSource/Gui/View/BrnRoadSignIconManager.h"    // BrnGui::RoadSignIcon (RenderRoadSign's bank element -- reached through BrnMapIconManager.h too, named here because this TU dereferences it)
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"          // BrnGui::MapTransform
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptRenderHandler.h"       // CgsGui::AptIm2dRenderBuffer
#include "GameShared/GameClasses/Gui/View/ParticleSystem2d/CgsBillboardRenderer.h"  // the shared GUI render states + screen transform
#include <cmath>   // std::cos / std::sin (RotatateRect)

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
    {
        static const bool sbCnavDiag = (getenv("BRN_SATNAV_DIAG") != 0);
        static s32 saiSeen[8] = { 0 };
        s32 liSlot = -1;
        switch (liEventType)
        {
        case KI_EVENT_RENDER_MAIN_MAP:       liSlot = 0; break;
        case KI_EVENT_DRAW_EVENT_ICONS:      liSlot = 1; break;
        case KI_EVENT_FILTER_EVENT_ICONS:    liSlot = 2; break;
        case KI_EVENT_MAP_CURSOR_STATUS:     liSlot = 3; break;
        case KI_EVENT_MAP_ICON_STATUS:       liSlot = 4; break;
        case KI_EVENT_ROAD_SIGN_ICON_STATUS: liSlot = 5; break;
        case KI_EVENT_SET_CACHE:             liSlot = 6; break;
        default: break;
        }
        if (sbCnavDiag && liSlot >= 0 && saiSeen[liSlot] < 3 && CgsDev::Log::gpDebugPrint != 0)
        {
            ++saiSeen[liSlot];
            *CgsDev::Log::gpDebugPrint << "[cnav-diag] RecvEvent id=" << liEventType << "\n";
        }
    }
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

    // [DIAG] NOT IN THE X360 BINARY -- [cnav-diag] which map records reach this renderer.
    // BRN_SATNAV_DIAG only; first 3 arrivals per id.
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

// ============================================================================
// FOLDED FROM BrnCrashNavIconRenderer_wK_01.cpp (wave K) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// BrnCrashNavIconRenderer_wK_01.cpp -- BrnGui::CrashNavIconRenderer, the RENDER half.
//
// Partner TU of BrnCrashNavIconRenderer.cpp (the CORE half: ctor / Construct / Prepare /
// Release / Destruct / RecvEvent / GetID / SetRenderEnabled / InitResources /
// InitEventTypeUvs). Both compile against the SAME committed header -- see the HEADER
// CONTRACT block at the top of BrnCrashNavIconRenderer.h.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (Jan-2008):
//   RenderComponent      @0x8246AE38   RenderIcons         @0x8246A410
//   RenderEventIcon      @0x8245D210   RenderCursor        @0x8245D8C0
//   RenderQuad           @0x8245DB90   RotatateRect        @0x8245DD28
//   RenderDriveThroughs  @0x82469038   RenderDriveThrough  @0x824639F8
//   RenderRoadSigns      @0x824692D8   RenderStartFinish   @0x82465210
//   RenderRivals         @0x82465468   GetNumIcons         @0x82456C68
//
//   GetIconInformation   @0x82456D80   RenderRoadSign      @0x82463E78
//
// ⭐ 2026-08-29 (FIX2): GetIconInformation and RenderRoadSign, which the first pass
//    parked as "conductor link stubs blocked on unrecoverable data", are BODIED. Nothing
//    in this TU needs a link stub any more. Every value that was called unrecoverable was
//    read out of the raw image or out of the asm -- see the recovery notes on each
//    function and on the KAPC_ROAD_IDS / KAV4_SIGN_PLATE_UV /
//    KAU_EVENTMODE_TO_MINI_ICON_COLUMN tables below.
//
// ---------------------------------------------------------------------------
// THE DRAW TARGET, and the SatNav-311 trap the brief names
// ---------------------------------------------------------------------------
// Every Render* below takes CgsGraphics::ImRenderBuffer<Basic2dColouredTexturedVertex>*,
// resolved from the ImRendererSet exactly as BrnBoostBarRenderer.cpp's ResolveBoostBarBuffer
// and BrnSatNavRenderer.cpp's RenderComponent already do. The console reaches it as
// `*lpRendererSet` then `+ 4`; that `+ 4` IS CgsGui::AptIm2dRenderBuffer::mCommandBuffer,
// so on this host it is reached BY NAME. Nothing in this TU applies an X360 byte offset to
// a host object: every member read below is a named field. (The SatNav H3b 311 defect was
// exactly the opposite -- a raw console offset re-applied on the 64-bit gate.)
//
// TWO consequences of the host width that this TU reproduces deliberately:
//  * mRivalIcons / mStartFinishIcons / mGuiEventMapIconStatus.lpSatNavIcons are walked with
//    array indexing and named CrashNavMapIcon methods. On the console the element is the
//    0x1F0-byte GUI component and the icon subobject sits at element+0x90 (hence the
//    RenderRivals `this + 608` vs the RenderDriveThroughs `this + 464` pair, 0x90 apart) --
//    a distinction that does not survive to the host and must not be re-introduced.
//  * every packed vertex colour goes through PackVertexColour/UnpackConsoleColour below,
//    never a raw u32 store: the console's word is big-endian [r][g][b][a] and the host
//    vertex is the little-endian struct RGBA8 {r,g,b,a}. (Same convention
//    BrnBoostBarRenderer.cpp's PackBillboardDiffuse note cites this function for.)
//
// SEMANTIC-LEVEL SIMD: RenderEventIcon's coordinate-space build and RenderQuad's colour
// pack are hand-vectorised VMX128 on the console (lvx128/vperm/vrlimi128/vmaddfp/vmulfp128
// /fctidz). Those have no portable PC equivalent, so they are reconstructed at the semantic
// level through the named MapTransform helpers and named struct fields -- the policy
// rw/math/vpu/types.h states and BrnMapUtils.cpp / BrnSatNavRenderer.cpp already follow.
//
// ASSERTS are lowered to CGS_ASSERT with the recovered expression; the console's own
// BrnCrashNavIconRenderer.cpp line number rides in a trailing comment (CGS_ASSERT stamps
// __FILE__/__LINE__ of THIS file).
// ============================================================================


namespace BrnGui
{
// ---------------------------------------------------------------------------
// ⚠️ PRE-EXISTING ODR FORK -- why SetMaskRect is declared here instead of included.
//
// Its declaration's home is GameSource/Gui/BrnCustomRendererManager.h, which is what this
// TU wants to include (BrnBoostBarRenderer.cpp does exactly that). But that header pulls
// GameSource/Gui/CustomRenderer/Renderers/BrnNetworkPlayerImageRenderer.h, whose line 75
// re-defines `BrnGui::GuiEventNetworkPlayerImage` -- a type whose canonical home is
// GameSource/Gui/BrnGuiDemangledEventTypes.h:172, which THIS TU already pulls (through
// BrnCrashNavIconRenderer.h, for GuiEventSetHoveredEventIcon id 559). Including both is a
// hard C2011. The fork is not mine and pre-dates this wave: GameBridgeGameStateToX.cpp:56,
// GameBridgeGameStateToX_EventFlowGuiEvents.cpp:65-81 and GameBridgeWorldToGui.cpp:37 all
// record it, and the second of those already names the fix ("remove the
// GuiEventNetworkPlayerImage fork from BrnNetworkPlayerImageRenderer.h:75") -- which needs
// the demangled home to grow the real {NetworkTexture*, s32} shape first, and is a
// network-player-image wave, not a crash-nav one.
//
// A FUNCTION declaration is not a type fork: the signature below is copied verbatim from
// BrnCustomRendererManager.h:235 and links against the one body in BrnCustomRenderer.cpp
// (X360 @0x82450D28). Replace it with the include the moment the type fork is retired.
// ---------------------------------------------------------------------------
void SetMaskRect(CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex>& lrCmd,
                 const renderengine::TextureState* lpMaskTextureState,
                 const Vector4& lv4Rect, const Vector4& lv4MaskUv);

namespace
{
    // -----------------------------------------------------------------------
    // Device (1280x720) -> normalised screen. The console keeps these two as
    // FUNCTION-LOCAL statics with a lazy first-use guard word, one pair per render
    // function: RenderDriveThrough (flt_82FB382C/3830, guard dword_82FB3834),
    // RenderStartFinish (flt_82FB39E4/39E8, guard dword_82FB39EC), RenderRivals
    // (flt_82FB3AA8/3AAC, guard dword_82FB3AB0) and RenderRoadSign (flt_82FB39C8/39CC,
    // guard dword_82FB39D0). The VALUES are recovered -- they are stored by the guarded
    // init arm inside the function bodies themselves, so unlike the .rdata floats below
    // they are not a data-segment read. Hoisted to one file-local pair: the guards are a
    // compiler-generated first-use latch over a constant, with no observable ordering.
    // -----------------------------------------------------------------------
    const f32 KF_DEVICE_TO_NORMALISED_X = 0.00078125001f;   // 1/1280
    const f32 KF_DEVICE_TO_NORMALISED_Y = 0.0013888889f;    // 1/720

    // -----------------------------------------------------------------------
    // The icon zoom band. RenderIcons @0x8246A4E8-0x8246A524:
    //   lfIconScale   = 3500 / mfZoomLevel                         (flt_82058790 == 1/3500)
    //   lfInvZoomBand = 1 / clamp(mfZoomLevel, 3500, 5500)         (flt_8205878C / flt_82058788)
    // flt_8205878C is already committed as 3500.0f elsewhere in the tree
    // (VehiclePhysics.cpp KF_CRASH_SCRUB_MASS names the same .rdata word).
    // -----------------------------------------------------------------------
    const f32 KF_ICON_ZOOM_REFERENCE = 3500.0f;   // flt_8205878C
    const f32 KF_ICON_ZOOM_MAX       = 5500.0f;   // flt_82058788

    // -----------------------------------------------------------------------
    // ⭐ RECOVERED 2026-08-29 (FIX2) -- dword_82055064[6], GetIconInformation's OFFLINE-events
    // "event mode -> MINI-icon column" remap. Read store-for-store out of the raw image
    // (file offset = VA - 0x82000000, big-endian): 0x82055064 = { 3, 1, 5, 4, 2, 4 }.
    //
    // Indexed by BrnProgression::RaceEventData::EModeType and answering an
    // ECrashNavEventTypeMiniIconIndex, which is what makes the six values legible:
    //   E_MODE_RACE(0)          -> 3 = MINI_INDEX_RACE
    //   E_MODE_ROAD_RAGE(1)     -> 1 = MINI_INDEX_ROADRAGE
    //   E_MODE_STUNT_ATTACK(2)  -> 5 = MINI_INDEX_STUNT
    //   E_MODE_SURVIVOR(3)      -> 4 = MINI_INDEX_MARKEDMAN
    //   E_MODE_BURNING_ROUTE(4) -> 2 = MINI_INDEX_BURNINGROUTE
    //   E_MODE_PURSUIT(5)       -> 4 = MINI_INDEX_MARKEDMAN
    // -- every entry lands inside E_CRASHNAVICON_EVENTTYPE_MINI_INDEX_COUNT (6), which is the
    // independent check that the table was read at the right address and the right width.
    //
    // It is byte-identical to the sat-nav twin dword_82054E48, already committed as
    // BrnSatNavRenderer.cpp's KAU_EVENTTYPE_TO_ICONROW; both were re-read from the image for
    // this note, so this is a measurement of two tables, not a copy of one.
    // -----------------------------------------------------------------------
    const u32 KAU_EVENTMODE_TO_MINI_ICON_COLUMN[6] = { 3, 1, 5, 4, 2, 4 };

    // =======================================================================
    // ⭐ THE ROAD-SIGN DATA, RECOVERED 2026-08-29 (FIX2) -- every value below was READ, not
    // authored: the string table and the 4-float rects out of the raw image (file offset =
    // VA - 0x82000000, big-endian), the half-extents and the colour vectors out of the
    // guarded first-use init arms inside RenderRoadSign's own asm.
    // =======================================================================

    // off_82F27C98 .. off_82F27D98 -- the 65 road ids RenderRoadSign matches each
    // RoadSign::mpcRoadId against. The ORDER is load-bearing: a hit's INDEX is the slot into
    // the published RoadSignIcon bank (`192 * v35 + mpRoadSignIcons`), so this table is the
    // only thing that maps an authored sign record to a live icon.
    //
    // ⚠️ THE PREVIOUS BANNER'S BOUND WAS OFF BY ONE. It named the range
    // "off_82F27C98..off_82F27D9C"; 0x82F27D9C is already the next pool ("ACH_FIRST_REPAIR",
    // the achievement-id strings). The table's last entry is 0x82F27D98 -> "invisible", which
    // is also what the asm's own bail test names (`if (v36 == off_82F27D98) goto end`), and
    // 0x82F27D98 - 0x82F27C98 == 0x100 == 64 strides, i.e. exactly 65 entries -- matching
    // RoadSignList::KI_NUM_ROAD_SIGNS and RoadSignIconManager::KU_NUM_SIGN_ICONS + 1.
    // Sixty-four are junction ids; the sixty-fifth is the literal "invisible" sentinel.
    const s32 KI_NUM_ROAD_IDS = 65;
    const char* const KAPC_ROAD_IDS[KI_NUM_ROAD_IDS] =
    {
        "397134", "394306", "535195", "505312", "393062",
        "396133", "394474", "386215", "394965", "385737",
        "392688", "393166", "397409", "396228", "385860",
        "392532", "392323", "396706", "393198", "395917",
        "392997", "393860", "396988", "396188", "397165",
        "395490", "387153", "393756", "392684", "394853",
        "506394", "394273", "396114", "395952", "396456",
        "393760", "397201", "396742", "390723", "397455",
        "561416", "396460", "383595", "396718", "561415",
        "387198", "397601", "395656", "386734", "397702",
        "395197", "506519", "395600", "392589", "393911",
        "394487", "397348", "393900", "393120", "387078",
        "393762", "535196", "561121", "561413", "invisible",
    };

    // flt_82FB39A0 / flt_82FB39A4 -- the POLE quad's device-space half extents, and
    // flt_82FB3898.. -- its atlas rect (the one rect the earlier pass could already name).
    const f32 KF_SIGN_POLE_HALFWIDTH  = 24.0f;   // flt_82FB39A0 (guard bit 0x20)
    const f32 KF_SIGN_POLE_HALFHEIGHT = 50.0f;   // flt_82FB39A4
    const Vector4 KV4_SIGN_POLE_UV =
        { 0.30078125f, 0.65625f, 0.39453125f, 0.87109375f };   // flt_82FB3898 (guard 0x200000)

    // The plate half-extents, one pair per RoadSign::ESignSize, from the four guarded init
    // arms (bits 0x10 / 0x8 / 0x4 / 0x2 in dword_82FB39D0, in SMALL..LARGE order):
    //   SMALL flt_82FB39A8/AC   MID_1 flt_82FB39B0/B4
    //   MID_2 flt_82FB39B8/BC   LARGE flt_82FB39C0/C4
    // (Every plate is 24 device pixels tall; only the width grows.)
    const CgsGraphics::Vector2 KAV2_SIGN_HALF_EXTENTS[RoadSign::E_SIGNSIZE_COUNT] =
    {
        {  53.0f, 24.0f },   // E_SIGNSIZE_SMALL
        {  68.0f, 24.0f },   // E_SIGNSIZE_MID_1
        {  80.5f, 24.0f },   // E_SIGNSIZE_MID_2
        { 109.5f, 24.0f },   // E_SIGNSIZE_LARGE
    };

    // The plate atlas rects, [sign size][sign colour]. THIS IS THE SELECTION the earlier pass
    // recorded as "a jump table the export's pseudocode folds away" -- true of the pseudocode,
    // not of the asm: the size switch (jpt_8246491C) picks one of four blocks, and each block
    // runs its own 4-way colour switch that stores four floats into RenderQuad's UV-rect stack
    // slot. Every arm's source address is in the comment beside it.
    const Vector4 KAV4_SIGN_PLATE_UV[RoadSign::E_SIGNSIZE_COUNT][4] =
    {
        {   // E_SIGNSIZE_SMALL  (jpt_82464D20)
            { 0.7109375f,  0.21875f,   0.9375f,     0.3203125f  },  // flt_82FB3888
            { 0.4765625f,  0.21875f,   0.703125f,   0.3203125f  },  // flt_82FB38D8
            { 0.67578125f, 0.328125f,  0.90234375f, 0.4296875f  },  // flt_82FB3848
            { 0.0f,        0.875f,     0.2265625f,  0.9765625f  },  // flt_82FB3918
        },
        {   // E_SIGNSIZE_MID_1  (jpt_82464AC8)
            { 0.0f,        0.65625f,   0.29296875f, 0.7578125f  },  // flt_82FB38B8
            { 0.35546875f, 0.53515625f,0.6484375f,  0.63671875f },  // flt_82FB38F8
            { 0.0f,        0.765625f,  0.29296875f, 0.8671875f  },  // flt_82FB3868
            { 0.35546875f, 0.42578125f,0.6484375f,  0.52734375f },  // flt_82FB3938
        },
        {   // E_SIGNSIZE_MID_2  (jpt_82464BF4)
            { 0.0f,        0.4375f,    0.34765625f, 0.5390625f  },  // flt_82FB38A8
            { 0.0f,        0.328125f,  0.34765625f, 0.4296875f  },  // flt_82FB38E8
            { 0.0f,        0.546875f,  0.34765625f, 0.6484375f  },  // flt_82FB3858
            { 0.4765625f,  0.109375f,  0.82421875f, 0.2109375f  },  // flt_82FB3928
        },
        {   // E_SIGNSIZE_LARGE  (jpt_8246499C)
            { 0.4765625f,  0.0f,       0.9453125f,  0.1015625f  },  // flt_82FB38C8
            { 0.0f,        0.109375f,  0.46875f,    0.2109375f  },  // flt_82FB3908
            { 0.0f,        0.21875f,   0.46875f,    0.3203125f  },  // flt_82FB3878
            { 0.0f,        0.0f,       0.46875f,    0.1015625f  },  // flt_82FB3948
        },
    };
    // FLAG (recovered but unreferenced): the eighteenth guarded rect, flt_82FB3838 (guard bit
    // 0x8000000) = { 0.35546875, 0.328125, 0.66796875, 0.41796875 }, is initialised by
    // RenderRoadSign and read by no arm of either switch. Authored-but-dead atlas data;
    // recorded here so a later reader does not go looking for a nineteenth combination.

    // The sign COLOUR vectors (unk_82FB3990 / 3980 / 3970 / 3960, guard bits 0x40..0x200).
    // Despite four slots there are only two distinct colours -- near-white (230/255) for
    // colours 0 and 1, black for 2 and 3. They drive the TEXT, not the plate quad: both
    // quads are submitted with the 1.0 splat (`vmr128 v1, v126`), and the colour vector goes
    // to mTextObject.mTextColour after a [0,1] clamp.
    const Vector4 KAV4_SIGN_TEXT_COLOUR[4] =
    {
        { 0.90196079f, 0.90196079f, 0.90196079f, 1.0f },   // unk_82FB3990
        { 0.90196079f, 0.90196079f, 0.90196079f, 1.0f },   // unk_82FB3980
        { 0.0f,        0.0f,        0.0f,        1.0f },   // unk_82FB3970
        { 0.0f,        0.0f,        0.0f,        1.0f },   // unk_82FB3960
    };

    // The two loose .rdata text constants the earlier banner also listed as missing.
    const f32 KF_SIGN_CHAR_SPACING   = 0.9f;    // flt_82F25C7C -> mfCharSpacingMultiplier
    const f32 KF_SIGN_FONT_SIZE_SCALE = 1.15f;  // flt_82F25C80 -> the mfFontHeight multiplier

    // The console moves an icon record's position with ONE whole-register `lvx128`/`stvx128`
    // pair, so all four lanes travel; the host types are distinct (Vector4 lane in, Vector3
    // out), and the copy is spelled out rather than cast so every lane travels.
    inline void CopyPositionLane(Vector3& lrv3Out, const Vector4& lrv4Lane)
    {
        lrv3Out.x = lrv4Lane.x;
        lrv3Out.y = lrv4Lane.y;
        lrv3Out.z = lrv4Lane.z;
        lrv3Out.w = lrv4Lane.w;
    }

    // -----------------------------------------------------------------------
    // ⚠️ FLAG (values, not algorithm) -- .data floats the symbol export does not carry.
    //
    // RenderIcons reads FOUR two-entry (per ECrashNavIconType) half-extent tables at
    // flt_82FB36F8 / flt_82FB3700 / flt_82FB3710 / flt_82FB3718 and scales each by
    // 1/clamp(zoom,3500,5500). Their DWARF names are on the class
    // (BrnCrashNavIconRenderer.h:353-356 / :364-367 -- KAF_ICON_HALFWIDTH_*,
    // KAF_ICON_HALFHEIGHT_*, KAF_MINI_ICON_HALFWIDTH_*, KAF_MINI_ICON_HALFHEIGHT_*), and
    // RenderEventIcon's own use pins which is which: the "use mini icon" arm takes
    // (flt_82FB3718, flt_82FB3700) and the big arm takes (flt_82FB36F8, flt_82FB3710).
    // ONLY the numeric contents are missing -- the whole 0x82FB36F0..0x82FB3728 block is
    // written by cinit thunks that are not in the export set, and the tree has only the
    // .i64, no readable image. (The four SIBLING lanes of that same block --
    // 0x82FB36F0/3708/3720/3728 -- belong to BrnSatNavRenderer and WERE recovered in the
    // H3b wave; ours are the interleaved four.)
    //
    // Carried over from BrnSatNavRenderer.cpp's recovered WORLDSIZE pair, on the same
    // justification E1 used for the atlas geometry: the two renderers index THE SAME two
    // icon atlases (texture ids 204/205) through the same 2-entry table and use the same
    // "authored world size / zoom" idiom (the sat-nav's own SCREENSPACE pair is its
    // no-zoom online path, which this renderer has no counterpart for). The mini pair is
    // the half of it that the 32px mini cell is of the 64px icon cell.
    // WORTH A DATA-DUMP FOLLOW-UP -- the produced on-screen icon SIZE is unverified for
    // this build; the indexing/zoom algorithm is X360-proven.
    // -----------------------------------------------------------------------
    const f32 KAF_ICON_HALFWIDTH      [CrashNavIconRenderer::E_CRASHNAVICON_NUM] = { 37.5f, 37.5f };           // flt_82FB36F8
    const f32 KAF_ICON_HALFHEIGHT     [CrashNavIconRenderer::E_CRASHNAVICON_NUM] = { 55.555557f, 55.555557f }; // flt_82FB3710
    const f32 KAF_MINI_ICON_HALFWIDTH [CrashNavIconRenderer::E_CRASHNAVICON_NUM] = { 18.75f, 18.75f };         // flt_82FB3718
    const f32 KAF_MINI_ICON_HALFHEIGHT[CrashNavIconRenderer::E_CRASHNAVICON_NUM] = { 27.777779f, 27.777779f }; // flt_82FB3700

    // ⚠️ FLAG (values, not algorithm) -- the same .rdata gap, cluster flt_82F25BC8 /
    // flt_82F25C48..flt_82F25C84. Each is named for the exact slot it fills; the
    // arithmetic around every one of them is X360-verbatim.
    //
    //  * KU_ICON_BASE_COLOUR (dword_82F25BC8) -- the event-icon vertex colour word whose
    //    LOW (alpha) byte RenderIcons rewrites, and only ever rewrites. Opaque white is
    //    the only value under which the fade arithmetic below is the whole story.
    //  * the cursor / drive-through half-extents are stated in NORMALISED screen units
    //    (the space every quad in this TU is built in). RenderStartFinish's own recovered
    //    pair is 32 device px on both axes, and these icons come off the SAME atlas page
    //    at the same authored cell size, so 32/1280 and 32/720 are the derived pair --
    //    and 0.025 / 0.044444446 are exactly two of the ratios BrnSatNavRenderer.cpp's
    //    recovered cinit lanes are built from, which is corroboration, not proof.
    //  * KF_PLAYER_ICON_PULSE_PERIOD (flt_82F25C84) is the period of the local-player
    //    icon's halo pulse in seconds; 1.0 is a placeholder.
    const u32 KU_ICON_BASE_COLOUR              = 0xFFFFFFFFu;   // dword_82F25BC8
    const f32 KF_CURSOR_HOVER_OFFSET_Y         = 0.0f;          // flt_82F25C58
    const f32 KF_CURSOR_HOVER_OFFSET_X         = 0.0f;          // flt_82F25C5C
    const f32 KF_CURSOR_HOVER_HALFHEIGHT       = 0.044444446f;  // flt_82F25C60
    const f32 KF_CURSOR_HOVER_HALFWIDTH        = 0.025f;        // flt_82F25C64
    const f32 KF_CURSOR_OFFSET_Y               = 0.0f;          // flt_82F25C48
    const f32 KF_CURSOR_OFFSET_X               = 0.0f;          // flt_82F25C4C
    const f32 KF_CURSOR_HALFHEIGHT             = 0.044444446f;  // flt_82F25C50
    const f32 KF_CURSOR_HALFWIDTH              = 0.025f;        // flt_82F25C54
    const f32 KF_DRIVETHROUGH_HOVER_LIFT       = 0.044444446f;  // flt_82F25C70
    const f32 KF_DRIVETHROUGH_HALFHEIGHT       = 0.044444446f;  // flt_82F25C74
    const f32 KF_DRIVETHROUGH_HALFWIDTH        = 0.025f;        // flt_82F25C78
    const f32 KF_PLAYER_ICON_PULSE_PERIOD      = 1.0f;          // flt_82F25C84

    // -----------------------------------------------------------------------
    // RECOVERED function-local statics (their init arms are in the function bodies, so
    // these ARE image values -- no FLAG). Grouped here so each render function reads as
    // the console's own straight-line code.
    // -----------------------------------------------------------------------

    // RenderCursor @0x8245D8C0 (flt_82FB3770..377C, guard dword_82FB3780).
    const f32 KAF_CURSOR_UV[4] = { 0.0f, 0.0f, 0.515625f, 0.515625f };

    // RenderStartFinish @0x82465210 (flt_82FB39D4..39E0, guard dword_82FB39EC). Device
    // pixels: a 64x64 quad nudged up-left off the marker's own device position.
    const f32 KF_STARTFINISH_HALFWIDTH  = 32.0f;    // flt_82FB39E0
    const f32 KF_STARTFINISH_HALFHEIGHT = 32.0f;    // flt_82FB39DC
    const f32 KF_STARTFINISH_OFFSET_X   = -10.0f;   // flt_82FB39D4
    const f32 KF_STARTFINISH_OFFSET_Y   = -25.0f;   // flt_82FB39D8

    // RenderRivals @0x82465468 (flt_82FB3AA0/3AA4, guard dword_82FB3AB0).
    const f32 KF_RIVAL_HALFWIDTH  = 32.0f;   // flt_82FB3AA4
    const f32 KF_RIVAL_HALFHEIGHT = 32.0f;   // flt_82FB3AA0

    // The hover grow/shrink timing the three hover paths share (RenderEventIcon
    // @0x8245D2xx, RenderDriveThrough @0x82463Bxx, RenderRivals @0x824656xx and
    // RenderCursor all inline the same two literals). DWARF h:371/372 names them
    // KF_SELECTED_GROW_TIME / KF_SELECTED_SHRINK_TIME; the reciprocals 4.0 and 10.0 are
    // what the console actually multiplies by.
    const f32 KF_SELECTED_GROW_TIME    = 0.25f;   // 1/4.0
    const f32 KF_SELECTED_SHRINK_TIME  = 0.1f;    // 1/10.0
    const f32 KF_SELECTED_GROW_RATE    = 4.0f;
    const f32 KF_SELECTED_SHRINK_RATE  = 10.0f;

    // ---- small shared helpers (all X360-inlined) ---------------------------

    f32 Clamp01(f32 lfValue)
    {
        if (lfValue < 0.0f) return 0.0f;
        if (lfValue > 1.0f) return 1.0f;
        return lfValue;
    }

    // The console's `fctidz` + low-byte read of a 0..255 float lane.
    u8 ColourLaneToByte(f32 lfLane)
    {
        f32 lfScaled = lfLane;
        if (lfScaled < 0.0f) lfScaled = 0.0f;
        if (lfScaled > 1.0f) lfScaled = 1.0f;
        return static_cast<u8>(static_cast<s32>(lfScaled * 255.0f));
    }

    // RenderQuad's colour pack, isolated. The console clamps v1 to [0,1], multiplies by
    // the all-255 vector at unk_8305A950, truncates each lane and shuffles the four bytes
    // so the stored word is big-endian [x][y][z][w] -- i.e. memory order r,g,b,a. On the
    // host that memory order IS struct RGBA8, so the shuffle folds away and the lanes are
    // assigned by name.
    CgsGraphics::RGBA8 PackVertexColour(const Vector4& lrv4Colour)
    {
        CgsGraphics::RGBA8 lColour;
        lColour.r = ColourLaneToByte(lrv4Colour.x);
        lColour.g = ColourLaneToByte(lrv4Colour.y);
        lColour.b = ColourLaneToByte(lrv4Colour.z);
        lColour.a = ColourLaneToByte(lrv4Colour.w);
        return lColour;
    }

    // The inverse fold for RenderIcons' packed event-icon colour word, which travels as a
    // raw u32 (dword_82F25BC8 with its low byte rewritten) and lands straight in the four
    // vertices. Console memory order is [r][g][b][a] with `a` in the low byte of the word.
    CgsGraphics::RGBA8 UnpackConsoleColour(u32 luColour)
    {
        CgsGraphics::RGBA8 lColour;
        lColour.r = static_cast<u8>(luColour >> 24);
        lColour.g = static_cast<u8>(luColour >> 16);
        lColour.b = static_cast<u8>(luColour >> 8);
        lColour.a = static_cast<u8>(luColour);
        return lColour;
    }

    CgsGraphics::Vector2 MakeV2(f32 lfX, f32 lfY)
    {
        CgsGraphics::Vector2 lv2;
        lv2.x = lfX;
        lv2.y = lfY;
        return lv2;
    }

    Vector4 MakeV4(f32 lfX, f32 lfY, f32 lfZ, f32 lfW)
    {
        Vector4 lv4;
        lv4.x = lfX; lv4.y = lfY; lv4.z = lfZ; lv4.w = lfW;
        return lv4;
    }

    // The console's opaque-white quad colour: `vspltisw v0,1` + `vcfsx`/`vcsxwfp128`,
    // i.e. the splatted 1.0 vector every RenderQuad call site but RenderRivals passes.
    Vector4 OpaqueWhite()
    {
        return MakeV4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    // Resolve the 2D command buffer out of the renderer set. Identical to
    // BrnBoostBarRenderer.cpp's ResolveBoostBarBuffer and BrnSatNavRenderer.cpp's inline
    // read: the console's `v4 = *a2` is the AptIm2dRenderBuffer and its `v4 + 4` is that
    // object's mCommandBuffer, so on the host both collapse to this named member.
    CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex>*
    ResolveCrashNavBuffer(CgsGui::ImRendererSet* lpRendererSet)
    {
        if (lpRendererSet == 0)
            return 0;
        CgsGui::AptIm2dRenderBuffer* lpAptBuffer =
            *reinterpret_cast<CgsGui::AptIm2dRenderBuffer* const*>(lpRendererSet);
        return (lpAptBuffer != 0) ? &lpAptBuffer->mCommandBuffer : 0;
    }

    // The per-IconState rival/player icon colour (RenderRivals' jump table
    // @0x82465940). Eleven distinct 0..1 RGBA vectors, each a guarded function-local
    // static whose init arm carries the literal lanes -- so these ARE recovered values.
    // The PLAYER_* row (states 1..13) and the RIVAL_* row (states 14..25) share the same
    // eleven colours; states 1/2/3 and 14/15/16 collapse onto their row's first entry the
    // way the console's `default:` arm does.
    const Vector4 KAV4_ICON_COLOUR_YELLOW = { 1.0f,        0.95294118f, 0.023529412f, 1.0f }; // unk_82FB3A90
    const Vector4 KAV4_ICON_COLOUR_RED    = { 0.91764706f, 0.082352944f,0.19607843f,  1.0f }; // unk_82FB3A80
    const Vector4 KAV4_ICON_COLOUR_BLUE   = { 0.18431373f, 0.38431373f, 0.66274512f,  1.0f }; // unk_82FB3A70
    const Vector4 KAV4_ICON_COLOUR_PINK   = { 0.79215688f, 0.50980395f, 0.73333335f,  1.0f }; // unk_82FB3A60
    const Vector4 KAV4_ICON_COLOUR_GREEN  = { 0.3137255f,  0.72156864f, 0.24313726f,  1.0f }; // unk_82FB3A50
    const Vector4 KAV4_ICON_COLOUR_ORANGE = { 0.82745099f, 0.3764706f,  0.14901961f,  1.0f }; // unk_82FB3A40
    const Vector4 KAV4_ICON_COLOUR_PURPLE = { 0.43137255f, 0.18039216f, 0.78823531f,  1.0f }; // unk_82FB3A30
    const Vector4 KAV4_ICON_COLOUR_CYAN   = { 0.10588235f, 0.97647059f, 0.88627452f,  1.0f }; // unk_82FB3A20
    const Vector4 KAV4_ICON_COLOUR_WHITE  = { 1.0f,        1.0f,        1.0f,         1.0f }; // unk_82FB3A10
    const Vector4 KAV4_ICON_COLOUR_GRAY   = { 0.58823532f, 0.58823532f, 0.58823532f,  1.0f }; // unk_82FB3A00
    const Vector4 KAV4_ICON_COLOUR_BLACK  = { 0.0f,        0.0f,        0.0f,         1.0f }; // unk_82FB39F0

    // Returns the icon colour and reports whether the state is a LOCAL-PLAYER one (the
    // console's `v43`, raised on exactly states 1..13 -- the flag that adds the pulsing
    // halo quad underneath).
    Vector4 GetRivalIconColour(MapIconBrnBase::IconState leState, bool* lpbIsLocalPlayer)
    {
        *lpbIsLocalPlayer = (leState >= MapIconBrnBase::E_ICONSTATE_PLAYER_OFFLINE &&
                             leState <= MapIconBrnBase::E_ICONSTATE_PLAYER_BLACK);
        switch (leState)
        {
        case MapIconBrnBase::E_ICONSTATE_PLAYER_RED:
        case MapIconBrnBase::E_ICONSTATE_RIVAL:
        case MapIconBrnBase::E_ICONSTATE_RIVAL_RED:     return KAV4_ICON_COLOUR_RED;
        case MapIconBrnBase::E_ICONSTATE_PLAYER_BLUE:
        case MapIconBrnBase::E_ICONSTATE_RIVAL_BLUE:    return KAV4_ICON_COLOUR_BLUE;
        case MapIconBrnBase::E_ICONSTATE_PLAYER_PINK:
        case MapIconBrnBase::E_ICONSTATE_RIVAL_PINK:    return KAV4_ICON_COLOUR_PINK;
        case MapIconBrnBase::E_ICONSTATE_PLAYER_GREEN:
        case MapIconBrnBase::E_ICONSTATE_RIVAL_GREEN:   return KAV4_ICON_COLOUR_GREEN;
        case MapIconBrnBase::E_ICONSTATE_PLAYER_ORANGE:
        case MapIconBrnBase::E_ICONSTATE_RIVAL_ORANGE:  return KAV4_ICON_COLOUR_ORANGE;
        case MapIconBrnBase::E_ICONSTATE_PLAYER_PURPLE:
        case MapIconBrnBase::E_ICONSTATE_RIVAL_PURPLE:  return KAV4_ICON_COLOUR_PURPLE;
        case MapIconBrnBase::E_ICONSTATE_PLAYER_CYAN:
        case MapIconBrnBase::E_ICONSTATE_RIVAL_CYAN:    return KAV4_ICON_COLOUR_CYAN;
        case MapIconBrnBase::E_ICONSTATE_PLAYER_WHITE:
        case MapIconBrnBase::E_ICONSTATE_RIVAL_WHITE:   return KAV4_ICON_COLOUR_WHITE;
        case MapIconBrnBase::E_ICONSTATE_PLAYER_GRAY:
        case MapIconBrnBase::E_ICONSTATE_RIVAL_GRAY:    return KAV4_ICON_COLOUR_GRAY;
        case MapIconBrnBase::E_ICONSTATE_PLAYER_BLACK:
        case MapIconBrnBase::E_ICONSTATE_RIVAL_BLACK:   return KAV4_ICON_COLOUR_BLACK;
        default:                                        return KAV4_ICON_COLOUR_YELLOW;
        }
    }

    // RenderDriveThrough's per-state atlas rect (guarded statics flt_82FB37EC..flt_82FB3828
    // behind dword_82FB3834). All four are RECOVERED image values.
    const Vector4 KAV4_DRIVETHROUGH_UV_JUNKYARD   = { 0.59375f,  0.26171875f, 0.890625f, 0.625f };      // flt_82FB37FC
    const Vector4 KAV4_DRIVETHROUGH_UV_BODYSHOP   = { 0.296875f, 0.625f,      0.59375f,  0.98828125f }; // flt_82FB381C
    const Vector4 KAV4_DRIVETHROUGH_UV_GASSTATION = { 0.59375f,  0.625f,      0.890625f, 0.98828125f }; // flt_82FB380C
    const Vector4 KAV4_DRIVETHROUGH_UV_PAINTSHOP  = { 0.0f,      0.625f,      0.296875f, 0.98828125f }; // flt_82FB37EC

    // RenderStartFinish's two atlas rects (flt at sp; literals in the body).
    const Vector4 KAV4_STARTFINISH_UV_START  = { 0.25f, 0.5f, 0.5f,  0.75f };   // IconState 51
    const Vector4 KAV4_STARTFINISH_UV_FINISH = { 0.5f,  0.5f, 0.75f, 0.75f };   // IconState 52

    // The drive-through icon's static screen nudge: flt_82FB37E4 == KF_DRIVETHROUGH_HALFWIDTH
    // * -0.5, flt_82FB37E8 == 0 (the guarded init arm computes the first from the .rdata
    // half-width, so it inherits that constant's FLAG and nothing more).
    const f32 KF_DRIVETHROUGH_OFFSET_Y = 0.0f;   // flt_82FB37E8
}

// ---------------------------------------------------------------------------
// @0x8245DD28 -- RotatateRect (DWARF spelling, sic). Rotate the axis-aligned screen rect
// {x0,y0,x1,y1} about its own centre by lfRotationInRadians and emit the four corners in
// the buffer's strip order: TL, BL, TR, BR.
//
// The console evaluates the same eight products with cos in f31 and sin in f18 and the
// centre recomputed inline at every use; written once here.
// ---------------------------------------------------------------------------
void CrashNavIconRenderer::RotatateRect(Vector4 lv4Rect, f32 lfRotationInRadians,
                                        CgsGraphics::Vector2& lrv2TopLeft,
                                        CgsGraphics::Vector2& lrv2BottomLeft,
                                        CgsGraphics::Vector2& lrv2TopRight,
                                        CgsGraphics::Vector2& lrv2BottomRight)
{
    const f32 lfCos = static_cast<f32>(std::cos(static_cast<double>(lfRotationInRadians)));
    const f32 lfSin = static_cast<f32>(std::sin(static_cast<double>(lfRotationInRadians)));

    const f32 lfCentreX = lv4Rect.x + (lv4Rect.z - lv4Rect.x) * 0.5f;
    const f32 lfCentreY = lv4Rect.y + (lv4Rect.w - lv4Rect.y) * 0.5f;

    const f32 lfLeft   = lv4Rect.x - lfCentreX;
    const f32 lfRight  = lv4Rect.z - lfCentreX;
    const f32 lfTop    = lv4Rect.y - lfCentreY;
    const f32 lfBottom = lv4Rect.w - lfCentreY;

    // x' = centre.x + (x * cos - y * sin), y' = centre.y + (x * sin + y * cos)
    lrv2TopLeft.x     = lfCentreX + (lfLeft  * lfCos - lfTop    * lfSin);
    lrv2TopLeft.y     = lfCentreY + (lfLeft  * lfSin + lfTop    * lfCos);
    lrv2BottomLeft.x  = lfCentreX + (lfLeft  * lfCos - lfBottom * lfSin);
    lrv2BottomLeft.y  = lfCentreY + (lfLeft  * lfSin + lfBottom * lfCos);
    lrv2TopRight.x    = lfCentreX + (lfRight * lfCos - lfTop    * lfSin);
    lrv2TopRight.y    = lfCentreY + (lfRight * lfSin + lfTop    * lfCos);
    lrv2BottomRight.x = lfCentreX + (lfRight * lfCos - lfBottom * lfSin);
    lrv2BottomRight.y = lfCentreY + (lfRight * lfSin + lfBottom * lfCos);
}

// ---------------------------------------------------------------------------
// @0x8245DB90 -- RenderQuad. Four screen-space corners + one UV RECT {u0,v0,u1,v1} + one
// 0..1 colour -> a four-vertex triangle strip through the bound texture/blend states.
//
// The vertex/UV pairing is X360-verbatim (@0x8245DBB8-0x8245DC50):
//   v0 = TL, uv (u0,v0)   v1 = BL, uv (u0,v1)   v2 = TR, uv (u1,v0)   v3 = BR, uv (u1,v1)
// Primitive type 6 == triangle strip, the same literal every 2D quad in the tree submits.
//
// ⭐ PARAMETER ORDER CORRECTED 2026-08-29 (FIX2) to the DWARF row (h:575): colour 6th BY
// VALUE, then the two states, then the UV rect LAST. See the header for the register proof.
// ---------------------------------------------------------------------------
void CrashNavIconRenderer::RenderQuad(Im2dCommandBuffer* lpRenderBuffer,
                                      const CgsGraphics::Vector2& lrv2TopLeft,
                                      const CgsGraphics::Vector2& lrv2BottomLeft,
                                      const CgsGraphics::Vector2& lrv2TopRight,
                                      const CgsGraphics::Vector2& lrv2BottomRight,
                                      Vector4 lv4Colour,
                                      const renderengine::TextureState* lpTextureState,
                                      const renderengine::BlendState* lpBlendState,
                                      const Vector4& lrv4UvRect)
{
    const CgsGraphics::RGBA8 lColour = PackVertexColour(lv4Colour);

    CgsGraphics::Basic2dColouredTexturedVertex laVertices[4];

    laVertices[0].mv2Pos    = lrv2TopLeft;
    laVertices[0].mv4Colour = lColour;
    laVertices[0].mv2Tex0UV = MakeV2(lrv4UvRect.x, lrv4UvRect.y);

    laVertices[1].mv2Pos    = lrv2BottomLeft;
    laVertices[1].mv4Colour = lColour;
    laVertices[1].mv2Tex0UV = MakeV2(lrv4UvRect.x, lrv4UvRect.w);

    laVertices[2].mv2Pos    = lrv2TopRight;
    laVertices[2].mv4Colour = lColour;
    laVertices[2].mv2Tex0UV = MakeV2(lrv4UvRect.z, lrv4UvRect.y);

    laVertices[3].mv2Pos    = lrv2BottomRight;
    laVertices[3].mv4Colour = lColour;
    laVertices[3].mv2Tex0UV = MakeV2(lrv4UvRect.z, lrv4UvRect.w);

    lpRenderBuffer->SetState(lpTextureState);   // X360 Basic2dColouredTexturedVertex_::SetState
    lpRenderBuffer->SetState(lpBlendState);     // X360 sub_82458EC0
    lpRenderBuffer->Render(static_cast<renderengine::PrimitiveType>(6), laVertices, 4);
}

// ---------------------------------------------------------------------------
// DWARF cpp:2797 -- CalculateUVsForIndex. The atlas cell walk InitEventTypeUvs (core half)
// and RenderEventIcon both inline on the console: split liIndex into (row, column) by the
// page's columns-per-row and emit that cell's four corner UVs.
//
// The core half open-codes the same walk inside InitEventTypeUvs (which is where the X360
// actually inlines it); this out-of-line copy exists because the DWARF declares it and the
// header carries the declaration.
// ---------------------------------------------------------------------------
void CrashNavIconRenderer::CalculateUVsForIndex(s32 liIndex,
                                                CgsGraphics::Vector2& lrv2TopLeft,
                                                CgsGraphics::Vector2& lrv2BottomLeft,
                                                CgsGraphics::Vector2& lrv2TopRight,
                                                CgsGraphics::Vector2& lrv2BottomRight,
                                                f32 lfTextureWidth, f32 lfTextureHeight,
                                                f32 lfIconWidth, f32 lfIconHeight)
{
    // The console truncates the float ratio to an int and divides the frame index by it.
    const s32 liColumnsPerRow = static_cast<s32>(lfTextureWidth / lfIconWidth);
    const s32 liAtlasRow      = (liColumnsPerRow != 0) ? (liIndex / liColumnsPerRow) : 0;
    const s32 liAtlasCol      = (liColumnsPerRow != 0) ? (liIndex % liColumnsPerRow) : 0;

    const f32 lfInvW = 1.0f / lfTextureWidth;
    const f32 lfInvH = 1.0f / lfTextureHeight;

    const f32 lfU0 = static_cast<f32>(liAtlasCol) * lfIconWidth  * lfInvW;
    const f32 lfV0 = static_cast<f32>(liAtlasRow) * lfIconHeight * lfInvH;
    const f32 lfU1 = lfU0 + lfIconWidth  * lfInvW;
    const f32 lfV1 = lfV0 + lfIconHeight * lfInvH;

    lrv2TopLeft     = MakeV2(lfU0, lfV0);
    lrv2BottomLeft  = MakeV2(lfU0, lfV1);
    lrv2TopRight    = MakeV2(lfU1, lfV0);
    lrv2BottomRight = MakeV2(lfU1, lfV1);
}

// ---------------------------------------------------------------------------
// h:512 -- GetActiveIconType. The display type every icon pass keys off: while a cross-fade
// is running the FADING type wins, otherwise the live one. The console inlines this at the
// head of GetNumIcons (@0x82456C68) and GetIconInformation (@0x82456D80) as the identical
// `if (mbOldTypeFading == 1) ... else ...` pair.
// ---------------------------------------------------------------------------
GuiEventDrawEventIcons::EIconDisplayType CrashNavIconRenderer::GetActiveIconType() const
{
    if (mbOldTypeFading)
        return meFadingIconDisplayType;
    return meIconDisplayType;
}

// ---------------------------------------------------------------------------
// h:536 -- IsIgnoredIcon. Linear scan of the event's own ignore list (RenderIcons
// @0x8246A770-0x8246A7A8: the loop bound is re-read from miNumIconsToIgnore every
// iteration, reproduced).
// ---------------------------------------------------------------------------
bool CrashNavIconRenderer::IsIgnoredIcon(u32 luEventId) const
{
    for (s32 li = 0; li < miNumIconsToIgnore; ++li)
    {
        if (mauIconsToIgnore[li] == luEventId)
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// @0x82456C68 -- GetNumIcons. How many event icons the ACTIVE display type has.
// ---------------------------------------------------------------------------
u32 CrashNavIconRenderer::GetNumIcons() const
{
    switch (GetActiveIconType())
    {
    case GuiEventDrawEventIcons::E_ICON_DISPLAY_TYPE_OFFLINE_EVENTS:
        return mpGuiCache->GetNumProfileEvents();

    case GuiEventDrawEventIcons::E_ICON_DISPLAY_TYPE_ONLINE_EVENT_STARTS:
        return mpGuiCache->GetNumEventStarts();

    case GuiEventDrawEventIcons::E_ICON_DISPLAY_TYPE_ONLINE_CHECKPOINTS:
    {
        // The console clamps to the 2D icon bank's capacity (EventIconManager's own
        // KI_MAX_2DEVENTICONS == 175, the literal in the asm).
        s32 liCount = mpGuiCache->GetWorldDataController()->GetTotalNumberOfOnlineLandmarks();
        if (liCount >= EventIconManager::KI_MAX_2DEVENTICONS)
            liCount = EventIconManager::KI_MAX_2DEVENTICONS;
        return static_cast<u32>(liCount);
    }

    case GuiEventDrawEventIcons::E_ICON_DISPLAY_TYPE_ONLINE_FINISH_POINTS:
        return mpGuiCache->GetNumOnlineFinishPoints();

    case GuiEventDrawEventIcons::E_ICON_DISPLAY_TYPE_ONLINE_EVENT_PRESETS:
        return static_cast<u32>(mpGuiCache->GetNumPresetEvents());

    default:
        CGS_ASSERT(false, "Invalid icon display type in GetNumIcons");   // console line 899
        return 0;
    }
}

// ---------------------------------------------------------------------------
// @0x82456D80 -- GetIconInformation. Everything the icon pass needs about icon `luIndex` of
// the ACTIVE display type: is it filtered out, does it draw as a MINI icon, its event id,
// its icon-type row, its event-type column, and its world position.
//
// ⭐ UNBLOCKED 2026-08-29 (FIX2). The previous pass parked this as "needs a conductor link
// stub, blocked on four unrecoverable inputs". All four turned out to be recoverable, and
// three of them were already in the tree:
//
//  (a) dword_82055064[6], the OFFLINE arm's event-mode -> icon-column remap, IS in the raw
//      image (VA 0x82055064, file offset 0x55064, big-endian): { 3, 1, 5, 4, 2, 4 }. It is
//      byte-for-byte the SAME table as its sat-nav twin dword_82054E48, which the tree
//      already commits as BrnSatNavRenderer.cpp's KAU_EVENTTYPE_TO_ICONROW -- the earlier
//      note that "a different address means it cannot stand in" was right to refuse to guess
//      and wrong about the values: both are now READ, not assumed.
//
//  (b) GuiCache +0x9E58 / +0x9E5C are NOT unnamed: BrnGuiCache.h carves them as
//      meGameModeType (GetGameMode()) and muEventID (GetEventID()).
//
//  (c) "GuiCache +0x4AF8 / +0x4AFC, the counter pair whose equality promotes an event-type-4
//      icon to column 10" DOES NOT EXIST -- it is a Hex-Rays artefact. The asm
//      @0x82456F7C-0x82456F8C is
//          lwz r11, 0x90(r31)   ; mpGuiCache
//          ld  r10, 0x10(r3)    ; the EVENT RECORD's +0x10 doubleword
//          ld  r11, 0x4AF8(r11) ; the cache's mLocalPlayerOriginalCarId (one 8-byte CgsID)
//          cmpld cr6, r10, r11
//      i.e. ONE 64-bit compare of the event's +0x10 id against the player's original car id,
//      which the decompiler split into two bogus 32-bit reads of the same CgsID's halves.
//      This is the burning-route special case: a BURNING_ROUTE event whose car is the
//      player's own car is the CURRENT burning route, and takes column 10 instead of 4.
//
//  (d) the two "indexed cache accessors the header does not expose": GetOnlineFinishPoint IS
//      declared (X360 @0x82506940), and the event-start indexer is X360 @0x824F8830 -- the
//      address the cache header had mislabelled GetNumEventStarts. Both reachable by name
//      now; see the address-correction note in BrnGuiCache.h.
//
// ARGUMENT ORDER is the console's, pinned register-for-register: r26=lpbFiltered,
// r22=lpbUseMiniIcon, r28=lpuEventID, r24=lpeIconType, r25=lpuEventTypeIndex, r23=position.
// ---------------------------------------------------------------------------
void CrashNavIconRenderer::GetIconInformation(u32 luIndex, bool* lpbFiltered,
                                              bool* lpbUseMiniIcon, u32* lpuEventID,
                                              ECrashNavIconType* lpeIconType,
                                              u32* lpuEventTypeIndex,
                                              Vector3& lrv3Position) const
{
    CGS_ASSERT(lpbFiltered != 0, "lpbFiltered");     // console line 926
    CGS_ASSERT(lpuEventID != 0, "lpuEventID");       // console line 927
    CGS_ASSERT(lpeIconType != 0, "lpeIconType");     // console line 928

    switch (GetActiveIconType())
    {
    case GuiEventDrawEventIcons::E_ICON_DISPLAY_TYPE_OFFLINE_EVENTS:
    {
        const BrnProgression::ProfileEvent* lpProfileEvent = mpGuiCache->GetProfileEvent(luIndex);

        // [FLAG PC bring-up guard] the console derefs straight through; on this build the
        // WorldDataController's progression binding can still be staging, exactly as
        // BrnSatNavRenderer.cpp's RenderIconsForSatNav already documents and guards.
        // DELETE-WHEN the WDC data binding lands.
        if (lpProfileEvent == 0)
        {
            *lpbFiltered       = true;
            *lpbUseMiniIcon    = false;
            *lpuEventID        = 0;
            *lpeIconType       = E_CRASHNAVICON_EVENT_NOTATTEMPTED;
            *lpuEventTypeIndex = 0;
            lrv3Position.SetZero();
            return;
        }

        const u32 luEventId = lpProfileEvent->GetID();

        const SatNavEventDisplayInfo* lpDisplayInfo =
            mpGuiCache->GetProfileEventDisplayInfo(luEventId);
        const BrnProgression::RaceEventData* lpEventInfo =
            mpGuiCache->GetWorldDataController()->GetEventInfoFromEventId(luEventId);

        *lpuEventID = luEventId;

        if (lpEventInfo == 0 || lpDisplayInfo == 0)   // same bring-up guard
        {
            *lpbFiltered       = true;
            *lpbUseMiniIcon    = false;
            *lpeIconType       = E_CRASHNAVICON_EVENT_NOTATTEMPTED;
            *lpuEventTypeIndex = 0;
            lrv3Position.SetZero();
            return;
        }

        // ---- the filter arm -------------------------------------------------------------
        // The cache's game-mode word decides WHICH filter applies. -1 (no mode) and 15 mean
        // "we are on the map, filter by the renderer's own game-mode filter"; anything else
        // means "we are inside an event, so only THAT event's icon survives".
        const s32 liCacheGameMode = mpGuiCache->GetGameMode();
        const bool lbUseModeFilter = (liCacheGameMode == -1 || liCacheGameMode == 15);

        const u8 lu8EventMode = lpEventInfo->GetMode();

        if (lbUseModeFilter)
        {
            // Survives when the renderer's filter matches this event's mode, or when the
            // filter is E_MODE_COUNT (6) == "no filter".
            *lpbFiltered = !(meGameModeFilter == static_cast<s32>(lu8EventMode) ||
                             meGameModeFilter == 6);
        }
        else
        {
            *lpbFiltered = (luEventId != mpGuiCache->GetEventID());
        }

        *lpuEventTypeIndex = lu8EventMode;

        // ---- the icon-type row -----------------------------------------------------------
        // Completed if the player has a rank or non-rank win, or a special-event win on a
        // BURNING_ROUTE event. (The same predicate BrnSatNavRenderer.cpp uses, which tests the
        // REMAPPED row == 2 -- identical, since the remap sends mode 4 to row 2.)
        const u16 lu16Flags = lpProfileEvent->GetFlags();
        const bool lbCompleted =
            (lu16Flags & BrnProgression::ProfileEvent::E_FLAG_RANK_WIN) != 0 ||
            (lu16Flags & BrnProgression::ProfileEvent::E_FLAG_NON_RANK_WIN) != 0 ||
            ((lu16Flags & BrnProgression::ProfileEvent::E_FLAG_WON_SPECIAL_EVENT_BEFORE) != 0 &&
             lu8EventMode == BrnProgression::RaceEventData::E_MODE_BURNING_ROUTE);
        *lpeIconType = lbCompleted ? E_CRASHNAVICON_EVENT_COMPLETED
                                   : E_CRASHNAVICON_EVENT_NOTATTEMPTED;

        // ---- the CURRENT burning route gets its own column --------------------------------
        // FLAG (name, not value): the doubleword compared here is +0x10 on the event record,
        // which the tree exposes as GetEventInstanceId() -- the name the sat-nav wave gave it.
        // The console compares it against the cache's mLocalPlayerOriginalCarId, so on a
        // BURNING_ROUTE record that word is the event's REQUIRED CAR id. Same word either way;
        // the accessor name is the one the tree already commits.
        if (*lpuEventTypeIndex == BrnProgression::RaceEventData::E_MODE_BURNING_ROUTE &&
            lpEventInfo->GetEventInstanceId() == mpGuiCache->GetLocalPlayerOriginalCarId())
        {
            *lpuEventTypeIndex = 10;
        }

        lrv3Position = lpDisplayInfo->mv3Position;   // lvx128 r27 -> stvx128 r23

        // ---- the MINI-icon arm -------------------------------------------------------------
        // An icon that is neither the hovered one nor inside a mode-filtered view draws as the
        // small badge, through the mini-icon column remap. Two disjoint arms with the
        // console's own early returns (asm @0x82456FA0-0x8245701C).
        const u32 luColumn        = *lpuEventTypeIndex;
        const bool lbNotHovered   = (mHoveredEventIcon.muHoveredEventID != luEventId);
        const bool lbNoModeFilter = (meGameModeFilter == 6);

        if (luColumn < KU_ICON_EVENT_TYPE_ONLINE_OFFSET && lbNotHovered && lbNoModeFilter)
        {
            *lpuEventTypeIndex = KAU_EVENTMODE_TO_MINI_ICON_COLUMN[luColumn];
            *lpbUseMiniIcon    = true;
            return;
        }
        if (luColumn == 10 && lbNotHovered && lbNoModeFilter)
        {
            // The current burning route's mini badge is column 0 (`stw r30(=0), 0(r25)`) ==
            // E_CRASHNAVICON_EVENTTYPE_MINI_INDEX_CURRENT_BURNINGROUTE.
            *lpuEventTypeIndex = E_CRASHNAVICON_EVENTTYPE_MINI_INDEX_CURRENT_BURNINGROUTE;
            *lpbUseMiniIcon    = true;
            return;
        }
        *lpbUseMiniIcon = false;
        break;
    }

    case GuiEventDrawEventIcons::E_ICON_DISPLAY_TYPE_ONLINE_EVENT_STARTS:
    {
        const SatNavEventDisplayInfo* lpEventStart = mpGuiCache->GetEventStart(luIndex);

        // The console makes the online-event lookup for its asserts and drops the result.
        mpGuiCache->GetWorldDataController()->GetOnlineEventInfoFromEventId(
            lpEventStart->muEventInstanceId);

        *lpuEventID        = lpEventStart->muEventInstanceId;
        *lpeIconType       = E_CRASHNAVICON_EVENT_NOTATTEMPTED;
        *lpuEventTypeIndex = KU_ICON_EVENT_TYPE_ONLINE_OFFSET;   // li r10, 6

        // The five authored start-points Construct appended are keyed on the JUNCTION id
        // (`lwz r11, 0x14(r29)` -> the Array<int,5>::Contains stack argument).
        *lpbFiltered    = mOnlineStartpointsToIgnore.Contains(
                              static_cast<s32>(lpEventStart->muJunctionId));
        *lpbUseMiniIcon = false;

        lrv3Position = lpEventStart->mv3Position;
        break;
    }

    case GuiEventDrawEventIcons::E_ICON_DISPLAY_TYPE_ONLINE_CHECKPOINTS:
    {
        GuiEventUpdateSatNav::SatNavIconInfo lIconInfo;
        mpGuiCache->GetOnlineLandmarkInfoAtPositionInList(static_cast<s32>(luIndex), &lIconInfo);

        // `lhz r11, var_70 / extsh` -- the sign-extended landmark-index half-word @+0x20.
        *lpuEventID        = static_cast<u32>(static_cast<s32>(lIconInfo.GetLandmarkIndexHalf()));
        *lpbFiltered       = false;
        *lpeIconType       = E_CRASHNAVICON_EVENT_NOTATTEMPTED;
        *lpuEventTypeIndex = KU_ICON_EVENT_TYPE_ONLINE_OFFSET;
        *lpbUseMiniIcon    = false;

        CopyPositionLane(lrv3Position, lIconInfo.GetPositionLane());
        break;
    }

    case GuiEventDrawEventIcons::E_ICON_DISPLAY_TYPE_ONLINE_FINISH_POINTS:
    {
        GuiEventUpdateSatNav::SatNavIconInfo lIconInfo;
        mpGuiCache->GetOnlineFinishPoint(static_cast<s32>(luIndex), &lIconInfo);

        *lpuEventID        = static_cast<u32>(static_cast<s32>(lIconInfo.GetLandmarkIndexHalf()));
        *lpbFiltered       = false;
        *lpeIconType       = E_CRASHNAVICON_EVENT_NOTATTEMPTED;
        *lpbUseMiniIcon    = false;
        *lpuEventTypeIndex = KU_ICON_EVENT_TYPE_ONLINE_OFFSET;

        CopyPositionLane(lrv3Position, lIconInfo.GetPositionLane());
        break;
    }

    case GuiEventDrawEventIcons::E_ICON_DISPLAY_TYPE_ONLINE_EVENT_PRESETS:
    {
        const PresetEvent* lpPresetEvent = mpGuiCache->GetPresetEvent(static_cast<s32>(luIndex));
        const SatNavEventDisplayInfo* lpDisplayInfo =
            mpGuiCache->GetPresetEventDisplayInfo(lpPresetEvent->GetPositionLookupId());

        // Again made for its asserts; the result is dropped.
        mpGuiCache->GetWorldDataController()->GetOnlineEventInfoFromEventId(
            lpDisplayInfo->muEventInstanceId);

        // The console re-fetches the preset record for the id store (`bl GetPresetEvent`
        // twice); reproduced through the one pointer -- the accessor is pure.
        *lpuEventID        = lpPresetEvent->GetEventId();
        *lpbFiltered       = false;
        *lpeIconType       = E_CRASHNAVICON_EVENT_NOTATTEMPTED;
        *lpbUseMiniIcon    = false;
        *lpuEventTypeIndex = KU_ICON_EVENT_TYPE_ONLINE_OFFSET;

        lrv3Position = lpDisplayInfo->mv3Position;
        break;
    }

    default:
        CGS_ASSERT(false, "Invalid icon display type in GetIconInformation");   // console line 1101
        break;
    }
}

// ---------------------------------------------------------------------------
// @0x8245D8C0 -- RenderCursor. One quad for the map cursor, off the crash-nav icon page.
//
// The console's structure is an outer `if (miAnimationState != 4)` with the SAME test
// repeated on the non-hover arm (a compiler artefact of the shared tail); the inner test
// can never fail, so it is not reproduced. `miAnimationState = 4` at the end is the
// one-shot latch: the cursor draws once per published GuiEventMapCursorStatus (event 560),
// which the map component republishes every frame.
// ---------------------------------------------------------------------------
void CrashNavIconRenderer::RenderCursor(Im2dCommandBuffer* lpRenderBuffer)
{
    CGS_ASSERT(mpIconsTextureState != 0, "mpIconsTextureState");   // console line 1474

    if (mGuiEventMapCursorStatus.miAnimationState == 4)
        return;

    const f32 lfPosX = mGuiEventMapCursorStatus.mv2Position.x;   // device pixels
    const f32 lfPosY = mGuiEventMapCursorStatus.mv2Position.y;

    f32 lfHalfWidth;
    f32 lfHalfHeight;
    f32 lfCentreX;
    f32 lfCentreY;

    if (mGuiEventMapCursorStatus.miDisplayState == 0)
    {
        // The "snapped to an icon" cursor: it eases from half size up to full over
        // KF_SELECTED_GROW_TIME while the shared hover grow is running.
        lfHalfWidth  = KF_CURSOR_HOVER_HALFWIDTH;
        lfHalfHeight = KF_CURSOR_HOVER_HALFHEIGHT;
        lfCentreX    = KF_CURSOR_HOVER_OFFSET_X + lfPosX * KF_DEVICE_TO_NORMALISED_X;
        lfCentreY    = KF_CURSOR_HOVER_OFFSET_Y + lfPosY * KF_DEVICE_TO_NORMALISED_Y;

        const f32 lfNow = mpGuiCache->GetTime();
        if (mfHoveredIconScaleEndTime > lfNow && mfHoveredIconGrowing)
        {
            // lerp(0.5, 1.0, clamp01(1 - (end - now) * 4))
            const f32 lfT = Clamp01(1.0f - (mfHoveredIconScaleEndTime - lfNow) * KF_SELECTED_GROW_RATE);
            mfCursorScaleFactor = 0.5f + (1.0f - 0.5f) * lfT;
        }
        else
        {
            mfCursorScaleFactor = 1.0f;
        }
        lfHalfWidth  *= mfCursorScaleFactor;
        lfHalfHeight *= mfCursorScaleFactor;
    }
    else
    {
        // The free cursor: fixed size, its own screen nudge.
        lfHalfWidth  = KF_CURSOR_HALFWIDTH;
        lfHalfHeight = KF_CURSOR_HALFHEIGHT;
        lfCentreX    = KF_CURSOR_OFFSET_X + lfPosX * KF_DEVICE_TO_NORMALISED_X;
        lfCentreY    = KF_CURSOR_OFFSET_Y + lfPosY * KF_DEVICE_TO_NORMALISED_Y;
    }

    // The console builds the four vertices inline (it does NOT go through RenderQuad here)
    // with a hard 0xFFFFFFFF colour word -- opaque white in the same [r][g][b][a] order
    // RenderQuad packs.
    CgsGraphics::RGBA8 lColour;
    lColour.r = 255; lColour.g = 255; lColour.b = 255; lColour.a = 255;

    const f32 lfLeft   = lfCentreX - lfHalfWidth;
    const f32 lfRight  = lfCentreX + lfHalfWidth;
    const f32 lfTop    = lfCentreY - lfHalfHeight;
    const f32 lfBottom = lfCentreY + lfHalfHeight;

    CgsGraphics::Basic2dColouredTexturedVertex laVertices[4];
    laVertices[0].mv2Pos = MakeV2(lfLeft,  lfTop);
    laVertices[1].mv2Pos = MakeV2(lfLeft,  lfBottom);
    laVertices[2].mv2Pos = MakeV2(lfRight, lfTop);
    laVertices[3].mv2Pos = MakeV2(lfRight, lfBottom);
    laVertices[0].mv2Tex0UV = MakeV2(KAF_CURSOR_UV[0], KAF_CURSOR_UV[1]);
    laVertices[1].mv2Tex0UV = MakeV2(KAF_CURSOR_UV[0], KAF_CURSOR_UV[3]);
    laVertices[2].mv2Tex0UV = MakeV2(KAF_CURSOR_UV[2], KAF_CURSOR_UV[1]);
    laVertices[3].mv2Tex0UV = MakeV2(KAF_CURSOR_UV[2], KAF_CURSOR_UV[3]);
    for (s32 li = 0; li < 4; ++li)
        laVertices[li].mv4Colour = lColour;

    lpRenderBuffer->SetState(mpIconsTextureState);
    lpRenderBuffer->SetState(CgsGui::gpGuiBlendStateStandard);   // X360 dword_83010F20
    lpRenderBuffer->Render(static_cast<renderengine::PrimitiveType>(6), laVertices, 4);

    mGuiEventMapCursorStatus.miAnimationState = 4;
}

// ---------------------------------------------------------------------------
// @0x82469038 -- RenderDriveThroughs. One pass over the live crash-nav icon bank the map
// component published this frame (event 561): SORT each icon into the renderer's own
// per-frame banks by its IconState, and draw the drive-throughs as it goes.
//
//   states 1..25   (PLAYER_* / RIVAL_*)  -> copied into mRivalIcons
//   states 36..39  (the four drive-throughs) -> drawn now, UNLESS it is the hovered one,
//                  in which case its index is parked in miSelectedIndex so RenderIcons can
//                  redraw it LAST (on top of everything else)
//   states 51..52  (the custom-rendered start/finish pair) -> copied into mStartFinishIcons
//
// The console's loop bound is `while (v6 <= v3[114])` -- i.e. `<= liNumberOfIcons`, one
// past the published count. Reproduced as written: it is the shipped behaviour, and the
// bank is a fixed-capacity array whose slot past the count is live storage, not a fault.
// (Guarded below only by the same non-null test the console makes on the bank pointer.)
// ---------------------------------------------------------------------------
void CrashNavIconRenderer::RenderDriveThroughs(Im2dCommandBuffer* lpRenderBuffer, f32 lfScale)
{
    if (mGuiEventMapIconStatus.liNumberOfIcons <= 0 ||
        mGuiEventMapIconStatus.lpSatNavIcons == 0)
    {
        return;
    }

    for (s32 liIndex = 0; liIndex <= mGuiEventMapIconStatus.liNumberOfIcons; ++liIndex)
    {
        CrashNavMapIcon& lrIcon = mGuiEventMapIconStatus.lpSatNavIcons[liIndex].mIcon;   // bank + 496*i + 144

        const MapIconBrnBase::IconState leState = lrIcon.GetState();
        if (leState >= MapIconBrnBase::E_ICONSTATE_PLAYER_OFFLINE &&
            leState <= MapIconBrnBase::E_ICONSTATE_RIVAL_BLACK)
        {
            // states 1..25 -- a player or rival marker.
            CGS_ASSERT(miRivalIconsCount < KI_MAX_RIVALS,
                       "Trying to store too many rivals");   // console line 1652
            mRivalIcons[miRivalIconsCount] = lrIcon;
            ++miRivalIconsCount;
        }
        else if (leState >= MapIconBrnBase::E_ICONSTATE_CRASHNAV_JUNKYARD &&
                 leState <= MapIconBrnBase::E_ICONSTATE_CRASHNAV_PAINT_SHOP)
        {
            // states 36..39 -- a drive-through.
            if (lrIcon.muId == static_cast<u32>(mHoveredEventIcon.mHoveredDriveThroughID))
                miSelectedIndex = liIndex;   // deferred to the end of RenderIcons
            else
                RenderDriveThrough(lpRenderBuffer, liIndex, lfScale);
        }
        else if (leState == MapIconBrnBase::E_ICONSTATE_CRASHNAV_CUSTOMRENDERED_START_POINT ||
                 leState == MapIconBrnBase::E_ICONSTATE_CRASHNAV_CUSTOMRENDERED_FINISH_POINT)
        {
            // states 51..52 -- the pre-race start/finish pair.
            CGS_ASSERT(miStartFinishIconCount < KI_NUM_STARTFINISH_ICONS,
                       "Trying to add too many start finishes");   // console line 1663
            mStartFinishIcons[miStartFinishIconCount] = lrIcon;
            ++miStartFinishIconCount;
        }
    }
}

// ---------------------------------------------------------------------------
// @0x824639F8 -- RenderDriveThrough. One drive-through icon, atlas rect picked by its
// IconState, with the shared hover grow/shrink animation when it is the hovered one.
// ---------------------------------------------------------------------------
void CrashNavIconRenderer::RenderDriveThrough(Im2dCommandBuffer* lpRenderBuffer,
                                              s32 liIndex, f32 lfScale)
{
    CrashNavMapIcon& lrIcon = mGuiEventMapIconStatus.lpSatNavIcons[liIndex].mIcon;   // bank + 496*i + 144

    Vector4 lv4Uv;
    switch (lrIcon.GetState())
    {
    case MapIconBrnBase::E_ICONSTATE_CRASHNAV_JUNKYARD:    lv4Uv = KAV4_DRIVETHROUGH_UV_JUNKYARD;   break;
    case MapIconBrnBase::E_ICONSTATE_CRASHNAV_BODYSHOP:    lv4Uv = KAV4_DRIVETHROUGH_UV_BODYSHOP;   break;
    case MapIconBrnBase::E_ICONSTATE_CRASHNAV_GAS_STATION: lv4Uv = KAV4_DRIVETHROUGH_UV_GASSTATION; break;
    case MapIconBrnBase::E_ICONSTATE_CRASHNAV_PAINT_SHOP:  lv4Uv = KAV4_DRIVETHROUGH_UV_PAINTSHOP;  break;
    default:
        return;   // the console's `default: goto <epilogue>` -- draw nothing
    }

    // flt_82FB37E4 == KF_DRIVETHROUGH_HALFWIDTH * -0.5, flt_82FB37E8 == 0.
    const Vector2 lv2Position = lrIcon.GetPosition();   // device pixels
    f32 lfCentreX = lv2Position.x * KF_DEVICE_TO_NORMALISED_X + (KF_DRIVETHROUGH_HALFWIDTH * -0.5f);
    f32 lfCentreY = lv2Position.y * KF_DEVICE_TO_NORMALISED_Y + KF_DRIVETHROUGH_OFFSET_Y;

    f32 lfHalfWidth;
    f32 lfHalfHeight;

    if (lrIcon.muId != static_cast<u32>(mHoveredEventIcon.mHoveredDriveThroughID))
    {
        lfHalfWidth  = KF_DRIVETHROUGH_HALFWIDTH  * lfScale;
        lfHalfHeight = KF_DRIVETHROUGH_HALFHEIGHT * lfScale;
    }
    else
    {
        const f32 lfNow = mpGuiCache->GetTime();

        // A NEW hover restarts the grow leg.
        if (static_cast<u32>(mHoveredEventIconLastFrame.mHoveredDriveThroughID) !=
            static_cast<u32>(mHoveredEventIcon.mHoveredDriveThroughID))
        {
            mfHoveredIconScaleFactor  = 1.0f;
            mfHoveredIconGrowing      = true;
            mfHoveredIconScaleEndTime = lfNow + KF_SELECTED_GROW_TIME;
        }

        f32 lfLift;
        if (mfHoveredIconScaleEndTime <= lfNow)
        {
            // Settled: full lift, and the grow leg hands over to the shrink leg once.
            lfLift = KF_DRIVETHROUGH_HOVER_LIFT * lfScale;
            if (mfHoveredIconGrowing)
            {
                mfHoveredIconGrowing      = false;
                mfHoveredIconScaleEndTime = lfNow + KF_SELECTED_SHRINK_TIME;
            }
            lfCentreY += lfLift;
        }
        else if (mfHoveredIconGrowing)
        {
            // Growing: the icon lifts off the map as it swells 2.2 -> 1.0.
            const f32 lfT = Clamp01(1.0f - (mfHoveredIconScaleEndTime - lfNow) * KF_SELECTED_GROW_RATE);
            lfLift = (KF_DRIVETHROUGH_HOVER_LIFT - KF_DRIVETHROUGH_HOVER_LIFT * lfT) * lfScale;
            mfHoveredIconScaleFactor = 2.2f - lfT * 1.2f;
            lfCentreY += lfLift;
        }
        else
        {
            // Settling: already lifted, easing 2.2 -> 2.0.
            lfCentreY += KF_DRIVETHROUGH_HOVER_LIFT * lfScale;
            const f32 lfT = Clamp01(1.0f - (mfHoveredIconScaleEndTime - lfNow) * KF_SELECTED_SHRINK_RATE);
            mfHoveredIconScaleFactor = 2.0f + lfT * 0.20000005f;
        }

        lfHalfWidth  = mfHoveredIconScaleFactor * KF_DRIVETHROUGH_HALFWIDTH  * lfScale;
        lfHalfHeight = mfHoveredIconScaleFactor * KF_DRIVETHROUGH_HALFHEIGHT * lfScale;
    }

    const f32 lfLeft   = lfCentreX - lfHalfWidth;
    const f32 lfRight  = lfCentreX + lfHalfWidth;
    const f32 lfTop    = lfCentreY - lfHalfHeight;
    const f32 lfBottom = lfCentreY + lfHalfHeight;

    RenderQuad(lpRenderBuffer,
               MakeV2(lfLeft,  lfTop),      // TL
               MakeV2(lfLeft,  lfBottom),   // BL
               MakeV2(lfRight, lfTop),      // TR
               MakeV2(lfRight, lfBottom),   // BR
               OpaqueWhite(),
               mpIconsTextureState,
               CgsGui::gpGuiBlendStateStandard,   // X360 dword_83010F20
               lv4Uv);
}

// ---------------------------------------------------------------------------
// @0x824692D8 -- RenderRoadSigns. Walk the 65 authored road-sign records; draw every one
// whose road id is NOT the hovered road, and park the hovered one's index in
// miSelectedIndex so RenderIcons redraws it last (on top).
//
// The console copy-constructs each record onto the stack first (RoadSign::RoadSign
// @0x8244BAD8) purely to reach mpcRoadId; on the host the record is read in place.
// The gate is the published bank pointer, not the list itself: with no
// GuiEventRoadSignIconStatus this frame there are no icon records to key off.
// ---------------------------------------------------------------------------
void CrashNavIconRenderer::RenderRoadSigns(Im2dCommandBuffer* lpRenderBuffer)
{
    if (mRoadSignIconStatus.mpRoadSignIcons == 0)
        return;

    for (s32 liIndex = 0; liIndex < RoadSignList::KI_NUM_ROAD_SIGNS; ++liIndex)
    {
        const RoadSign& lrSign = mRoadSignList.maRoadSigns[liIndex];

        // strcmp(sign.mpcRoadId, mHoveredEventIcon.mpcHoveredRoadName); a null hovered
        // name short-circuits straight to "draw it" (the console's `if (!v6) goto draw`).
        bool lbIsHovered = false;
        if (mHoveredEventIcon.mpcHoveredRoadName != 0)
        {
            const char* lpcSign    = lrSign.mpcRoadId;
            const char* lpcHovered = mHoveredEventIcon.mpcHoveredRoadName;
            s32 liDelta = 0;
            for (;;)
            {
                liDelta = static_cast<s32>(*lpcSign) - static_cast<s32>(*lpcHovered);
                if (*lpcSign == 0 || liDelta != 0)
                    break;
                ++lpcSign;
                ++lpcHovered;
            }
            lbIsHovered = (liDelta == 0);
        }

        if (lbIsHovered)
            miSelectedIndex = liIndex;   // deferred to the end of RenderIcons
        else
            RenderRoadSign(lpRenderBuffer, liIndex);
    }
}

// ---------------------------------------------------------------------------
// @0x82465210 -- RenderStartFinish. The pre-race start and finish markers, from the bank
// RenderDriveThroughs filled this frame. Both quads are a fixed 64x64 device-pixel square
// nudged up and to the left of the marker's own device position; the atlas rect is picked
// by which of the two custom-rendered states the icon carries.
// ---------------------------------------------------------------------------
void CrashNavIconRenderer::RenderStartFinish(Im2dCommandBuffer* lpRenderBuffer)
{
    for (s32 liIndex = 0; liIndex < miStartFinishIconCount; ++liIndex)
    {
        CrashNavMapIcon& lrIcon = mStartFinishIcons[liIndex];

        Vector4 lv4Uv;
        const MapIconBrnBase::IconState leState = lrIcon.GetState();
        if (leState == MapIconBrnBase::E_ICONSTATE_CRASHNAV_CUSTOMRENDERED_START_POINT)
            lv4Uv = KAV4_STARTFINISH_UV_START;
        else if (leState == MapIconBrnBase::E_ICONSTATE_CRASHNAV_CUSTOMRENDERED_FINISH_POINT)
            lv4Uv = KAV4_STARTFINISH_UV_FINISH;
        else
            continue;   // the console's fall-through: neither arm ran, nothing drawn

        const Vector2 lv2Position = lrIcon.GetPosition();   // device pixels

        const f32 lfLeft = (lv2Position.x - KF_STARTFINISH_HALFWIDTH + KF_STARTFINISH_OFFSET_X) *
                           KF_DEVICE_TO_NORMALISED_X;
        const f32 lfRight = (lv2Position.x + KF_STARTFINISH_HALFWIDTH + KF_STARTFINISH_OFFSET_X) *
                            KF_DEVICE_TO_NORMALISED_X;
        const f32 lfTop = (lv2Position.y - KF_STARTFINISH_HALFHEIGHT + KF_STARTFINISH_OFFSET_Y) *
                          KF_DEVICE_TO_NORMALISED_Y;
        const f32 lfBottom = (lv2Position.y + KF_STARTFINISH_HALFHEIGHT + KF_STARTFINISH_OFFSET_Y) *
                             KF_DEVICE_TO_NORMALISED_Y;

        RenderQuad(lpRenderBuffer,
                   MakeV2(lfLeft,  lfTop),      // TL
                   MakeV2(lfLeft,  lfBottom),   // BL
                   MakeV2(lfRight, lfTop),      // TR
                   MakeV2(lfRight, lfBottom),   // BR
                   OpaqueWhite(),
                   // ⭐ ATLAS CORRECTED 2026-08-29 (FIX2). This bound mpIconsTextureState
                   // (member +0x110, GUITEXTURES id 206). The console loads
                   // `lwz r9, 0xF4(r29)` @0x824653A8 with r29 = r3 = this, and +0xF4 is
                   // mapIconTextureStates[0] == E_CRASHNAVICON_EVENT_NOTATTEMPTED (id 204).
                   // Compare RenderCursor (0x8245D8E4/0x8245DB0C) and RenderDriveThrough
                   // (0x82463E3C), which DO load 0x110 -- so this is a per-call-site bind,
                   // not a global fold. With the wrong page both start/finish markers
                   // sampled UVs authored for another atlas.
                   mapIconTextureStates[E_CRASHNAVICON_EVENT_NOTATTEMPTED],
                   CgsGui::gpGuiBlendStateStandard,
                   lv4Uv);
    }
}

// ---------------------------------------------------------------------------
// @0x82465468 -- RenderRivals. The player and rival markers, from the bank
// RenderDriveThroughs filled this frame. Each is one rotated, alpha-scaled quad in its
// team colour; the LOCAL player additionally gets a pulsing halo quad drawn underneath it.
//
// The eleven team colours and the 32x32 half-extents are recovered function-local statics
// (their init arms carry the literals); only KF_PLAYER_ICON_PULSE_PERIOD is .rdata.
//
// NOTE the shared hover state this function drives: it is the same
// mfHoveredIconScaleFactor / mfHoveredIconScaleEndTime / mfHoveredIconGrowing triple that
// RenderCursor and RenderDriveThrough read, keyed here on mHoveredPlayerID.
// ---------------------------------------------------------------------------
void CrashNavIconRenderer::RenderRivals(Im2dCommandBuffer* lpRenderBuffer)
{
    for (s32 liIndex = 0; liIndex < miRivalIconsCount; ++liIndex)
    {
        CrashNavMapIcon& lrIcon = mRivalIcons[liIndex];

        bool lbIsLocalPlayer = false;
        const Vector4 lv4IconColour = GetRivalIconColour(lrIcon.GetState(), &lbIsLocalPlayer);

        const f32 lfNow = mpGuiCache->GetTime();

        // ---- the shared hover latch, keyed on the hovered PLAYER id ----------------
        if (static_cast<u32>(mHoveredEventIconLastFrame.mHoveredPlayerID) ==
            static_cast<u32>(mHoveredEventIcon.mHoveredPlayerID))
        {
            if (mfHoveredIconScaleEndTime < lfNow &&
                mHoveredEventIconLastFrame.mHoveredPlayerID != 0)
            {
                mfHoveredIconScaleFactor = 1.0f;
                mfHoveredIconGrowing     = false;
            }
        }
        else
        {
            mfHoveredIconScaleFactor  = 1.0f;
            mfHoveredIconGrowing      = true;
            mfHoveredIconScaleEndTime = lfNow + KF_SELECTED_GROW_TIME;
        }

        const Vector2 lv2Position = lrIcon.GetPosition();   // device pixels
        const f32 lfLeft   = lv2Position.x - KF_RIVAL_HALFWIDTH;
        const f32 lfRight  = lv2Position.x + KF_RIVAL_HALFWIDTH;
        const f32 lfTop    = lv2Position.y - KF_RIVAL_HALFHEIGHT;
        const f32 lfBottom = lv2Position.y + KF_RIVAL_HALFHEIGHT;

        // The icon's own colour takes its ALPHA from the icon's fade (icon alpha is
        // authored 0..100; the console scales by 0.01).
        Vector4 lv4MainColour = lv4IconColour;
        lv4MainColour.w = lrIcon.GetAlpha() * 0.0099999998f;

        CgsGraphics::Vector2 lv2TL;
        CgsGraphics::Vector2 lv2BL;
        CgsGraphics::Vector2 lv2TR;
        CgsGraphics::Vector2 lv2BR;

        // ---- the local player's pulsing halo, UNDER the icon -----------------------
        if (lbIsLocalPlayer)
        {
            if (mfPlayerIconPulseEndTime <= lfNow)
            {
                mfPlayerIconPulseScale   = 1.0f;
                mfPlayerIconPulseEndTime = lfNow + KF_PLAYER_ICON_PULSE_PERIOD;
            }
            else
            {
                // The halo expands 1.0 -> 2.5 across the period...
                const f32 lfT = Clamp01(1.0f -
                    (mfPlayerIconPulseEndTime - lfNow) / KF_PLAYER_ICON_PULSE_PERIOD);
                mfPlayerIconPulseScale = 2.5f - lfT * 1.5f;
            }

            // ...and fades out as it does (alpha = 2 - scale).
            Vector4 lv4HaloColour = lv4IconColour;
            lv4HaloColour.w = 2.0f - mfPlayerIconPulseScale;

            const f32 lfHaloHalfWidth  = mfPlayerIconPulseScale * KF_RIVAL_HALFWIDTH;
            const f32 lfHaloHalfHeight = mfPlayerIconPulseScale * KF_RIVAL_HALFHEIGHT;

            RotatateRect(MakeV4(lv2Position.x - lfHaloHalfWidth,
                                lv2Position.y - lfHaloHalfHeight,
                                lv2Position.x + lfHaloHalfWidth,
                                lv2Position.y + lfHaloHalfHeight),
                         lrIcon.GetRotation(), lv2TL, lv2BL, lv2TR, lv2BR);

            lv2TL.x *= KF_DEVICE_TO_NORMALISED_X;  lv2TL.y *= KF_DEVICE_TO_NORMALISED_Y;
            lv2BL.x *= KF_DEVICE_TO_NORMALISED_X;  lv2BL.y *= KF_DEVICE_TO_NORMALISED_Y;
            lv2TR.x *= KF_DEVICE_TO_NORMALISED_X;  lv2TR.y *= KF_DEVICE_TO_NORMALISED_Y;
            lv2BR.x *= KF_DEVICE_TO_NORMALISED_X;  lv2BR.y *= KF_DEVICE_TO_NORMALISED_Y;

            // FLAG (UV): the console passes the halo's atlas rect on the stack, in the
            // slot the export's argument list truncates. It is the SAME crash-nav icon
            // page and the same cell as the icon quad below -- the two calls differ only
            // in the vector register (v1) they hand the colour in -- so the full-cell
            // rect is reproduced for both.
            // ⭐ ATLAS CORRECTED 2026-08-29 (FIX2): the console loads `lwz r9, 0xF4(r29)`
            // @0x82465C78 -- mapIconTextureStates[0], id 204 -- not +0x110 (id 206).
            RenderQuad(lpRenderBuffer, lv2TL, lv2BL, lv2TR, lv2BR,
                       lv4HaloColour,
                       mapIconTextureStates[E_CRASHNAVICON_EVENT_NOTATTEMPTED],
                       CgsGui::gpGuiBlendStateStandard,
                       MakeV4(0.0f, 0.0f, 1.0f, 1.0f));
        }

        // ---- the icon itself -------------------------------------------------------
        RotatateRect(MakeV4(lfLeft, lfTop, lfRight, lfBottom),
                     lrIcon.GetRotation(), lv2TL, lv2BL, lv2TR, lv2BR);

        lv2TL.x *= KF_DEVICE_TO_NORMALISED_X;  lv2TL.y *= KF_DEVICE_TO_NORMALISED_Y;
        lv2BL.x *= KF_DEVICE_TO_NORMALISED_X;  lv2BL.y *= KF_DEVICE_TO_NORMALISED_Y;
        lv2TR.x *= KF_DEVICE_TO_NORMALISED_X;  lv2TR.y *= KF_DEVICE_TO_NORMALISED_Y;
        lv2BR.x *= KF_DEVICE_TO_NORMALISED_X;  lv2BR.y *= KF_DEVICE_TO_NORMALISED_Y;

        // ⭐ ATLAS CORRECTED 2026-08-29 (FIX2): `lwz r9, 0xF4(r29)` @0x82465D54.
        RenderQuad(lpRenderBuffer, lv2TL, lv2BL, lv2TR, lv2BR,
                   lv4MainColour,
                   mapIconTextureStates[E_CRASHNAVICON_EVENT_NOTATTEMPTED],
                   CgsGui::gpGuiBlendStateStandard,
                   MakeV4(0.0f, 0.0f, 1.0f, 1.0f));
    }
}

// ---------------------------------------------------------------------------
// @0x8245D210 -- RenderEventIcon. One event icon: place it, cull it against the map's view
// rect, draw its atlas cell, and publish its DEVICE-space position into the caller's
// EventIcon2D run (which RenderIcons hands on to the manager's 2D icon bank).
//
// The four f32* are the caller's once-per-frame half-extent tables, indexed by
// leIconType; the "mini" pair is taken when lbUseMiniIcon is raised.
// ---------------------------------------------------------------------------
void CrashNavIconRenderer::RenderEventIcon(Im2dCommandBuffer* lpRenderBuffer,
                                           bool lbUseMiniIcon, bool lbFiltered,
                                           ECrashNavIconType leIconType,
                                           u32 luEventId, u32 luEventTypeIndex,
                                           Vector3 lv3Position,
                                           const f32* lpafMiniHalfWidth,
                                           const f32* lpafMiniHalfHeight,
                                           const f32* lpafHalfWidth,
                                           const f32* lpafHalfHeight,
                                           u32 luColour,
                                           EventIconManager::EventIcon2D* lpaIcon2D,
                                           s32* lpiIconCount)
{
    f32 lfHalfWidth;
    f32 lfHalfHeight;

    if (lbUseMiniIcon)
    {
        lfHalfWidth  = lpafMiniHalfWidth[leIconType];
        lfHalfHeight = lpafMiniHalfHeight[leIconType];
    }
    else
    {
        lfHalfWidth  = lpafHalfWidth[leIconType];
        lfHalfHeight = lpafHalfHeight[leIconType];
    }

    // The OFFLINE-events view grows the hovered event's icon with the shared hover
    // animation. (`!v56 && a6 == *(a1 + 352)`: the ACTIVE display type is
    // OFFLINE_EVENTS and this is the hovered event id.)
    if (!lbUseMiniIcon &&
        GetActiveIconType() == GuiEventDrawEventIcons::E_ICON_DISPLAY_TYPE_OFFLINE_EVENTS &&
        luEventId == mHoveredEventIcon.muHoveredEventID)
    {
        const f32 lfNow = mpGuiCache->GetTime();

        // A NEW hover restarts the grow leg (from half size, so the icon pops).
        if (mHoveredEventIconLastFrame.muHoveredEventID != mHoveredEventIcon.muHoveredEventID)
        {
            mfHoveredIconScaleFactor  = 0.5f;
            mfHoveredIconGrowing      = true;
            mfHoveredIconScaleEndTime = lfNow + KF_SELECTED_GROW_TIME;
        }

        if (mfHoveredIconScaleEndTime <= lfNow)
        {
            if (mfHoveredIconGrowing)
            {
                mfHoveredIconGrowing      = false;
                mfHoveredIconScaleEndTime = lfNow + KF_SELECTED_SHRINK_TIME;
            }
        }
        else if (mfHoveredIconGrowing)
        {
            // lerp(1.1, 0.5, ...) run the console's way round: base 1.1, delta (0.5 - 1.1).
            const f32 lfT = Clamp01(1.0f -
                (mfHoveredIconScaleEndTime - lfNow) * KF_SELECTED_GROW_RATE);
            mfHoveredIconScaleFactor = 1.1f + (0.5f - 1.1f) * (1.0f - lfT);
        }
        else
        {
            // Settling back: base 1.0, delta (1.1 - 1.0).
            const f32 lfT = Clamp01(1.0f -
                (mfHoveredIconScaleEndTime - lfNow) * KF_SELECTED_SHRINK_RATE);
            mfHoveredIconScaleFactor = 1.0f + (1.1f - 1.0f) * (1.0f - lfT);
        }

        lfHalfWidth  *= mfHoveredIconScaleFactor;
        lfHalfHeight *= mfHoveredIconScaleFactor;
    }

    // ---- world -> the map's on-screen view rect --------------------------------------
    // The console builds MakeCoordSpaceFromRect(mv4MapRect) and
    // MakeCoordSpaceFromRect(mv4ViewRect) inline as two VMX 3x3s and hands both to
    // Transform(point, from, to) == inverse(from) o to. The (x, z) flatten is the
    // unk_82CDA450 vperm lane pick -- the same one BrnSatNavRenderer.cpp documents.
    Vector2 lv2Flat;
    lv2Flat.x = lv3Position.x;
    lv2Flat.y = lv3Position.z;
    lv2Flat.z = 0.0f;
    lv2Flat.w = 0.0f;

    const Vector2 lv2Screen = MapTransform::Transform(
        lv2Flat,
        MapTransform::MakeCoordSpaceFromRect(mRenderMainMapEvent.mv4MapRect),
        MapTransform::MakeCoordSpaceFromRect(mRenderMainMapEvent.mv4ViewRect));

    const f32 lfLeft   = lv2Screen.x - lfHalfWidth;
    const f32 lfRight  = lv2Screen.x + lfHalfWidth;
    const f32 lfTop    = lv2Screen.y - lfHalfHeight;
    const f32 lfBottom = lv2Screen.y + lfHalfHeight;

    // ---- the four-way overlap test against the view rect ------------------------------
    // (@0x8245D5xx: four vcmpgtfp. against lanes 0..3 of mv4ViewRect, chained; the whole
    // chain is guarded on top by `!lbFiltered`.)
    if (!(lfRight > mRenderMainMapEvent.mv4ViewRect.x)) return;
    if (!(mRenderMainMapEvent.mv4ViewRect.z > lfLeft))  return;
    if (!(lfBottom > mRenderMainMapEvent.mv4ViewRect.y)) return;
    if (!(mRenderMainMapEvent.mv4ViewRect.w > lfTop))    return;
    if (lbFiltered) return;

    // ---- the atlas cell ---------------------------------------------------------------
    // The console indexes the pre-built UV tables directly (the mini tables have 6 columns
    // per icon-type row, the big tables KU_ICON_EVENT_TYPE_COUNT).
    CgsGraphics::Vector2 lv2UvTL;
    CgsGraphics::Vector2 lv2UvBL;
    CgsGraphics::Vector2 lv2UvTR;
    CgsGraphics::Vector2 lv2UvBR;
    if (lbUseMiniIcon)
    {
        lv2UvTL = mav2MiniIconUvTopLeft    [leIconType][luEventTypeIndex];
        lv2UvBL = mav2MiniIconUvBottomLeft [leIconType][luEventTypeIndex];
        lv2UvTR = mav2MiniIconUvTopRight   [leIconType][luEventTypeIndex];
        lv2UvBR = mav2MiniIconUvBottomRight[leIconType][luEventTypeIndex];
    }
    else
    {
        lv2UvTL = mav2IconUvTopLeft    [leIconType][luEventTypeIndex];
        lv2UvBL = mav2IconUvBottomLeft [leIconType][luEventTypeIndex];
        lv2UvTR = mav2IconUvTopRight   [leIconType][luEventTypeIndex];
        lv2UvBR = mav2IconUvBottomRight[leIconType][luEventTypeIndex];
    }

    // The console builds the strip inline here rather than through RenderQuad, because it
    // carries FOUR independent corner UVs (RenderQuad only takes a rect) and a
    // pre-packed colour word.
    const CgsGraphics::RGBA8 lColour = UnpackConsoleColour(luColour);

    CgsGraphics::Basic2dColouredTexturedVertex laVertices[4];
    laVertices[0].mv2Pos = MakeV2(lfLeft,  lfTop);     laVertices[0].mv2Tex0UV = lv2UvTL;
    laVertices[1].mv2Pos = MakeV2(lfLeft,  lfBottom);  laVertices[1].mv2Tex0UV = lv2UvBL;
    laVertices[2].mv2Pos = MakeV2(lfRight, lfTop);     laVertices[2].mv2Tex0UV = lv2UvTR;
    laVertices[3].mv2Pos = MakeV2(lfRight, lfBottom);  laVertices[3].mv2Tex0UV = lv2UvBR;
    for (s32 li = 0; li < 4; ++li)
        laVertices[li].mv4Colour = lColour;

    lpRenderBuffer->SetState(mapIconTextureStates[leIconType]);
    lpRenderBuffer->SetState(CgsGui::gpGuiBlendStateStandard);
    lpRenderBuffer->Render(static_cast<renderengine::PrimitiveType>(6), laVertices, 4);

    // ---- publish the icon's DEVICE-space position -------------------------------------
    // The second Transform hop is normalised-space -> device-space (the console passes
    // &smm33NormalisedSpace and &smm33DeviceSpace by address); the run is the caller's
    // scratch, appended in place with its own count.
    const Vector2 lv2Device = MapTransform::Transform(
        lv2Screen, MapTransform::GetNormalisedSpace(), MapTransform::GetDeviceSpace());

    lpaIcon2D[*lpiIconCount].muEventID       = luEventId;
    lpaIcon2D[*lpiIconCount].mfEventIconPosX = lv2Device.x;
    lpaIcon2D[*lpiIconCount].mfEventIconPosY = lv2Device.y;
    ++(*lpiIconCount);
}

// ---------------------------------------------------------------------------
// @0x8246A410 -- RenderIcons. The whole in-mask draw list, in the console's order:
//   the event icons (with the cross-fade alpha)      -> RenderEventIcon
//   the map furniture, MAINMAP view only             -> RenderDriveThroughs / RenderRoadSigns / RenderCursor
//   the ONE deferred hovered thing, on top           -> RenderDriveThrough / RenderEventIcon / RenderRoadSign
//   the start/finish pair, then the rivals           -> RenderStartFinish / RenderRivals
//   publish the 2D icon run, then roll the per-frame state
// ---------------------------------------------------------------------------
void CrashNavIconRenderer::RenderIcons(Im2dCommandBuffer* lpRenderBuffer)
{
    CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");           // console line 1120
    CGS_ASSERT(lpRenderBuffer != 0, "lpRenderBuffer");   // console line 1121
    CGS_ASSERT(mRenderMainMapEvent.mfZoomLevel != 0.0f,
               "mRenderMainMapEvent.mfZoomLevel != 0.0f");   // console line 1122

    const f32 lfZoom = mRenderMainMapEvent.mfZoomLevel;

    miSelectedIndex        = -1;
    miRivalIconsCount      = 0;
    miStartFinishIconCount = 0;

    // The drive-through / road-sign scale, and the event-icon zoom band.
    const f32 lfIconScale = 1.0f / (lfZoom * (1.0f / KF_ICON_ZOOM_REFERENCE));
    f32 lfBandedZoom = lfZoom;
    if (lfBandedZoom < KF_ICON_ZOOM_REFERENCE) lfBandedZoom = KF_ICON_ZOOM_REFERENCE;
    if (lfBandedZoom > KF_ICON_ZOOM_MAX)       lfBandedZoom = KF_ICON_ZOOM_MAX;
    const f32 lfInvBandedZoom = 1.0f / lfBandedZoom;

    // The four half-extent tables, scaled once for the whole frame.
    f32 lafHalfWidth     [E_CRASHNAVICON_NUM];
    f32 lafHalfHeight    [E_CRASHNAVICON_NUM];
    f32 lafMiniHalfWidth [E_CRASHNAVICON_NUM];
    f32 lafMiniHalfHeight[E_CRASHNAVICON_NUM];
    for (s32 liType = 0; liType < E_CRASHNAVICON_NUM; ++liType)
    {
        lafHalfWidth     [liType] = KAF_ICON_HALFWIDTH      [liType] * lfInvBandedZoom;
        lafHalfHeight    [liType] = KAF_ICON_HALFHEIGHT     [liType] * lfInvBandedZoom;
        lafMiniHalfWidth [liType] = KAF_MINI_ICON_HALFWIDTH [liType] * lfInvBandedZoom;
        lafMiniHalfHeight[liType] = KAF_MINI_ICON_HALFHEIGHT[liType] * lfInvBandedZoom;
    }

    EventIconManager::EventIcon2D la2DIcons[EventIconManager::KI_MAX_2DEVENTICONS];
    s32 li2DIconCount = 0;

    u32 luIconColour = KU_ICON_BASE_COLOUR;

    // ---- the event-icon pass -----------------------------------------------------------
    if (mbRenderEventStarts)
    {
        const u32 luNumIcons = GetNumIcons();

        // The cross-fade alpha (only the colour word's low/alpha byte is ever rewritten).
        if (mbOldTypeFading)
        {
            const f32 lfNow = mpGuiCache->GetTime();   // carries the :250 -FLT_MAX assert
            if (lfNow > mfIconFadeEndTime)
            {
                mfIconFadeEndTime       = 0.0f;
                mbOldTypeFading         = false;
                meFadingIconDisplayType = GuiEventDrawEventIcons::E_ICON_DISPLAY_TYPE_COUNT;
            }
            else
            {
                CGS_ASSERT(lfNow >= mfIconFadeStartTime,
                           "lfCurrentTime >= mfIconFadeStartTime");   // console line 1182
                const f32 lfT = (lfNow - mfIconFadeStartTime) /
                                (mfIconFadeEndTime - mfIconFadeStartTime);
                // The console's two fsel-clamped arms. (The export's operand order for the
                // second arm's fsel pair is garbled -- Hex-Rays splits it across a reused
                // f0 -- but the two legs are unambiguous: fade IN when the incoming set is
                // being switched on, OUT otherwise.)
                const f32 lfAlpha = (mbRenderEventStarts ? Clamp01(lfT) : (1.0f - Clamp01(lfT))) * 255.0f;
                luIconColour = (luIconColour & 0xFFFFFF00u) |
                               static_cast<u32>(static_cast<s32>(lfAlpha));
            }
        }
        else if (!mRenderMainMapEvent.mbIsActive)
        {
            // The map is not the active view: the icons go half-alpha
            // (`clrrwi r11, r20, 7 / ori r11, 0x80` -- the byte lands on exactly 0x80).
            luIconColour = (luIconColour & 0xFFFFFF00u) | 0x80u;
        }

        for (u32 luIndex = 0; luIndex < luNumIcons; ++luIndex)
        {
            bool              lbFiltered      = false;
            bool              lbUseMiniIcon   = false;
            u32               luEventId       = 0;
            ECrashNavIconType leIconType      = E_CRASHNAVICON_EVENT_NOTATTEMPTED;
            u32               luEventTypeIndex = 0;
            Vector3           lv3Position;

            GetIconInformation(luIndex, &lbFiltered, &lbUseMiniIcon, &luEventId,
                               &leIconType, &luEventTypeIndex, lv3Position);

            CGS_ASSERT(luEventTypeIndex < KU_ICON_EVENT_TYPE_COUNT,
                       "Event type index is too big.");   // console line 1209

            if (IsIgnoredIcon(luEventId))
                continue;

            if (luEventId == mHoveredEventIcon.muHoveredEventID)
            {
                miSelectedIndex = static_cast<s32>(luIndex);   // deferred to the top
            }
            else
            {
                RenderEventIcon(lpRenderBuffer, lbUseMiniIcon, lbFiltered, leIconType,
                                luEventId, luEventTypeIndex,
                                lv3Position,
                                lafMiniHalfWidth, lafMiniHalfHeight,
                                lafHalfWidth, lafHalfHeight,
                                luIconColour, la2DIcons, &li2DIconCount);
            }
        }
    }

    // ---- the map furniture, MAINMAP view only ------------------------------------------
    if (mRenderMainMapEvent.meMapType == GuiEventRenderMainMap::E_MAPTYPE_MAINMAP)
    {
        RenderDriveThroughs(lpRenderBuffer, lfIconScale);
        RenderRoadSigns(lpRenderBuffer);
        RenderCursor(lpRenderBuffer);
    }

    // ---- the one deferred hovered thing, redrawn on top --------------------------------
    if (miSelectedIndex != -1)
    {
        if (mHoveredEventIcon.mHoveredDriveThroughID != 0)
        {
            RenderDriveThrough(lpRenderBuffer, miSelectedIndex, lfIconScale);
        }
        else if (mHoveredEventIcon.muHoveredEventID != 0)
        {
            bool              lbFiltered      = false;
            bool              lbUseMiniIcon   = false;
            u32               luEventId       = 0;
            ECrashNavIconType leIconType      = E_CRASHNAVICON_EVENT_NOTATTEMPTED;
            u32               luEventTypeIndex = 0;
            Vector3           lv3Position;

            GetIconInformation(static_cast<u32>(miSelectedIndex), &lbFiltered, &lbUseMiniIcon,
                               &luEventId, &leIconType, &luEventTypeIndex, lv3Position);

            CGS_ASSERT(luEventTypeIndex < KU_ICON_EVENT_TYPE_COUNT,
                       "Event type index is too big.");   // console line 1256

            RenderEventIcon(lpRenderBuffer, lbUseMiniIcon, lbFiltered, leIconType,
                            luEventId, luEventTypeIndex,
                            lv3Position,
                            lafMiniHalfWidth, lafMiniHalfHeight,
                            lafHalfWidth, lafHalfHeight,
                            luIconColour, la2DIcons, &li2DIconCount);
        }
        else if (mHoveredEventIcon.mpcHoveredRoadName != 0 &&
                 mRoadSignIconStatus.mpRoadSignIcons != 0)
        {
            RenderRoadSign(lpRenderBuffer, miSelectedIndex);
        }
    }

    RenderStartFinish(lpRenderBuffer);
    RenderRivals(lpRenderBuffer);

    // ---- publish this frame's 2D icon run ----------------------------------------------
    // X360: `lwz r11, 0x4060(cache)` (mpMapIconManager) then `+ 0xA1B4` (the embedded
    // EventIconManager) -- reached here through the named accessor.
    mpGuiCache->GetMapIconManager()->GetEventIconManager().Update2DIcons(la2DIcons, li2DIconCount);

    // ---- roll the per-frame state ------------------------------------------------------
    // The published banks are consumed: they only live for the frame that published them.
    mGuiEventMapIconStatus.liNumberOfIcons = 0;
    mGuiEventMapIconStatus.lpSatNavIcons   = 0;
    mRoadSignIconStatus.mpRoadSignIcons    = 0;

    // ...and the hover record rolls into the last-frame slot, which is what every
    // "did the hover CHANGE this frame" test above keys off.
    mHoveredEventIconLastFrame = mHoveredEventIcon;
    mHoveredEventIcon.mHoveredDriveThroughID = 0;
    mHoveredEventIcon.mHoveredPlayerID       = 0;
    mHoveredEventIcon.muHoveredEventID       = 0;
    mHoveredEventIcon.mpcHoveredRoadName     = 0;
}

// ---------------------------------------------------------------------------
// @0x8246AE38 -- RenderComponent. The per-frame entry the GUI CustomRendererManager calls:
// open the buffer, install the cull-none rasteriser and the shared 2D screen transform,
// push the map's own mask over the view rect, draw everything, pop, close.
//
// The mask texture is picked by the published map type:
//   MAINMAP(0)                  -> mpBackgroundMaskTextureState (texture id 203)
//   PRERACE(1), LOADING_MAP(2)  -> mpPreRaceMaskTextureState    (texture id 202)
//   anything >= 3               -> mpBackgroundMaskTextureState
// (The console reaches the last two through the `v8 = v7 < 3` predicate it also feeds to
// the vsldoi that builds the {0,0,1,1} mask-UV vector -- the vector is the same on every
// arm, so only the texture pick survives here.)
// ---------------------------------------------------------------------------
void CrashNavIconRenderer::RenderComponent(CgsGui::ImRendererSet* lpRendererSet)
{
    CGS_ASSERT(mePrepareStage == E_PREPARESTAGE_DONE,
               "mePrepareStage == E_PREPARESTAGE_DONE");   // console line 552
    CGS_ASSERT(lpRendererSet != 0, "lpImRenderers");       // console line 555

    Im2dCommandBuffer* lpRenderBuffer = ResolveCrashNavBuffer(lpRendererSet);
    CGS_ASSERT(lpRenderBuffer != 0, "lpImRenderers->mpIm2dRenderBuffer");   // console line 556

    // [FLAG PC bring-up] the console derefs straight through these asserts. On this build
    // the component can be pumped before its stage machine has finished (the crash-nav
    // state's resource pump is still being wired -- see s1 §1.5), so the three console
    // preconditions get a real early-out here rather than an AV.
    if (lpRenderBuffer == 0 || mePrepareStage != E_PREPARESTAGE_DONE)
        return;

    // [FLAG PC bring-up] THE NOT-YET-LATCHED WINDOW. The map screen can render one frame
    // before any GuiEventRenderMainMap (id 223) has reached RecvEvent, so mRenderMainMapEvent
    // is still the all-zero ctor record -- and RenderIcons' console assert
    // `mRenderMainMapEvent.mfZoomLevel != 0.0f` (line 1122) fires, timing-dependently, roughly
    // one boot in a few. UNREACHABLE ON CONSOLE: the map state publishes 223 to every seated
    // renderer before the screen's first render pump, so an all-zero record is not a state the
    // console's ordering can produce; on this build the seating/pump order is still being wired
    // (s1 §1.5), which is what opens the window. MainMapRenderer::RenderComponent already rides
    // it out with its four-way guard (mpActiveTextures / mpGuiCache / the two mask states, all
    // of which stay null-or-unset until the same event + InitResources have run), so slot 3
    // takes the IDENTICAL early-out -- the two halves of the map screen must appear on the same
    // frame or neither, and matching the guards is what guarantees that.
    // [DIAG] NOT IN THE X360 BINARY -- [cnav-diag] the gate report: which of the early-outs
    // above / below is holding the crash-nav furniture off the screen. BRN_SATNAV_DIAG only,
    // every 120th call.
    {
        static const bool sbDiag = (getenv("BRN_SATNAV_DIAG") != 0);
        static s32 siTick = 0;
        if (sbDiag && CgsDev::Log::gpDebugPrint != 0 && (siTick++ % 120) == 0)
            *CgsDev::Log::gpDebugPrint
                << "[cnav-diag] RenderComponent: buffer=" << (lpRenderBuffer != 0)
                << " stage=" << static_cast<s32>(mePrepareStage)
                << " textures=" << (mRenderMainMapEvent.mpActiveTextures != 0)
                << " cache=" << (mpGuiCache != 0)
                << " masks=" << (mpBackgroundMaskTextureState != 0) << "/" << (mpPreRaceMaskTextureState != 0)
                << " maptype=" << static_cast<s32>(mRenderMainMapEvent.meMapType)
                << " zoom=" << mRenderMainMapEvent.mfZoomLevel
                << " active=" << (mRenderMainMapEvent.mbIsActive ? 1 : 0)
                << " drawEvents=" << (mbRenderEventStarts ? 1 : 0)
                << " displayType=" << static_cast<s32>(meIconDisplayType)
                << " numIcons=" << (mpGuiCache != 0 && meIconDisplayType < GuiEventDrawEventIcons::E_ICON_DISPLAY_TYPE_COUNT ? static_cast<s32>(GetNumIcons()) : -1)
                << " bank=" << mGuiEventMapIconStatus.liNumberOfIcons << "/" << (mGuiEventMapIconStatus.lpSatNavIcons != 0)
                << " cursor=" << mGuiEventMapCursorStatus.miDisplayState << "/" << mGuiEventMapCursorStatus.miAnimationState
                << "\n";
    }

    if (mRenderMainMapEvent.mpActiveTextures == 0 ||
        mpGuiCache                           == 0 ||
        mpBackgroundMaskTextureState         == 0 ||
        mpPreRaceMaskTextureState            == 0)
    {
        return;
    }

    lpRenderBuffer->BeginRendering();                                   // X360 sub_824587B0
    lpRenderBuffer->SetState(CgsGui::gpGuiRasterizerStateCullNone);     // X360 dword_83010F3C
    lpRenderBuffer->SetTransform(CgsGui::gBillboardScreenTransform);    // X360 unk_83011090

    const renderengine::TextureState* lpMaskTextureState;
    if (mRenderMainMapEvent.meMapType == GuiEventRenderMainMap::E_MAPTYPE_MAINMAP)
        lpMaskTextureState = mpBackgroundMaskTextureState;
    else if (mRenderMainMapEvent.meMapType < GuiEventRenderMainMap::E_MAPTYPE_COUNT)
        lpMaskTextureState = mpPreRaceMaskTextureState;
    else
        lpMaskTextureState = mpBackgroundMaskTextureState;

    // The mask covers the whole published view rect at the mask texture's full UV range
    // (the `vspltisw 1 / vcfsx / vperm / vsldoi` block builds exactly {0,0,1,1}).
    SetMaskRect(*lpRenderBuffer, lpMaskTextureState,
                mRenderMainMapEvent.mv4ViewRect, MakeV4(0.0f, 0.0f, 1.0f, 1.0f));

    RenderIcons(lpRenderBuffer);

    lpRenderBuffer->PopMask();
    lpRenderBuffer->EndRendering();                                     // X360 sub_82458898
}

// ============================================================================
// @0x82463E78 -- RenderRoadSign. One authored road sign: its pole quad, its coloured plate
// quad, and its two text lines, all anchored on the live RoadSignIcon's world position.
//
// ⭐ UNBLOCKED 2026-08-29 (FIX2). The previous pass parked this as "blocked on data": the
// 65-entry road-id string table, the eighteen 4-float guarded rect statics and their
// per-(size, colour) selection, and two loose text constants. Every one of them is in the
// raw image or in the asm; see KAPC_ROAD_IDS / KAV4_SIGN_PLATE_UV / KAV2_SIGN_HALF_EXTENTS /
// KAV4_SIGN_TEXT_COLOUR below, and the selection note on KAV4_SIGN_PLATE_UV. The claim that
// "the selection is a jump table the export's pseudocode folds away" was true of the
// PSEUDOCODE only -- the asm carries all sixteen arms explicitly (jpt_82464D20 and its three
// siblings), which is what made them recoverable.
//
// SHAPE (asm @0x82463E78-0x82465200):
//   * assert the text renderer, then find this sign's road id in the 65-entry table. NO
//     MATCH == RETURN BEFORE DRAWING -- the console's own `if (v36 == off_82F27D98) goto
//     end`. The matched INDEX is the RoadSignIcon slot (`192 * v35`), which is why the table
//     order is load-bearing and could not be invented.
//   * roll the shared hover latch on the hovered ROAD NAME (the same
//     mfHoveredIconScaleFactor / ...EndTime / ...Growing triple RenderCursor, RenderRivals
//     and RenderDriveThrough drive, keyed here on a string compare).
//   * WorldToDevice the icon's cached world lane, then lay both quads out around that device
//     point in the published mfScaleFactor.
//   * the two text lines go through mTextObject under the renderer's own mTextTransform,
//     and the billboard screen transform is restored afterwards.
//
// The console copy-assigns the whole 104-byte record onto the stack (RoadSign::operator=
// @0x8244BBB8) purely to reach its fields; on the host it is read in place by name.
// ---------------------------------------------------------------------------
void CrashNavIconRenderer::RenderRoadSign(Im2dCommandBuffer* lpRenderBuffer, s32 liIndex)
{
    CGS_ASSERT(mpTextRenderer != 0, "Text renderer not set!");   // console line 1884
    if (mpTextRenderer == 0 || mRoadSignIconStatus.mpRoadSignIcons == 0)
        return;   // [FLAG PC bring-up] the console derefs straight through both

    const RoadSign& lrSign = mRoadSignList.maRoadSigns[liIndex];

    // ⭐ THE PREMISE OF THIS GUARD IS GONE (2026-08-29, wave G3): BrnGui::RoadSignList::Construct
    // @0x8244BC90 -- the 65-record authored literal table -- IS now reconstructed, in
    // CustomRenderer/Renderers/BrnRoadSign.cpp, and the renderer's ctor already calls it
    // (BrnCrashNavIconRenderer.cpp:288). So mpcRoadId / mapcText are no longer ctor garbage
    // and the string walk below has real pointers to walk.
    // ⚠️ THE GUARD ITSELF STAYS until this is boot-verified -- belt and braces, deliberately.
    // It costs three loads on a 65-iteration loop and it is the difference between a wild
    // read and a skipped sign if the ctor's Construct call is ever reordered away. There is
    // no console equivalent (the console's table is a compiled-in initialiser that cannot be
    // unset), so it is still a PC bring-up guard, not a reconstruction.
    // DELETE-WHEN a boot run has drawn road signs.
    if (lrSign.mpcRoadId == 0 || lrSign.mapcText[0] == 0 || lrSign.mapcText[1] == 0)
        return;

    // ⚠️ AUTHORED-DATA NOTE, so a tester does not read this as a defect: exactly ONE of the
    // 65 records ("392373" / "6th ST", index 26) carries a road id that is NOT among the 64
    // live ids in KAPC_ROAD_IDS. The scan below therefore falls through to the "invisible"
    // sentinel, finds no match, and this function returns before drawing -- which is the
    // console's own behaviour (`if (v36 == off_82F27D98) goto end`). One authored sign in the
    // city has no icon slot. Do not "repair" the table.

    // ---- which RoadSignIcon slot is this sign's? -------------------------------------
    // Linear scan of the authored id table; the slot IS the table position. The console
    // open-codes the strcmp (`v39 = *v38 - *v37`), reproduced.
    s32 liIconSlot = -1;
    for (s32 liRoad = 0; liRoad < KI_NUM_ROAD_IDS; ++liRoad)
    {
        const char* lpcTable = KAPC_ROAD_IDS[liRoad];
        const char* lpcSign  = lrSign.mpcRoadId;
        s32 liDelta = 0;
        for (;;)
        {
            liDelta = static_cast<s32>(*lpcSign) - static_cast<s32>(*lpcTable);
            if (*lpcSign == 0 || liDelta != 0)
                break;
            ++lpcSign;
            ++lpcTable;
        }
        if (liDelta == 0)
        {
            liIconSlot = liRoad;
            break;
        }
    }
    if (liIconSlot < 0)
        return;   // the console's "walked past off_82F27D98" bail -- nothing is drawn

    // ---- the shared hover latch, keyed on the hovered ROAD NAME -----------------------
    // A hover that CHANGED this frame restarts the grow leg; an unchanged hover whose grow
    // window has expired settles back to 1.0. (asm @0x82464918-... via LABEL_72 / LABEL_75.)
    {
        const char* lpcLastFrame = mHoveredEventIconLastFrame.mpcHoveredRoadName;
        const char* lpcThisFrame = mHoveredEventIcon.mpcHoveredRoadName;

        bool lbHoverChanged;
        if (lpcLastFrame == 0 && lpcThisFrame != 0)
        {
            lbHoverChanged = true;
        }
        else if (lpcThisFrame == 0)
        {
            lbHoverChanged = false;
        }
        else
        {
            s32 liDelta = 0;
            for (;;)
            {
                liDelta = static_cast<s32>(*lpcLastFrame) - static_cast<s32>(*lpcThisFrame);
                if (*lpcLastFrame == 0 || liDelta != 0)
                    break;
                ++lpcLastFrame;
                ++lpcThisFrame;
            }
            lbHoverChanged = (liDelta != 0);
            lpcLastFrame = mHoveredEventIconLastFrame.mpcHoveredRoadName;   // walked above
        }

        const f32 lfNow = mpGuiCache->GetTime();   // carries the :250 -FLT_MAX assert
        if (lbHoverChanged)
        {
            mfHoveredIconScaleFactor  = 1.0f;
            mfHoveredIconGrowing      = true;
            mfHoveredIconScaleEndTime = lfNow + KF_SELECTED_GROW_TIME;
        }
        else if (mfHoveredIconScaleEndTime < lfNow && lpcLastFrame != 0)
        {
            mfHoveredIconScaleFactor = 1.0f;
            mfHoveredIconGrowing     = false;
        }
    }

    // ---- the anchor: this sign's icon in device space ---------------------------------
    const RoadSignIcon& lrIcon = mRoadSignIconStatus.mpRoadSignIcons[liIconSlot];
    const f32 lfScale = mRoadSignIconStatus.mfScaleFactor;

    // The console rebuilds the icon's cached +0x90 lane as a world triple with a zeroed third
    // lane before the transform (`*&v134 = lane; v136 = 0.0`), then WorldToDevice(v, false).
    Vector3 lv3World;
    lv3World.x = lrIcon.mv4WorldPosition.x;
    lv3World.y = lrIcon.mv4WorldPosition.y;
    lv3World.z = 0.0f;
    lv3World.w = 0.0f;
    const Vector2 lv2Device = MapTransform::WorldToDevice(lv3World, false);

    // ---- the POLE quad -----------------------------------------------------------------
    {
        const f32 lfLeft   = lv2Device.x + lfScale * (lrSign.mvPoleOffset.x - KF_SIGN_POLE_HALFWIDTH);
        const f32 lfRight  = lv2Device.x + lfScale * (lrSign.mvPoleOffset.x + KF_SIGN_POLE_HALFWIDTH);
        const f32 lfTop    = lv2Device.y + lfScale * (lrSign.mvPoleOffset.y - KF_SIGN_POLE_HALFHEIGHT);
        const f32 lfBottom = lv2Device.y + lfScale * (lrSign.mvPoleOffset.y + KF_SIGN_POLE_HALFHEIGHT);

        RenderQuad(lpRenderBuffer,
                   MakeV2(lfLeft  * KF_DEVICE_TO_NORMALISED_X, lfTop    * KF_DEVICE_TO_NORMALISED_Y),
                   MakeV2(lfLeft  * KF_DEVICE_TO_NORMALISED_X, lfBottom * KF_DEVICE_TO_NORMALISED_Y),
                   MakeV2(lfRight * KF_DEVICE_TO_NORMALISED_X, lfTop    * KF_DEVICE_TO_NORMALISED_Y),
                   MakeV2(lfRight * KF_DEVICE_TO_NORMALISED_X, lfBottom * KF_DEVICE_TO_NORMALISED_Y),
                   OpaqueWhite(),                    // vmr128 v1, v126 (the 1.0 splat)
                   mpRoadSignsTextureState,          // lwz r9, 0x128(r31)
                   CgsGui::gpGuiBlendStateStandard,  // dword_83010F20
                   KV4_SIGN_POLE_UV);
    }

    // ---- the PLATE quad: size picks the extents, colour picks the atlas rect ------------
    const s32 liSignSize = static_cast<s32>(lrSign.meSignSize);
    CGS_ASSERT(liSignSize >= 0 && liSignSize < RoadSign::E_SIGNSIZE_COUNT,
               "Unknown sign size");                                  // console line 2197
    const s32 liSignColour = static_cast<s32>(lrIcon.meSignColour);
    CGS_ASSERT(liSignColour >= 0 &&
               liSignColour < static_cast<s32>(RoadSignIcon::KU_NUM_SIGN_COLOURS),
               "Unknown sign colour");                                // console lines 2050/2096/2142/2188
    if (liSignSize < 0 || liSignSize >= RoadSign::E_SIGNSIZE_COUNT ||
        liSignColour < 0 ||
        liSignColour >= static_cast<s32>(RoadSignIcon::KU_NUM_SIGN_COLOURS))
    {
        return;   // [FLAG PC bring-up] the console's asserts are fatal; here they early-out
    }

    const CgsGraphics::Vector2& lrv2HalfExtents = KAV2_SIGN_HALF_EXTENTS[liSignSize];
    const Vector4& lrv4PlateUv = KAV4_SIGN_PLATE_UV[liSignSize][liSignColour];
    const Vector4& lrv4SignColour = KAV4_SIGN_TEXT_COLOUR[liSignColour];

    const f32 lfPlateLeft   = lv2Device.x + lfScale * (lrSign.mvSignOffset.x - lrv2HalfExtents.x);
    const f32 lfPlateRight  = lv2Device.x + lfScale * (lrSign.mvSignOffset.x + lrv2HalfExtents.x);
    const f32 lfPlateTop    = lv2Device.y + lfScale * (lrSign.mvSignOffset.y - lrv2HalfExtents.y);
    const f32 lfPlateBottom = lv2Device.y + lfScale * (lrSign.mvSignOffset.y + lrv2HalfExtents.y);

    RenderQuad(lpRenderBuffer,
               MakeV2(lfPlateLeft  * KF_DEVICE_TO_NORMALISED_X, lfPlateTop    * KF_DEVICE_TO_NORMALISED_Y),
               MakeV2(lfPlateLeft  * KF_DEVICE_TO_NORMALISED_X, lfPlateBottom * KF_DEVICE_TO_NORMALISED_Y),
               MakeV2(lfPlateRight * KF_DEVICE_TO_NORMALISED_X, lfPlateTop    * KF_DEVICE_TO_NORMALISED_Y),
               MakeV2(lfPlateRight * KF_DEVICE_TO_NORMALISED_X, lfPlateBottom * KF_DEVICE_TO_NORMALISED_Y),
               OpaqueWhite(),                    // vmr128 v1, v126 again -- the PLATE is white;
               mpRoadSignsTextureState,          // the sign colour drives the TEXT, not this quad
               CgsGui::gpGuiBlendStateStandard,
               lrv4PlateUv);

    // ---- the two text lines -------------------------------------------------------------
    // Both go through the ONE shared mTextObject under the renderer's own mTextTransform.
    // The console's clamp-then-scale of the sign colour (`vmaxfp v13,v127,0 / vminfp v127,
    // v13, 1.0 / vmulfp v0, v127, unk_8305A950`) is exactly PackVertexColour's fold, and
    // unk_8305A950 is the same all-255 vector that helper already documents.
    const CgsGraphics::RGBA8 lTextRgba = PackVertexColour(lrv4SignColour);
    const CgsGraphics::RGBA   lTextColour =
        (static_cast<u32>(lTextRgba.r) << 24) | (static_cast<u32>(lTextRgba.g) << 16) |
        (static_cast<u32>(lTextRgba.b) << 8)  |  static_cast<u32>(lTextRgba.a);

    const f32 lfTextAnchorX = lv2Device.x + lfScale * lrSign.mvTextOffset.x;
    const f32 lfTextAnchorY = lv2Device.y + lfScale * lrSign.mvTextOffset.y;

    // Line 0 -- RIGHT-aligned inside the authored text box, which is offset by the record's
    // first individual-text offset and sized by mavTextBoxBounds[0].
    if (mTextObject.mbAutosize)
        mTextObject.CalculateAutosizing();

    mTextObject.mfFontHeight = (lrSign.mafFontSize[0] * KF_SIGN_FONT_SIZE_SCALE) * lfScale;
    mTextObject.mTextColour  = lTextColour;
    mTextObject.mv2TopLeft.mX =
        lfTextAnchorX + lfScale * lrSign.mavIndividualTextOffsets[0].x +
        lfScale * lrSign.mavTextBoxBounds[0][0];
    mTextObject.mv2TopLeft.mY =
        lfTextAnchorY + lfScale * lrSign.mavIndividualTextOffsets[0].y +
        lfScale * lrSign.mavTextBoxBounds[0][1];
    mTextObject.mv2BottomRight.mX =
        lfTextAnchorX + lfScale * lrSign.mavIndividualTextOffsets[0].x +
        lfScale * lrSign.mavTextBoxBounds[0][2];
    mTextObject.mv2BottomRight.mY =
        lfTextAnchorY + lfScale * lrSign.mavIndividualTextOffsets[0].y +
        lfScale * lrSign.mavTextBoxBounds[0][3];
    mTextObject.mfCharSpacingMultiplier = KF_SIGN_CHAR_SPACING;
    mTextObject.meAlignment  = CgsGraphics::TextObject::E_ALIGNMENT_RIGHT;   // li r11, 3
    mTextObject.mpUtf8String =
        reinterpret_cast<const CgsResource::CgsUtf8*>(lrSign.mapcText[0]);
    mTextObject.mfStringWidth = mTextObject.mpFont->GetStringWidth(mTextObject.mpUtf8String);
    if (mTextObject.mbAutosize)
        mTextObject.CalculateAutosizing();

    lpRenderBuffer->SetTransform(mTextTransform);   // SetTransform(a14, this + 16)
    mpTextRenderer->RenderStringBuffered(lpRenderBuffer, mTextObject);

    // Line 1 -- LEFT-aligned at a bare anchor point. The console writes the SAME point into
    // both corners (v121 == v123, v122 == v124), i.e. a degenerate box; reproduced.
    if (mTextObject.mbAutosize)
        mTextObject.CalculateAutosizing();

    mTextObject.mfFontHeight = (lrSign.mafFontSize[1] * KF_SIGN_FONT_SIZE_SCALE) * lfScale;
    mTextObject.mTextColour  = lTextColour;
    mTextObject.mv2TopLeft.mX =
        lfTextAnchorX + lfScale * lrSign.mavIndividualTextOffsets[1].x;
    mTextObject.mv2TopLeft.mY =
        lfTextAnchorY + lfScale * lrSign.mavIndividualTextOffsets[1].y;
    mTextObject.mv2BottomRight.mX = mTextObject.mv2TopLeft.mX;
    mTextObject.mv2BottomRight.mY = mTextObject.mv2TopLeft.mY;
    mTextObject.mfCharSpacingMultiplier = KF_SIGN_CHAR_SPACING;
    mTextObject.meAlignment  = CgsGraphics::TextObject::E_ALIGNMENT_LEFT;    // li r11, 1
    mTextObject.mpUtf8String =
        reinterpret_cast<const CgsResource::CgsUtf8*>(lrSign.mapcText[1]);
    mTextObject.mfStringWidth = mTextObject.mpFont->GetStringWidth(mTextObject.mpUtf8String);
    if (mTextObject.mbAutosize)
        mTextObject.CalculateAutosizing();

    mpTextRenderer->RenderStringBuffered(lpRenderBuffer, mTextObject);

    // Put the shared 2D screen transform back for everything that draws after us.
    lpRenderBuffer->SetTransform(CgsGui::gBillboardScreenTransform);   // unk_83011090
}

} // namespace BrnGui

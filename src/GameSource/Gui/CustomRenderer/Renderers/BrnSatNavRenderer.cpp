// BrnSatNavRenderer.cpp -- BrnGui::SatNavRenderer, the GUI custom-render component that
// draws the sat-nav / mini-map event icons and the player route highlight.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (Jan-2008). All 13 ledger functions of this
// TU are here. See BrnSatNavRenderer.h for the layout policy.
//
// SOURCE-OF-TRUTH: X360 ARTIST pseudocode + asm is authoritative for behaviour; the DWARF
// (references/DecFIGS/.../BrnSatNavRenderer.h) gives the declaration shapes/names. No
// Feb-2007 source exists for this TU.
//
// SEMANTIC-LEVEL SIMD: GetIconInformation and RenderIconsForSatNav are hand-vectorised on
// X360 (VMX128 lvx128/stvx128/vmaddfp/vrefp/vperm/vmsum3fp128). Those intrinsics have no
// portable PC equivalent, so the vector copies/transforms are reconstructed at the
// SEMANTIC level (faithful named-member math producing the same result), consistent with
// the policy in BrnMapUtils.h and rw/math/vpu/types.h. The control flow, side effects,
// asserts, early-outs and store targets are reproduced faithfully.
//
// ASSERTS: the X360 builds dynamic assert text through CgsDev::Assert::gpcMessageBuffer +
// StrStream; per the project convention these are lowered to the static-message
// Begin/Fire/End sequence with the recovered literal expression + the original file/line.

#include "GameSource/Gui/CustomRenderer/Renderers/BrnSatNavRenderer.h"

#include "GameSource/Gui/BrnGuiCache.h"                 // BrnGui::GuiCache, BrnGui::PresetEvent
#include "GameSource/Gui/BrnGuiWorldDataController.h"   // BrnGui::WorldDataController
#include "GameSource/GameState/Progression/BrnProfile.h"// BrnProgression::ProfileEvent
#include "SharedClasses/Progression/BrnRaceEventData.h" // BrnProgression::RaceEventData
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"       // BrnGui::MapTransform

#include "GameShared/GameClasses/Core/CgsAssert.h"      // BeginAssert/FireAssert/EndAssert
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderBuffer.h" // Im2dRenderBuffer (SetState/Render)
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasic2dColouredTexturedVertex.h" // vertex

#include <cstring> // memcpy

namespace BrnGui
{
namespace
{
// ---------------------------------------------------------------------------
// File-local data tables (X360 .rdata). The exact float contents are NOT
// recoverable from the symbol export (the build's data segment is not dumped),
// so the DWARF-named tables are declared here with the recovered SHAPE and the
// UV/clamp algorithm that consumes them is reconstructed faithfully. FLAG: the
// numeric table values below are inferred placeholders for the unrecovered
// .rdata constants -- the GENERATED UVs depend on them, so a reviewer should
// treat the produced UV numbers (not the algorithm) as unverified.
// ---------------------------------------------------------------------------

// Per-texture-resolution atlas dimensions (index 0 = SD, 1 = HD). DWARF h:213-216.
const f32 KAF_TEXTURE_WIDTH[2]  = { 1024.0f, 1024.0f }; // flt_82054E10
const f32 KAF_TEXTURE_HEIGHT[2] = { 1024.0f, 1024.0f }; // flt_82054E18
const f32 KAF_ICON_WIDTH[2]     = {   64.0f,   64.0f }; // flt_82054E20
const f32 KAF_ICON_HEIGHT[2]    = {   64.0f,   64.0f }; // flt_82054E28
const f32 KAF_MINI_ICON_WIDTH[2]  = { 32.0f, 32.0f };   // flt_82054E38 (DWARF h:229)
const f32 KAF_MINI_ICON_HEIGHT[2] = { 32.0f, 32.0f };   // flt_82054E40 (DWARF h:230)

// Mini-icon atlas V offset added to the second texture row. flt_82054E64. The asm adds the
// literal 0.875 to each generated mini-icon V; DWARF names it KF_MINI_ICON_TEXTURE_OFFSET (h:236).
const f32 KF_MINI_ICON_TEXTURE_OFFSET = 0.875f;

// RenderIconsForSatNav clamp band. flt_82054E68 == 0.025; its complement 0.975 (flt_82057F5C) is
// the upper edge. The visible-range gate compares against 0.0 (flt_82001CC0) / 1.0 (flt_82001C98).
const f32 KF_ICON_CLAMP_MIN = 0.025f;        // flt_82054E68
const f32 KF_ICON_CLAMP_MAX = 0.97500002f;   // flt_82057F5C
const f32 KF_VISIBLE_MIN    = 0.0f;          // flt_82001CC0
const f32 KF_VISIBLE_MAX    = 1.0f;          // flt_82001C98

// Per-(icon-type) icon half-extents, indexed by IconRendererSatNavIconInfo::meSatNavIconType
// (E_SATNAVICON_NUM == 2 entries each). RenderIconsForSatNav multiplies each by (1 / mfZoomLevel)
// once per frame (@0x8245FA9C-FAE4) to get the zoom-scaled half-width/half-height it passes to
// RenderSatNavIcon. The display-type-0 (profile) path uses the WORLDSIZE pair
// (KAF_ICON_HALFWIDTH_WORLDSIZE = flt_82FB36F0, KAF_ICON_HALFHEIGHT_WORLDSIZE = flt_82FB3728);
// the online paths use the SCREENSPACE pair (KAF_ICON_HALFWIDTH_SCREENSPACE = flt_82FB3708,
// KAF_ICON_HALFHEIGHT_SCREENSPACE = flt_82FB3720). FLAG: the .rdata float contents are NOT in the
// symbol export (data segment not dumped); modelled as placeholders -- the produced half-extents
// (not the indexing/zoom algorithm, which is X360-proven) are unverified.
const f32 KAF_ICON_HALFWIDTH_WORLDSIZE[SatNavRenderer::E_SATNAVICON_NUM]   = { 0.0f, 0.0f }; // flt_82FB36F0
const f32 KAF_ICON_HALFWIDTH_SCREENSPACE[SatNavRenderer::E_SATNAVICON_NUM] = { 0.0f, 0.0f }; // flt_82FB3708
const f32 KAF_ICON_HALFHEIGHT_SCREENSPACE[SatNavRenderer::E_SATNAVICON_NUM]= { 0.0f, 0.0f }; // flt_82FB3720
const f32 KAF_ICON_HALFHEIGHT_WORLDSIZE[SatNavRenderer::E_SATNAVICON_NUM]  = { 0.0f, 0.0f }; // flt_82FB3728

// Event-type-byte -> sat-nav-icon UV row map (dword_82054E48), indexed by
// RaceEventData::GetEventTypeByte(). Produces the icon's "row" value (0/1/2); a value of 2 is
// special-cased against the active event (the row-2 -> 0 override in GetIconInformation /
// RefreshSatNavIconInfo). FLAG: contents unrecovered -- 0x82054E48 is not in the symbol export
// (data segment not dumped). Modelled as all-zero, sized for a byte index; with the map all-zero
// the row-2 special cases never trigger, so the dependent override stays dormant (data-fidelity
// gap, reported in still_open). The CONSUMING logic is X360-faithful regardless of the contents.
const u32 KAU_EVENTTYPE_TO_ICONROW[256] = { 0 };

// maResourcesToLoad / muNumResourcesToLoad (DWARF h:147/154). The InitResources loop walks a
// 2-entry resource-tuple table (id + flags pairs, 8-byte stride) up to muNumResourcesToLoad.
// FLAG: tuple ids unrecovered (data segment not dumped); shape (2 entries) is X360-proven by
// the loop bound `v2 < &dword_82F25718`.
struct SatNavResourceTuple { u32 muResourceId; u32 muFlags; };
const SatNavResourceTuple KA_RESOURCES_TO_LOAD[2] = { { 0, 0 }, { 0, 0 } };
const u32 KU_NUM_RESOURCES_TO_LOAD = 2; // == E_SATNAVICON_NUM

const char* const KPC_FILE =
    "..\\..\\..\\GameSource\\Gui/CustomRenderer/Renderers/BrnSatNavRenderer.cpp";

// The X360 builds these asserts through StrStream/gpcMessageBuffer; lowered to the static
// Begin/Fire/End sequence at the recovered file/line.
void FireSatNavAssert(const char* lpcExpression, s32 liLine)
{
    CgsDev::Assert::BeginAssert();
    CgsDev::Assert::FireAssert(lpcExpression, KPC_FILE, liLine);
    CgsDev::Assert::EndAssert();
}

// ---------------------------------------------------------------------------
// Un-inlined helper: generate the four UV corners for one atlas frame.
// X360 inlines BrnGui::SatNavRenderer::CalculateUVsForIndex (DWARF cpp:983) into
// InitEventTypeUvs; restored here as a free helper. Computes the sprite-sheet UVs for
// frame `liFrameNumber` (1-based) in an atlas of (lfTextureWidth x lfTextureHeight) made of
// (lfIconWidth x lfIconHeight) cells.
// ---------------------------------------------------------------------------
void CalculateUVsForIndex(s32 liFrameNumber,
                          Vector2& lv2TopLeft, Vector2& lv2BottomLeft,
                          Vector2& lv2TopRight, Vector2& lv2BottomRight,
                          f32 lfTextureWidth, f32 lfTextureHeight,
                          f32 lfIconWidth, f32 lfIconHeight)
{
    const f32 lfInvTextureWidth  = 1.0f / lfTextureWidth;
    const f32 lfInvTextureHeight = 1.0f / lfTextureHeight;
    const f32 lfUWidth  = lfIconWidth  * lfInvTextureWidth;   // cell width in UV
    const f32 lfVHeight = lfIconHeight * lfInvTextureHeight;  // cell height in UV

    const s32 liFramesPerRow = static_cast<s32>(lfTextureWidth / lfIconWidth);
    const s32 liColumn = liFrameNumber / liFramesPerRow;
    const s32 liRow    = liFrameNumber % liFramesPerRow;

    const f32 lfU0 = static_cast<f32>(liRow)    * lfIconWidth  * lfInvTextureWidth;
    const f32 lfV0 = static_cast<f32>(liColumn) * lfIconHeight * lfInvTextureHeight;

    lv2TopLeft.x     = lfU0;            lv2TopLeft.y     = lfV0;
    lv2BottomLeft.x  = lfU0;            lv2BottomLeft.y  = lfV0 + lfVHeight;
    lv2TopRight.x    = lfU0 + lfUWidth; lv2TopRight.y    = lfV0;
    lv2BottomRight.x = lfU0 + lfUWidth; lv2BottomRight.y = lfV0 + lfVHeight;
}

} // anonymous namespace

// ===========================================================================
// SatNavRenderer
// ===========================================================================

// 0x827DD468 -- the embedded-subobject constructor the manager aggregate ctor runs
// (CustomRendererManager::CustomRendererManager). The X360 ctor sets the vptr (implicit in
// C++) and zero-initialises the pointer / resource-descriptor member range (+0x98..+0x120
// plus the 2-entry maIconResources/state slots at +0x128, stride 0x14), then `blr` -- it does
// NOT call Construct(); the manager runs the virtual Construct() as a separate pass afterwards.
SatNavRenderer::SatNavRenderer()
{
    // Member range the X360 ctor zeroes before the manager's Construct pass fills it.
    mpGuiCache                 = 0;   // +0x98
    mpMapTextureState          = 0;   // +0xAC
    mpMapBlendState            = 0;   // +0xC4 region
    mpMaskTextureState         = 0;
    mpMaskBlendState           = 0;
    mpRouteSegmentTextureState = 0;
    mpRouteSegmentBlendState   = 0;
    mapIconTextureStates[0]    = 0;   // +0x128 stride-0x14 loop, 2 entries
    mapIconTextureStates[1]    = 0;
}

// 0x8245F6C8
void SatNavRenderer::Construct()
{
    CustomRenderComponentInterface::Construct();

    // ---- stage machines + cached pointers ----
    mePrepareStage = E_PREPARESTAGE_START;       // +0x50
    meReleaseStage = E_RELEASESTAGE_START;       // +0x54
    mpGuiCache     = 0;                           // +0x90
    mpMapTextureState          = 0;              // +0xAC
    mpMapBlendState            = 0;              // +0xDC
    mpMaskTextureState         = 0;              // +0xF4
    mpMaskBlendState           = 0;              // +0x10C
    mpRouteSegmentTextureState = 0;              // +0x124

    // The six texture/blend resource descriptors are primed to a {count=0, type=6} head (the
    // X360 stores the 0x600000000 doubleword: low dword 0, high dword 6). Cleared by name.
    u32* const lapResourceHeads[6] =
    {
        mMapTextureStateResource,          // +0x128
        mMapBlendStateResource,            // +0x130
        mMaskTextureStateResource,         // +0x138
        mMaskBlendStateResource,           // +0x140
        mRouteSegmentTextureStateResource, // +0x148
        mRouteSegmentBlendStateResource,   // +0x150
    };
    for (u32 lu = 0; lu < 6; ++lu)
    {
        lapResourceHeads[lu][0] = 0;
        lapResourceHeads[lu][1] = 6;
        lapResourceHeads[lu][2] = 0;
        lapResourceHeads[lu][3] = 0;
        lapResourceHeads[lu][4] = 0;
    }

    meIconDisplayType = GuiEventEnableSatNavIcons::E_ICON_DISPLAY_TYPE_COUNT; // +0x158 = 5
    meGameModeFilter  = 6;                        // +0x15C

    // The four icon-UV corner table heads are primed to a {1,0} doubleword (the X360 stores
    // the 0x100000000 head of each table); the InitEventTypeUvs pass overwrites the rest.
    mav2IconUvTopLeft[0][0].x     = 1.0f; mav2IconUvTopLeft[0][0].y     = 0.0f; // +0x160
    mav2IconUvBottomLeft[0][0].x  = 1.0f; mav2IconUvBottomLeft[0][0].y  = 0.0f;
    mav2IconUvTopRight[0][0].x    = 1.0f; mav2IconUvTopRight[0][0].y    = 0.0f;
    mav2IconUvBottomRight[0][0].x = 1.0f; mav2IconUvBottomRight[0][0].y = 0.0f;

    // ---- icon textures + flags (X360 +0x78..+0x89) ----
    maIconResources[0][0]   = 0;
    maIconResources[1][0]   = 0;
    mapIconTextureStates[0] = 0;                  // +0x84
    mapIconTextureStates[1] = 0;
    mbRenderEventStarts     = false;              // +0x88
    mMapQuadColour          = 0;                  // +0x78 group head

    mbDrawRoute           = false;                // +0x1878
    muNumberOfSatNavIcons = 0;                    // +0x1870
    mbRefreshSatNavIcons  = true;                 // +0x5A1 = 1 (refresh on construct)

    // Map-quad colour: white at alpha 0xE5 (0xE5FFFFFF), stored at +0x08.
    mMapQuadColour = 0xE5FFFFFFu;

    UpdateRendererTransform();

    miSatNavRendererPM = -1;                      // +0x187C (pre-seeded before AddMonitor)
    miSatNavRendererPM = CgsDev::PerfMonCpu::AddMonitor("SatNavRenderer", 3, 0, 2.0, 0, 0);
    if (miSatNavRendererPM < 0)
        FireSatNavAssert("miSatNavRendererPM >= 0", 226);
}

// 0x82445850
void SatNavRenderer::Destruct()
{
    // The X360 forwards to the base destructor, then resets the display type to the "no
    // display" sentinel.
    CustomRenderComponentInterface::Destruct();
    meIconDisplayType = GuiEventEnableSatNavIcons::E_ICON_DISPLAY_TYPE_COUNT; // +0x158 = 5
}

// 0x82445888 -- the component's CgsID (64-bit content hash). Constant-folded in the asm to
// 0xB6744D9D6FE80000.
u32 SatNavRenderer::GetComponentID() const
{
    // The X360 returns the full 64-bit CgsID 0xB6744D9D6FE80000 in r3. The base virtual is
    // declared returning a u32 component id; return the distinguishing low word the manager
    // keys on. FLAG: the base widens CgsID(64) -> u32 here.
    return 0x6FE80000u;
}

// 0x8245F828
bool SatNavRenderer::Prepare(void* lpResourceAllocator, void* lpA, void* lpB)
{
    // X360 signature: Prepare(GuiEventQueueSmall*, IResourceAllocator*, IResourceAllocator*).
    // Only the first allocator arg (latched on stage START) and the staged GuiCache load matter.
    (void)lpA;
    (void)lpB;

    switch (mePrepareStage)
    {
    case E_PREPARESTAGE_START:
        mpHeapAllocator = lpResourceAllocator;   // +0x94
        mePrepareStage  = E_PREPARESTAGE_LOAD;
        return false;

    case E_PREPARESTAGE_LOAD:
        if (mpGuiCache != 0 &&
            mpGuiCache->EnsureResourcesAreLoaded(
                reinterpret_cast<const CgsGui::sResourceTuple*>(KA_RESOURCES_TO_LOAD),
                KU_NUM_RESOURCES_TO_LOAD))
        {
            mePrepareStage = E_PREPARESTAGE_INIT;
        }
        return false;

    case E_PREPARESTAGE_INIT:
        InitResources();
        mePrepareStage = E_PREPARESTAGE_DONE;
        return false;

    case E_PREPARESTAGE_DONE:
        meIconDisplayType = GuiEventEnableSatNavIcons::E_ICON_DISPLAY_TYPE_COUNT; // +0x158 = 5
        mePrepareStage    = E_PREPARESTAGE_DONE;
        return true;

    default:
        FireSatNavAssert(" unknown prepare stage in SatNavRender component ", 285);
        return false;
    }
}

// 0x824456E0
bool SatNavRenderer::Release()
{
    switch (meReleaseStage)
    {
    case E_RELEASESTAGE_START:
        // Free each owned texture/blend state through the resource allocator's destroy slot
        // (X360 (*(*mpHeapAllocator + 0x14))(mpHeapAllocator, &resource)). The allocator type is
        // uncommitted, so the dispatch is omitted but the null-out side effects are reproduced.
        if (mpMapTextureState)          mpMapTextureState = 0;
        if (mpMapBlendState)            mpMapBlendState = 0;
        if (mpMaskTextureState)         mpMaskTextureState = 0;
        if (mpMaskBlendState)           mpMaskBlendState = 0;
        if (mpRouteSegmentTextureState) mpRouteSegmentTextureState = 0;
        break;

    case E_RELEASESTAGE_DONE:
        // Already released -- fall through to the common tail (no extra work).
        break;

    default:
        FireSatNavAssert(" unknown release stage in SatNavRender component ", 361);
        return false;
    }

    meReleaseStage    = E_RELEASESTAGE_DONE;
    meIconDisplayType = GuiEventEnableSatNavIcons::E_ICON_DISPLAY_TYPE_COUNT; // +0x158 = 5
    return true;
}

// 0x82449940
void SatNavRenderer::RecvEvent(const void* lpEvent, s32 liEventType)
{
    if (lpEvent == 0)
        FireSatNavAssert(" null event passed ", 416);

    const u32* const lpuEventWords = reinterpret_cast<const u32*>(lpEvent);

    // Event-type ids (CgsModule event constants observed in the asm).
    enum
    {
        KI_EVENT_REFRESH_ICON_INFO = 311, // 0x137 -- RefreshSatNavIconInfo(event->[+0x10])
        KI_EVENT_SET_CACHE         = 64,  // 0x40  -- latch GuiCache pointer (event->[0])
        KI_EVENT_ENABLE_ICONS      = 204, // 0xCC  -- display type / mode filter / draw-route
        KI_EVENT_RENDER_SATNAV     = 212, // 0xD4  -- copy 48-byte GuiEventRenderSatNav payload
        KI_EVENT_REQUEST_REFRESH_A = 415, // 0x19F -- request icon refresh
        KI_EVENT_REQUEST_REFRESH_B = 556, // 0x22C -- request icon refresh
    };

    if (liEventType > KI_EVENT_REFRESH_ICON_INFO)
    {
        if (liEventType == KI_EVENT_REQUEST_REFRESH_A || liEventType == KI_EVENT_REQUEST_REFRESH_B)
            mbRefreshSatNavIcons = true;          // +0x5A1
        return;
    }

    switch (liEventType)
    {
    case KI_EVENT_REFRESH_ICON_INFO:
        RefreshSatNavIconInfo(static_cast<s32>(lpuEventWords[4])); // event->[+0x10]
        break;

    case KI_EVENT_SET_CACHE:
        if (lpuEventWords[0] == 0)
            FireSatNavAssert("lpCacheEvent->mpCachePointer", 430);
        mpGuiCache = reinterpret_cast<GuiCache*>(static_cast<uintptr_t>(lpuEventWords[0])); // +0x90
        break;

    case KI_EVENT_ENABLE_ICONS:
        // event[+8] (byte) -> +0x5A0 mbRenderEventStarts; event[0] -> +0x158 meIconDisplayType;
        // event[+4] -> +0x15C meGameModeFilter.
        mbRenderEventStarts = (reinterpret_cast<const u8*>(lpEvent)[8] != 0);
        meIconDisplayType =
            static_cast<GuiEventEnableSatNavIcons::EIconDisplayType>(lpuEventWords[0]);
        meGameModeFilter  = static_cast<s32>(lpuEventWords[1]);
        break;

    case KI_EVENT_RENDER_SATNAV:
        std::memcpy(mRenderSatNavEvent, lpEvent, 48); // +0x60 <- 48-byte payload
        break;

    default:
        break;
    }
}

// 0x82451400
u32 SatNavRenderer::GetNumIcons() const
{
    switch (meIconDisplayType)
    {
    case GuiEventEnableSatNavIcons::E_ICON_DISPLAY_TYPE_OFFLINE_EVENTS:      // 0
        return mpGuiCache->GetNumProfileEvents();

    case GuiEventEnableSatNavIcons::E_ICON_DISPLAY_TYPE_ONLINE_EVENT_STARTS: // 1
        return static_cast<u32>(mpGuiCache->GetNumPresetEvents());

    case GuiEventEnableSatNavIcons::E_ICON_DISPLAY_TYPE_ONLINE_CHECKPOINTS:  // 2
    {
        WorldDataController* const lpWdc = mpGuiCache->GetWorldDataController();
        const s32 liCount = lpWdc->GetTotalNumberOfOnlineLandmarks();
        if (liCount >= 175)
            return 175;
        return static_cast<u32>(liCount);
    }

    default:
        FireSatNavAssert("Invalid icon display type in GetNumIcons", 858);
        return 0;
    }
}

// 0x82449AA8 -- fill *lpInfo with the cached icon record for icon `luIndex`, sourced from the
// profile / preset / online-landmark list per the current display type.
void SatNavRenderer::GetIconInformation(u32 luIndex, IconRendererSatNavIconInfo* lpInfo) const
{
    switch (meIconDisplayType)
    {
    case GuiEventEnableSatNavIcons::E_ICON_DISPLAY_TYPE_OFFLINE_EVENTS:      // 0 (profile)
    {
        const BrnProgression::ProfileEvent* const lpProfileEvent = mpGuiCache->GetProfileEvent(luIndex);
        const u32 luEventId = lpProfileEvent->GetID();

        const SatNavEventDisplayInfo* const lpDisplay = mpGuiCache->GetProfileEventDisplayInfo(luEventId);

        WorldDataController* const lpWdc = mpGuiCache->GetWorldDataController();
        const BrnProgression::RaceEventData* const lpEventInfo = lpWdc->GetEventInfoFromEventId(luEventId);

        lpInfo->mv3Position = lpDisplay->mv3Position;
        lpInfo->miEventId   = static_cast<s32>(luEventId);

        const u32 luIconRow = KAU_EVENTTYPE_TO_ICONROW[lpEventInfo->GetEventTypeByte()];
        lpInfo->muEventTypeIndex = luIconRow;

        // Completed if the player has a rank/non-rank win, or a special-event win when this is
        // the "row 2" icon type.
        const u16 lu16Flags = lpProfileEvent->GetFlags();
        const bool lbCompleted =
            (lu16Flags & BrnProgression::ProfileEvent::E_FLAG_RANK_WIN) != 0 ||
            (lu16Flags & BrnProgression::ProfileEvent::E_FLAG_NON_RANK_WIN) != 0 ||
            ((lu16Flags & BrnProgression::ProfileEvent::E_FLAG_WON_SPECIAL_EVENT_BEFORE) != 0 &&
             luIconRow == 2);
        lpInfo->meSatNavIconType = lbCompleted ? E_SATNAVICON_EVENT_COMPLETED
                                               : E_SATNAVICON_EVENT_NOTATTEMPTED;

        // The X360 then overrides row 2 -> 0 when this is the cache's CURRENT online event (it
        // compares two far GuiCache id members at +19192/+19196). Those members are not yet
        // recovered on GuiCache, so the override is conservatively NOT applied. FLAG.
        break;
    }

    case GuiEventEnableSatNavIcons::E_ICON_DISPLAY_TYPE_ONLINE_EVENT_STARTS: // 1 (preset)
    {
        const PresetEvent* const lpPresetEvent = mpGuiCache->GetPresetEvent(static_cast<s32>(luIndex));
        const u32 luLookupId = lpPresetEvent->GetPositionLookupId();          // presetEvent[+0x20]

        const SatNavEventDisplayInfo* const lpDisplay = mpGuiCache->GetPresetEventDisplayInfo(luLookupId);

        // The WDC lookup uses the display record's own event-instance id (X360 reads
        // display[+0x18]), NOT the preset event's position-lookup id.
        WorldDataController* const lpWdc = mpGuiCache->GetWorldDataController();
        const BrnProgression::RaceEventData* const lpEventInfo =
            lpWdc->GetEventInfoFromEventId(lpDisplay->muEventInstanceId);

        lpInfo->mv3Position      = lpDisplay->mv3Position;
        lpInfo->miEventId        = static_cast<s32>(lpPresetEvent->GetEventId());     // presetEvent[+0x28]
        lpInfo->muEventTypeIndex = static_cast<u32>(lpEventInfo->GetIconFrameBase()) + 6;
        lpInfo->meSatNavIconType = E_SATNAVICON_EVENT_NOTATTEMPTED;
        break;
    }

    case GuiEventEnableSatNavIcons::E_ICON_DISPLAY_TYPE_ONLINE_CHECKPOINTS:  // 2 (online landmark)
    {
        GuiEventUpdateSatNav::SatNavIconInfo lLandmark;
        mpGuiCache->GetOnlineLandmarkInfoAtPositionInList(static_cast<s32>(luIndex), &lLandmark);

        // The landmark record carries its own world position (leading 16-byte lane) + a signed
        // landmark-index half-word; the X360 copies them into the cached icon and fixes the row to 6.
        const Vector4& lv4Pos = lLandmark.GetPositionLane();
        lpInfo->mv3Position.x = lv4Pos.x;
        lpInfo->mv3Position.y = lv4Pos.y;
        lpInfo->mv3Position.z = lv4Pos.z;
        lpInfo->miEventId        = static_cast<s32>(lLandmark.GetLandmarkIndexHalf()); // sign-extended @0x20
        lpInfo->muEventTypeIndex = 6;
        lpInfo->meSatNavIconType = E_SATNAVICON_EVENT_NOTATTEMPTED;
        break;
    }

    default:
        FireSatNavAssert("Invalid icon display type in GetIconInformation", 958);
        break;
    }
}

// 0x8245A1D8
void SatNavRenderer::InitResources()
{
    if (KU_NUM_RESOURCES_TO_LOAD < E_SATNAVICON_NUM)
        FireSatNavAssert("muNumResourcesToLoad >= E_SATNAVICON_NUM", 652);

    for (u32 luTextureIndex = 0; luTextureIndex < KU_NUM_RESOURCES_TO_LOAD; ++luTextureIndex)
    {
        const void* const lpTexture =
            mpGuiCache->GetLoadedResource(KA_RESOURCES_TO_LOAD[luTextureIndex].muResourceId);
        if (lpTexture == 0)
            FireSatNavAssert("lpTexture!=NULL", 662);

        // Build the texture-state init descriptor from the loaded texture + a fixed resource
        // descriptor, then create the icon texture state through the resource allocator. The X360
        // copies a 5-dword descriptor block from the allocator-built state into maIconResources;
        // modelled here as the named creation of mapIconTextureStates[luTextureIndex] (the
        // allocator dispatch + descriptor copy use uncommitted renderengine types -> reduced to
        // the named result store). FLAG: texture-state creation reduced to a named pointer store.
        mapIconTextureStates[luTextureIndex] =
            reinterpret_cast<renderengine::TextureState*>(const_cast<void*>(lpTexture));

        if (mapIconTextureStates[luTextureIndex] == 0)
            FireSatNavAssert("mapIconTextureStates[liTextureIndex]", 673);
    }

    InitEventTypeUvs();
}

// 0x82450E88 -- generate the per-event-type icon UV tables (and mini-icon tables) by walking
// the icon atlas. The X360 emits a verbose "unknown event type" debug-print path for the switch
// default; here the frame number is just the icon's 1-based index, so the switch (cases 0..10 ->
// 1..11) reduces to `frameNumber = eventType + 1`.
void SatNavRenderer::InitEventTypeUvs()
{
    // ---- full icon set: 2 texture resolutions x 11 event types ----
    for (u32 luTextureIndex = 0; luTextureIndex < 2; ++luTextureIndex)
    {
        for (u32 luEventType = 0; luEventType < KU_ICON_EVENT_TYPE_COUNT; ++luEventType)
        {
            const s32 liFrameNumber = static_cast<s32>(luEventType) + 1; // switch 0..10 -> 1..11
            CalculateUVsForIndex(
                liFrameNumber,
                mav2IconUvTopLeft[luTextureIndex][luEventType],
                mav2IconUvBottomLeft[luTextureIndex][luEventType],
                mav2IconUvTopRight[luTextureIndex][luEventType],
                mav2IconUvBottomRight[luTextureIndex][luEventType],
                KAF_TEXTURE_WIDTH[luTextureIndex], KAF_TEXTURE_HEIGHT[luTextureIndex],
                KAF_ICON_WIDTH[luTextureIndex], KAF_ICON_HEIGHT[luTextureIndex]);
        }
    }

    // ---- mini-icon set: 2 texture resolutions x 6 mini icons; lower atlas band (+offset V) ----
    for (u32 luTextureIndex = 0; luTextureIndex < 2; ++luTextureIndex)
    {
        for (u32 luMiniIndex = 0; luMiniIndex < E_SATNAVICON_EVENTTYPE_MINI_INDEX_COUNT; ++luMiniIndex)
        {
            const s32 liFrameNumber = static_cast<s32>(luMiniIndex);
            CalculateUVsForIndex(
                liFrameNumber,
                mav2MiniIconUvTopLeft[luTextureIndex][luMiniIndex],
                mav2MiniIconUvBottomLeft[luTextureIndex][luMiniIndex],
                mav2MiniIconUvTopRight[luTextureIndex][luMiniIndex],
                mav2MiniIconUvBottomRight[luTextureIndex][luMiniIndex],
                KAF_TEXTURE_WIDTH[luTextureIndex], KAF_TEXTURE_HEIGHT[luTextureIndex],
                KAF_MINI_ICON_WIDTH[luTextureIndex], KAF_MINI_ICON_HEIGHT[luTextureIndex]);

            mav2MiniIconUvTopLeft[luTextureIndex][luMiniIndex].y     += KF_MINI_ICON_TEXTURE_OFFSET;
            mav2MiniIconUvBottomLeft[luTextureIndex][luMiniIndex].y  += KF_MINI_ICON_TEXTURE_OFFSET;
            mav2MiniIconUvTopRight[luTextureIndex][luMiniIndex].y    += KF_MINI_ICON_TEXTURE_OFFSET;
            mav2MiniIconUvBottomRight[luTextureIndex][luMiniIndex].y += KF_MINI_ICON_TEXTURE_OFFSET;
        }
    }
}

// 0x824514B0 -- (re)build the cached on-map icon set from the current event list, but only when a
// refresh was requested or the cache emptied between frames.
void SatNavRenderer::InitSatNavIcons()
{
    const u32 luNumberOfIcons = GetNumIcons();

    if (mbRefreshSatNavIcons ||
        (luNumberOfIcons != 0 && muNumberOfSatNavIcons == 0))
    {
        mbRefreshSatNavIcons  = false;
        muNumberOfSatNavIcons = 0;

        if (luNumberOfIcons > KU_MAX_SATNAV_ICONS)
            FireSatNavAssert("luNumberOfIcons <= KU_MAX_SATNAV_ICONS", 1265);

        for (u32 luIcon = 0; luIcon < luNumberOfIcons; ++luIcon)
        {
            GetIconInformation(luIcon, &maCachedSatNavIcons[luIcon]);
            ++muNumberOfSatNavIcons;
        }
    }
}

// 0x824458A0 -- add/refresh a single cached icon for one event id (driven by the
// REFRESH_ICON_INFO event). Finds an existing slot for the event or appends a new one.
void SatNavRenderer::RefreshSatNavIconInfo(s32 liEventId)
{
    if (muNumberOfSatNavIcons >= KU_MAX_SATNAV_ICONS)
        FireSatNavAssert("muNumberOfSatNavIcons < KU_MAX_SATNAV_ICONS", 1292);

    // Search the cached icons for this event id. If found, there is nothing to do.
    u32 luIconIndex = 0;
    if (muNumberOfSatNavIcons != 0)
    {
        for (luIconIndex = 0; luIconIndex < muNumberOfSatNavIcons; ++luIconIndex)
        {
            if (maCachedSatNavIcons[luIconIndex].miEventId == liEventId)
                return; // already cached
        }
    }

    // Not cached -> append a fresh entry.
    const SatNavEventDisplayInfo* const lpDisplay =
        mpGuiCache->GetProfileEventDisplayInfo(static_cast<u32>(liEventId));

    WorldDataController* const lpWdc = mpGuiCache->GetWorldDataController();
    if (lpWdc == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mpWorldDataController",
                                   "..\\..\\..\\GameSource\\Gui/BrnGuiCache.h", 2324);
        CgsDev::Assert::EndAssert();
    }

    const BrnProgression::RaceEventData* const lpRaceEventData =
        lpWdc->GetEventInfoFromEventId(static_cast<u32>(liEventId));

    if (lpDisplay == 0)
        FireSatNavAssert("lpEventStart", 1306);
    if (lpRaceEventData == 0)
        FireSatNavAssert("lpRaceEventData", 1307);
    if (luIconIndex >= KU_MAX_SATNAV_ICONS)
        FireSatNavAssert("luIconIndex < KU_MAX_SATNAV_ICONS", 1308);

    IconRendererSatNavIconInfo& lIcon = maCachedSatNavIcons[luIconIndex];
    lIcon.miEventId        = liEventId;
    lIcon.meSatNavIconType = E_SATNAVICON_EVENT_NOTATTEMPTED;

    const u32 luIconRow = KAU_EVENTTYPE_TO_ICONROW[lpRaceEventData->GetEventTypeByte()];
    lIcon.muEventTypeIndex = luIconRow;
    lIcon.mv3Position      = lpDisplay->mv3Position;

    // The X360 then overrides row 2 -> 0 when this event is the cache's current online event (it
    // compares two far GuiCache id members at +19192/+19196). Those members are not yet recovered
    // on GuiCache, so the override is conservatively NOT applied (matches GetIconInformation). FLAG.

    ++muNumberOfSatNavIcons;
}

// 0x8245F978 -- draw the visible sat-nav icons (those inside the map viewport) plus the closest
// off-screen icon clamped to the viewport edge. Heavily VMX-vectorised on X360; the per-icon
// world->device transform + viewport clamp is reconstructed at the semantic level via MapTransform.
void SatNavRenderer::RenderIconsForSatNav(CgsGraphics::Im2dRenderBuffer* lpRenderBuffer)
{
    if (mpGuiCache == 0)
        FireSatNavAssert("mpGuiCache", 1013);
    if (lpRenderBuffer == 0)
        FireSatNavAssert("lpRenderBuffer", 1014);
    if (mfZoomLevel == 0.0f)
        FireSatNavAssert("mfZoomLevel != 0.0f", 1015);

    // The render pass only runs once event starts have been enabled (X360 gate on +0x5A0
    // mbRenderEventStarts).
    if (!mbRenderEventStarts)
        return;

    // (Re)build the per-icon SatNav info before iterating (X360 @0x8245FA60: `bl InitSatNavIcons`
    // unconditionally after the gate, before loading muNumberOfSatNavIcons @0x8245FA64).
    InitSatNavIcons();

    // The world-camera position is latched ONCE here (X360 @0x8245FA48 lvx128 v124, mpGuiCache,
    // 0x4AE0). The per-icon off-screen distance below measures against it.
    const Vector4& lv4CameraPos = mpGuiCache->GetWorldCameraPosition();

    // 1 / mfZoomLevel (X360 @0x8245FA4C fdivs f31, 1.0, mfZoomLevel). Every per-icon half-extent
    // is this-scaled.
    const f32 lfInvZoom = 1.0f / mfZoomLevel;

    // Pre-scale the per-icon-type half-extent tables by the inverse zoom (X360 @0x8245FA9C-FAE4:
    // a 2-iteration loop over meSatNavIconType in {0,1} that fills the four stack arrays
    // var_178/180/188/190). The display-type-0 (profile) path reads the WORLDSIZE pair; the online
    // paths read the SCREENSPACE pair (note the SCREENSPACE pair maps height->halfWidth-arg and
    // width->halfHeight-arg, matching the X360 r6/r7 selection).
    f32 lafHalfWidthScreen[SatNavRenderer::E_SATNAVICON_NUM];   // var_178 (flt_82FB3708 * invZoom)
    f32 lafHalfHeightWorld[SatNavRenderer::E_SATNAVICON_NUM];   // var_180 (flt_82FB3728 * invZoom)
    f32 lafHalfWidthWorld[SatNavRenderer::E_SATNAVICON_NUM];    // var_188 (flt_82FB36F0 * invZoom)
    f32 lafHalfHeightScreen[SatNavRenderer::E_SATNAVICON_NUM];  // var_190 (flt_82FB3720 * invZoom)
    for (u32 luType = 0; luType < SatNavRenderer::E_SATNAVICON_NUM; ++luType)
    {
        lafHalfWidthScreen[luType]  = KAF_ICON_HALFWIDTH_SCREENSPACE[luType]  * lfInvZoom;
        lafHalfHeightWorld[luType]  = KAF_ICON_HALFHEIGHT_WORLDSIZE[luType]   * lfInvZoom;
        lafHalfWidthWorld[luType]   = KAF_ICON_HALFWIDTH_WORLDSIZE[luType]    * lfInvZoom;
        lafHalfHeightScreen[luType] = KAF_ICON_HALFHEIGHT_SCREENSPACE[luType] * lfInvZoom;
    }

    u32  luClosestIconIndex = KU_MAX_SATNAV_ICONS; // 150 == "none"
    u32  luFirstEmptyIcon   = KU_MAX_SATNAV_ICONS;
    s32  liNumDrawn         = 0;
    f32  lfClosestDistSq    = 3.4028235e38f;        // FLT_MAX
    bool lbDrawClosest      = false;

    if (muNumberOfSatNavIcons == 0)
        return;

    const bool lbProfileDisplay =
        (meIconDisplayType == GuiEventEnableSatNavIcons::E_ICON_DISPLAY_TYPE_OFFLINE_EVENTS);

    for (u32 luIcon = 0; luIcon < muNumberOfSatNavIcons; ++luIcon)
    {
        IconRendererSatNavIconInfo& lIcon = maCachedSatNavIcons[luIcon];

        // An empty slot has BOTH muEventTypeIndex == 0 and meSatNavIconType == 0 (X360
        // @0x8245FB40: lwz *(base+0x5C4)=muEventTypeIndex, then *(base+0x5C8)=meSatNavIconType).
        // miEventId (+0x5C0) is NOT read here.
        bool lbEmptySlot = false;
        if (lIcon.muEventTypeIndex == 0 && lIcon.meSatNavIconType == E_SATNAVICON_EVENT_NOTATTEMPTED)
        {
            luFirstEmptyIcon = luIcon;
            lbEmptySlot      = true;
        }

        if (luIcon >= KU_MAX_SATNAV_ICONS)
            FireSatNavAssert("luIconIndex < KU_MAX_SATNAV_ICONS", 1080);

        // World -> device, then the world->device MakeTransform chain. The transformed 2D result
        // (X360 var_1A0 = v87/v88) is what every gate and the draw call below read; the raw
        // WorldToDevice result feeds only the transform input.
        const Vector2 lv2Device      = MapTransform::WorldToDevice(lIcon.mv3Position, false);
        const Vector2 lv2Transformed = MapTransform::Transform(
            lv2Device, MapTransform::GetWorldSpace(), MapTransform::GetDeviceSpace());

        if (lbEmptySlot)
            continue;

        // Per-icon-type half-extents for this display mode (X360 r6 -> f2=lfHalfWidth,
        // r7 -> f4=lfHalfHeight; selected by meIconDisplayType == 0).
        const u32 luType = static_cast<u32>(lIcon.meSatNavIconType);
        const f32 lfHalfWidth  = lbProfileDisplay ? lafHalfWidthWorld[luType]
                                                  : lafHalfHeightScreen[luType];
        const f32 lfHalfHeight = lbProfileDisplay ? lafHalfHeightWorld[luType]
                                                  : lafHalfWidthScreen[luType];

        // Inside the viewport -> draw it. The visible test reads the TRANSFORMED result
        // (X360 @0x8245FCAC fcmpu f1/f3 vs 0.0/1.0, f1/f3 == var_1A0).
        if (lv2Transformed.x >= KF_VISIBLE_MIN && lv2Transformed.x <= KF_VISIBLE_MAX &&
            lv2Transformed.y >= KF_VISIBLE_MIN && lv2Transformed.y <= KF_VISIBLE_MAX)
        {
            RenderSatNavIcon(lv2Transformed.x, lfHalfWidth, lv2Transformed.y, lfHalfHeight,
                             lIcon.meSatNavIconType, lIcon.muEventTypeIndex, lpRenderBuffer);
            ++liNumDrawn;
        }

        // Near/outside an edge -> candidate for the "closest off-screen" marker. The edge test
        // also reads the TRANSFORMED result (X360 @0x8245FD14 fcmpu f31/f30 vs 0.025/0.975,
        // f31/f30 == var_1A0).
        if (lv2Transformed.x <= KF_ICON_CLAMP_MIN || lv2Transformed.x >= KF_ICON_CLAMP_MAX ||
            lv2Transformed.y <= KF_ICON_CLAMP_MIN || lv2Transformed.y >= KF_ICON_CLAMP_MAX)
        {
            // X360 distance = |iconPos - cameraPos|^2 over the first 3 lanes (vsubfp128 +
            // vmsum3fp128 @0x8245FD34). cameraPos is the GuiCache world-camera lane latched above.
            const Vector3& lv3 = lIcon.mv3Position;
            const f32 lfDx = lv3.x - lv4CameraPos.x;
            const f32 lfDy = lv3.y - lv4CameraPos.y;
            const f32 lfDz = lv3.z - lv4CameraPos.z;
            const f32 lfDistSq    = lfDx * lfDx + lfDy * lfDy + lfDz * lfDz;
            const f32 lfAbsDistSq = lfDistSq < 0.0f ? -lfDistSq : lfDistSq; // X360 fabs @0x8245FD48
            if (lfAbsDistSq < lfClosestDistSq)
            {
                lfClosestDistSq    = lfAbsDistSq;
                luClosestIconIndex = luIcon;
            }
        }
    }

    // Decide whether to draw a clamped "closest off-screen" marker.
    if (liNumDrawn != 0)
    {
        if (luFirstEmptyIcon != KU_MAX_SATNAV_ICONS)
        {
            luClosestIconIndex = luFirstEmptyIcon;
            lbDrawClosest      = true;
        }
    }
    else if (luFirstEmptyIcon != KU_MAX_SATNAV_ICONS)
    {
        luClosestIconIndex = luFirstEmptyIcon;
        lbDrawClosest      = true;
    }
    else
    {
        // Nothing drawn and no empty slot: draw the nearest off-screen icon if one was found.
        lbDrawClosest = (lfClosestDistSq != 3.4028235e38f);
    }

    if (!lbDrawClosest)
        return;

    if (luClosestIconIndex >= KU_MAX_SATNAV_ICONS)
        FireSatNavAssert("luClosestIconIndex < KU_MAX_SATNAV_ICONS", 1172);

    IconRendererSatNavIconInfo& lClosest = maCachedSatNavIcons[luClosestIconIndex];
    const Vector2 lv2Device      = MapTransform::WorldToDevice(lClosest.mv3Position, true);
    const Vector2 lv2Transformed = MapTransform::Transform(
        lv2Device, MapTransform::GetWorldSpace(), MapTransform::GetDeviceSpace());

    // Clamp the marker into the visible band [0.025, 0.975] (X360 @0x8245FEBC-FF00).
    f32 lfEventStartX = lv2Transformed.x;
    f32 lfEventStartY = lv2Transformed.y;
    lfEventStartX = lfEventStartX < KF_ICON_CLAMP_MIN ? KF_ICON_CLAMP_MIN
                  : (lfEventStartX > KF_ICON_CLAMP_MAX ? KF_ICON_CLAMP_MAX : lfEventStartX);
    lfEventStartY = lfEventStartY < KF_ICON_CLAMP_MIN ? KF_ICON_CLAMP_MIN
                  : (lfEventStartY > KF_ICON_CLAMP_MAX ? KF_ICON_CLAMP_MAX : lfEventStartY);

    if (lfEventStartX >= KF_VISIBLE_MAX || lfEventStartX <= KF_VISIBLE_MIN ||
        lfEventStartY >= KF_VISIBLE_MAX || lfEventStartY <= KF_VISIBLE_MIN)
    {
        FireSatNavAssert("Failed to clamp icon to sat nav properly", 1215);
    }

    // Same per-icon-type half-extent selection as the in-loop draw (X360 @0x824600CC-82460108).
    const u32 luClosestType = static_cast<u32>(lClosest.meSatNavIconType);
    const f32 lfClosestHalfWidth  = lbProfileDisplay ? lafHalfWidthWorld[luClosestType]
                                                     : lafHalfHeightScreen[luClosestType];
    const f32 lfClosestHalfHeight = lbProfileDisplay ? lafHalfHeightWorld[luClosestType]
                                                     : lafHalfWidthScreen[luClosestType];

    RenderSatNavIcon(lfEventStartX, lfClosestHalfWidth, lfEventStartY, lfClosestHalfHeight,
                     lClosest.meSatNavIconType, lClosest.muEventTypeIndex, lpRenderBuffer);
}

// The immediate-mode renderer's default blend state every sat-nav icon quad binds (X360 reads
// the pointer-valued global dword_83010F20; the same default-blend singleton BrnFlaptRenderer
// binds). No reconstructed C++ home yet; declared extern as the data reference the draw reads.
extern const CgsGraphics::BlendState* const gpDefaultImBlendState; // dword_83010F20

// 0x8245A3A0 -- build the four-vertex quad (top-left, bottom-left, top-right, bottom-right) for
// one sat-nav icon and submit it through the immediate-mode render buffer. The quad colour is the
// renderer's map-quad colour byte-swapped to the vertex RGBA packing; the four UVs come from the
// per-(icon-type-row, event-type-column) corner tables -- the full-icon tables for the online
// display modes, the mini-icon tables for the profile (display-type-0) mode.
void SatNavRenderer::RenderSatNavIcon(f32 lfX, f32 lfHalfWidth, f32 lfY, f32 lfHalfHeight,
                                      ESatNavIconType leIconType, u32 luEventTypeIndex,
                                      CgsGraphics::Im2dRenderBuffer* lpRenderBuffer)
{
    if (leIconType >= E_SATNAVICON_NUM)
        FireSatNavAssert("leIconType < E_SATNAVICON_NUM", 1350);
    if (luEventTypeIndex >= KU_ICON_EVENT_TYPE_COUNT)
        FireSatNavAssert("luEventTypeIndex < KU_ICON_EVENT_TYPE_COUNT", 1351);
    if (lpRenderBuffer == 0)
        FireSatNavAssert("lpRenderBuffer", 1352);

    // Quad corner positions: f1/f3 are the centre, f2/f4 the half-extents (X360 @0x8245A44C-A4A8).
    const f32 lfLeft   = lfX - lfHalfWidth;
    const f32 lfRight  = lfX + lfHalfWidth;
    const f32 lfTop    = lfY - lfHalfHeight;
    const f32 lfBottom = lfY + lfHalfHeight;

    // Pack the map-quad colour into the vertex RGBA8. The X360 (@0x8245A468-A4B4) does a FULL
    // byte-reverse of mMapQuadColour (+0x08): with W = B0B1B2B3 (B0=MSB) the result word is
    // 0xB3B2B1B0, stored big-endian into RGBA8{r,g,b,a} -> r=W&0xFF, g=(W>>8)&0xFF, b=(W>>16)&0xFF,
    // a=(W>>24)&0xFF. So for 0xE5FFFFFF -> r/g/b=0xFF, a=0xE5 (translucent white).
    const u8 lbB0 = static_cast<u8>(mMapQuadColour >> 24);   // W MSB -> alpha
    const u8 lbB1 = static_cast<u8>(mMapQuadColour >> 16);
    const u8 lbB2 = static_cast<u8>(mMapQuadColour >> 8);
    const u8 lbB3 = static_cast<u8>(mMapQuadColour);         // W LSB -> red
    CgsGraphics::RGBA8 lColour;
    lColour.r = lbB3;   // r <- W & 0xFF
    lColour.g = lbB2;   // g <- (W>>8) & 0xFF
    lColour.b = lbB1;   // b <- (W>>16) & 0xFF
    lColour.a = lbB0;   // a <- (W>>24) & 0xFF

    // Select the UV corner tables: profile (display-type-0) -> the mini-icon tables; the online
    // modes -> the full-icon tables (X360 @0x8245A458 gate on meIconDisplayType == 0).
    const u32 luRow = static_cast<u32>(leIconType);
    const bool lbMiniIcons =
        (meIconDisplayType == GuiEventEnableSatNavIcons::E_ICON_DISPLAY_TYPE_OFFLINE_EVENTS);

    Vector2 lv2UvTopLeft, lv2UvBottomLeft, lv2UvTopRight, lv2UvBottomRight;
    if (lbMiniIcons)
    {
        lv2UvTopLeft     = mav2MiniIconUvTopLeft[luRow][luEventTypeIndex];
        lv2UvBottomLeft  = mav2MiniIconUvBottomLeft[luRow][luEventTypeIndex];
        lv2UvTopRight    = mav2MiniIconUvTopRight[luRow][luEventTypeIndex];
        lv2UvBottomRight = mav2MiniIconUvBottomRight[luRow][luEventTypeIndex];
    }
    else
    {
        lv2UvTopLeft     = mav2IconUvTopLeft[luRow][luEventTypeIndex];
        lv2UvBottomLeft  = mav2IconUvBottomLeft[luRow][luEventTypeIndex];
        lv2UvTopRight    = mav2IconUvTopRight[luRow][luEventTypeIndex];
        lv2UvBottomRight = mav2IconUvBottomRight[luRow][luEventTypeIndex];
    }

    // Four vertices in (top-left, bottom-left, top-right, bottom-right) order -> a triangle strip
    // (X360 submits primitive type 6 with 4 vertices @0x8245A5D8).
    CgsGraphics::Basic2dColouredTexturedVertex laVertices[4];

    laVertices[0].mv2Pos.x = lfLeft;   laVertices[0].mv2Pos.y = lfTop;
    laVertices[0].mv4Colour = lColour; laVertices[0].mv2Tex0UV.x = lv2UvTopLeft.x;
    laVertices[0].mv2Tex0UV.y = lv2UvTopLeft.y;

    laVertices[1].mv2Pos.x = lfLeft;   laVertices[1].mv2Pos.y = lfBottom;
    laVertices[1].mv4Colour = lColour; laVertices[1].mv2Tex0UV.x = lv2UvBottomLeft.x;
    laVertices[1].mv2Tex0UV.y = lv2UvBottomLeft.y;

    laVertices[2].mv2Pos.x = lfRight;  laVertices[2].mv2Pos.y = lfTop;
    laVertices[2].mv4Colour = lColour; laVertices[2].mv2Tex0UV.x = lv2UvTopRight.x;
    laVertices[2].mv2Tex0UV.y = lv2UvTopRight.y;

    laVertices[3].mv2Pos.x = lfRight;  laVertices[3].mv2Pos.y = lfBottom;
    laVertices[3].mv4Colour = lColour; laVertices[3].mv2Tex0UV.x = lv2UvBottomRight.x;
    laVertices[3].mv2Tex0UV.y = lv2UvBottomRight.y;

    // Bind this icon-type's texture state + the default blend state, then submit the quad
    // (X360 SetState(texture) @0x8245A5B4, SetState(blend, dword_83010F20) @0x8245A5C4,
    // Render(6, verts, 4) @0x8245A5D8). On the PC fold the ImRenderer<V> API is reached by name.
    lpRenderBuffer->SetState(mapIconTextureStates[luRow]);
    lpRenderBuffer->SetState(gpDefaultImBlendState);
    lpRenderBuffer->Render(static_cast<renderengine::PrimitiveType>(6), laVertices, 4);
}

// 0x8245F4D8 -- rebuild the screen-space mTransform from the on-screen viewport rectangle and the
// display aspect ratio, in place. The X360 body is hand-vectorised (VMX128 lvx128/vspltw/vsubfp/
// vmaddfp/vperm/vsldoi): it loads the viewport descriptor (unk_82FB36A0, a Vector4 of the
// rectangle's left/top/right/bottom screen params), computes the (right-left) / (bottom-top)
// extents and a {2.0, -2.0} normalised-device scale, and permutes them (with the control vectors
// unk_82CDA350 / unk_82CDA3C0) into the transform's origin + right/up basis Vector4 lanes, then
// folds in the aspect ratio.
//
// SEMANTIC-LEVEL SIMD: per the BrnSatNavRenderer.h policy (and matching CgsIm2d.cpp's note on
// Im2dTransform::TransformByAspectRatio), the VMX128 permute math has no portable PC equivalent and
// the input rectangle (unk_82FB36A0) + permute-control vectors (unk_82CDA350/3C0) are NOT in the
// symbol export (data segment not dumped). The reconstruction reproduces the faithful STRUCTURE --
// zero the transform, set the normalised-device scale lanes, and call TransformByAspectRatio() --
// and FLAGS the produced basis as data-fidelity-limited (the unrecovered viewport rect would set
// the origin + extents). The X360 store targets (mOriginXYZ/mRightUp/mColourShift/mColourScale at
// +0/+16/+32/+48) and the trailing TransformByAspectRatio() call are reproduced exactly.
void SatNavRenderer::UpdateRendererTransform()
{
    // The X360 {2.0, -2.0} normalised-device scale literals (flt_82001C98 == 1.0,
    // flt_82006D70 == -2.0, flt_82001D9C, flt_82001CC0 == 0.0) recovered from the asm immediates.
    const f32 KF_NDC_SCALE_X =  2.0f; // 1.0 * 2.0 (@0x8245F5FC-F604)
    const f32 KF_NDC_SCALE_Y = -2.0f; // 2.0 * -2.0 (@0x8245F610-F618)

    // FLAG: the viewport rectangle (unk_82FB36A0) and the two vperm control vectors
    // (unk_82CDA350 / unk_82CDA3C0) are not in the symbol export; the origin + per-axis extents
    // they would feed are left at the normalised-device default below. The structure and the
    // aspect-ratio fold are X360-faithful; the produced origin/extent numbers are unverified.
    mTransform.mOriginXYZ.SetZero();
    mTransform.mRightUp.SetZero();
    mTransform.mColourShift.SetZero();
    mTransform.mColourScale.SetZero();

    mTransform.mRightUp.x = KF_NDC_SCALE_X;
    mTransform.mRightUp.y = KF_NDC_SCALE_Y;

    // Fold the current display aspect ratio into the transform basis in place (X360 @0x8245F6B4).
    mTransform.TransformByAspectRatio();
}

} // namespace BrnGui

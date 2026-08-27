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
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"         // BrnGui::GuiEventJunctionInfo (the case-311 typed read)
#include "GameSource/Gui/BrnGuiWorldDataController.h"   // BrnGui::WorldDataController
#include "GameSource/GameState/Progression/BrnProfile.h"// BrnProgression::ProfileEvent
#include "SharedClasses/Progression/BrnRaceEventData.h" // BrnProgression::RaceEventData
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"       // BrnGui::MapTransform

#include "GameShared/GameClasses/Core/CgsAssert.h"      // BeginAssert/FireAssert/EndAssert
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderBuffer.h" // Im2dRenderBuffer (SetState/Render)
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBufferTemplate.h" // ImRenderBuffer<V> (the Apt 2D command buffer)
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptRenderHandler.h"  // CgsGui::AptIm2dRenderBuffer (set slot 0)
#include "GameSource/Gui/BrnCustomRendererManager.h"    // BrnGui::SetMaskRect (the mask push helper)
#include "pc/gcm/renderengine/renderstates.h"           // renderengine::TextureState (+Parameters) / Texture
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasic2dColouredTexturedVertex.h" // vertex

#include <cstring> // memcpy

namespace BrnGui
{
namespace
{
// ---------------------------------------------------------------------------
// File-local data tables (X360 .rdata / cinit). ⭐ H3b (2026-08-25): every value
// below is now READ OFF THE IMAGE (scratch h3b_dump6/7.txt) -- the old "data
// segment not dumped" placeholder FLAG is retired.
// ---------------------------------------------------------------------------

// Per-texture-resolution atlas dimensions (index 0 = SD, 1 = HD). DWARF h:213-216.
const f32 KAF_TEXTURE_WIDTH[2]  = { 256.0f, 256.0f };   // flt_82054E10
const f32 KAF_TEXTURE_HEIGHT[2] = { 256.0f, 256.0f };   // flt_82054E18
const f32 KAF_ICON_WIDTH[2]     = {  64.0f,  64.0f };   // flt_82054E20
const f32 KAF_ICON_HEIGHT[2]    = {  64.0f,  64.0f };   // flt_82054E28
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
// ⭐ H3b values from their cinit thunks (0x82C51224..0x82C512F0): each lane is a
// stored ratio x a world-size scale (0.0125*3000, 0.044444446*2500, 0.025*3000,
// 0.022222223*2500). Both lanes of each pair are identical on the image.
const f32 KAF_ICON_HALFWIDTH_WORLDSIZE[SatNavRenderer::E_SATNAVICON_NUM]   = { 37.5f, 37.5f };             // flt_82FB36F0
const f32 KAF_ICON_HALFWIDTH_SCREENSPACE[SatNavRenderer::E_SATNAVICON_NUM] = { 111.11112f, 111.11112f };   // flt_82FB3708
const f32 KAF_ICON_HALFHEIGHT_SCREENSPACE[SatNavRenderer::E_SATNAVICON_NUM]= { 75.0f, 75.0f };             // flt_82FB3720
const f32 KAF_ICON_HALFHEIGHT_WORLDSIZE[SatNavRenderer::E_SATNAVICON_NUM]  = { 55.555557f, 55.555557f };   // flt_82FB3728

// Event-type-byte -> sat-nav mini-icon index map (dword_82054E48, 6 entries read off
// the image: {3,1,5,4,2,4} = RACE/ROADRAGE/STUNT/MARKEDMAN/BURNINGROUTE/MARKEDMAN per
// ESatNavEventTypeMiniIconIndex). ⭐ H3b: values recovered (h3b_dump6.txt); the old
// all-zero placeholder FLAG is retired. Entries past the 6 authored event types stay 0.
const u32 KAU_EVENTTYPE_TO_ICONROW[256] = { 3, 1, 5, 4, 2, 4 };

// maResourcesToLoad / muNumResourcesToLoad (DWARF h:147/154). The InitResources loop walks a
// 2-entry resource-tuple table (id + flags pairs, 8-byte stride) up to muNumResourcesToLoad.
// ⭐ H3b: tuple values read off the image @0x82F25708 = {204, 11}, {205, 11} -- the two
// sat-nav icon-atlas textures (ids 204/205, resource type 11); placeholder FLAG retired.
struct SatNavResourceTuple { u32 muResourceId; u32 muFlags; };
const SatNavResourceTuple KA_RESOURCES_TO_LOAD[2] = { { 204u, 11u }, { 205u, 11u } };
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

// PC fold of the X360's lazy texture-state creation in RenderComponent (@0x82466288 /
// @0x8246645C): descriptor {2,2,0,1,1,2,0,0,13,0,1, lod 0.0, flags 0,0,0,1,1} + the
// payload texture, through renderengine::TextureState::Initialize (font precedent).
renderengine::TextureState* CreatePayloadTextureState(rw::Resource* lpBacking, void* lpTexture)
{
    renderengine::TextureState::Parameters lParams;
    lParams.muAddressU      = 2u;
    lParams.muAddressV      = 2u;
    lParams.muAddressW      = 0u;
    lParams.muMagFilter     = 1u;
    lParams.muMinFilter     = 1u;
    lParams.muMipFilter     = 2u;
    lParams.muField6        = 0u;
    lParams.muField7        = 0u;
    lParams.muMaxAnisotropy = 13u;
    lParams.muField9        = 0u;
    lParams.muField10       = 1u;
    lParams.mfMipLodBias    = 0.0f;
    lParams.mfField12       = 0.0f;
    lParams.muField13       = 0u;
    lParams.muField14       = 0u;
    lParams.muField15       = 0u;
    lParams.mu8Field40      = 0u;
    lParams.mu8Field41      = 0u;
    lParams.mu8Field42      = 0u;
    lParams.mu8Field43      = 1u;
    lParams.mu8Field44      = 1u;
    lParams.mpTexture       = reinterpret_cast<renderengine::Texture*>(lpTexture);
    return renderengine::TextureState::Initialize(lpBacking, &lParams);
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
// 0xB6744D9D6FE80000. ⭐ H3b: was a `u32 GetComponentID()` shadow (not the base's GetID
// slot, and truncated); the real override returns the full CgsID.
CgsID SatNavRenderer::GetID() const
{
    return 0xB6744D9D6FE80000ull;
}

// 0x8245F828
// ⭐ H3b: the old local signature `Prepare(void*, void*, void*)` SHADOWED the base
// virtual (GuiEventQueueSmall*, IResourceAllocator*, IResourceAllocator*) -- the
// manager's dispatch would have hit the base default AND the old body latched the
// EVENT QUEUE as the heap allocator. Real override + the real a1[1] latch now.
bool SatNavRenderer::Prepare(CgsGui::GuiEventQueueSmall* lpEventQueue,
                             rw::IResourceAllocator* lpHeapAllocator,
                             rw::IResourceAllocator* lpTextureAllocator)
{
    (void)lpEventQueue;
    (void)lpTextureAllocator;

    switch (mePrepareStage)
    {
    case E_PREPARESTAGE_START:
        mpHeapAllocator = lpHeapAllocator;   // +0x94 (the X360 latches a1[1])
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

// 0x82449940 (⭐ H3b: base-signature override -- the old `const void*` shadowed it)
void SatNavRenderer::RecvEvent(const CgsModule::Event* lpEvent, s32 liEventType)
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
        // ⭐ stuntrace-UI wave 2026-08-27: the console reads miEventID at record +0x10, but on
        // this host GuiEventJunctionInfo derives from CgsGui::GuiEvent<311> whose 3-word header
        // pads to 16 under the leading CgsID's alignment -- +0x10 is mSpecialEventCarId (ZERO on
        // every stunt junction), and miEventID sits at +0x20. The word-index read handed the
        // car-id low dword to RefreshSatNavIconInfo, which then re-fired its lookup asserts
        // EVERY frame (the null guard early-returns before ++muNumberOfSatNavIcons, so the id
        // never caches). Typed by-name read, the same shape FBurnMainHudState's case-311 uses.
        RefreshSatNavIconInfo(
            reinterpret_cast<const BrnGui::GuiEventJunctionInfo*>(lpEvent)->miEventID);
        break;

    case KI_EVENT_SET_CACHE:
    {
        // ⭐ H3b BOOT-AV FIX: the payload carries a NATIVE-WIDTH cache pointer on this
        // host; the old word-0 read TRUNCATED it to 32 bits (a non-null garbage pointer
        // that sailed through the null check -- valid-pointer-invalid-object -- and AV'd
        // in EnsureResourcesAreLoaded at +0x26). Typed read, the ticker's case-64 shape.
        GuiCache* const* lppCache = reinterpret_cast<GuiCache* const*>(lpEvent);
        if (*lppCache == 0)
            FireSatNavAssert("lpCacheEvent->mpCachePointer", 430);
        mpGuiCache = *lppCache; // +0x90
        break;
    }

    case KI_EVENT_ENABLE_ICONS:
        // event[+8] (byte) -> +0x5A0 mbRenderEventStarts; event[0] -> +0x158 meIconDisplayType;
        // event[+4] -> +0x15C meGameModeFilter.
        mbRenderEventStarts = (reinterpret_cast<const u8*>(lpEvent)[8] != 0);
        meIconDisplayType =
            static_cast<GuiEventEnableSatNavIcons::EIconDisplayType>(lpuEventWords[0]);
        meGameModeFilter  = static_cast<s32>(lpuEventWords[1]);
        break;

    case KI_EVENT_RENDER_SATNAV:
        // The X360 memcpy's the 48-byte record; the record is native-width on this host
        // (three 8-byte texture pointers), so a fixed 48-byte copy would truncate it --
        // typed assignment instead (the PlayAptMovie width precedent).
        mRenderSatNavEvent =
            *reinterpret_cast<const GuiEventRenderSatNav*>(lpEvent); // +0x60
        // [DIAG] NOT IN THE X360 BINARY -- [satnav-diag] the first two 212 payloads.
        {
            static s32 siLeft = 2;
            if (siLeft > 0 && CgsDev::Log::gpDebugPrint != 0)
            {
                --siLeft;
                *CgsDev::Log::gpDebugPrint
                    << "[satnav-diag] renderer 212: mapnn=" << static_cast<s32>(mRenderSatNavEvent.mpMapTexture != 0)
                    << " masknn=" << static_cast<s32>(mRenderSatNavEvent.mpMaskTexture != 0)
                    << " pos=(" << mRenderSatNavEvent.mv3CarPosition.x
                    << "," << mRenderSatNavEvent.mv3CarPosition.z
                    << ") zoom=" << mRenderSatNavEvent.miZoomLevel << "\n";
            }
        }
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

        // [FLAG PC bring-up guard, 2026-08-26] The WDC's mpProgressionData binding is not yet
        // staged on this build, so GetEventInfoFromEventId can legitimately answer NULL here
        // (its own no-match contract). The console never sees that (the controller is READY
        // with data before icons draw). Default icon row + not-attempted; boot-proven AV in
        // the first live event start otherwise. DELETE-WHEN the WDC data binding lands.
        if (lpEventInfo == 0)
        {
            lpInfo->muEventTypeIndex = 0;
            lpInfo->meSatNavIconType = E_SATNAVICON_EVENT_NOTATTEMPTED;
            return;
        }

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
        // [FLAG PC bring-up guard, 2026-08-26] same unstaged-WDC-binding null as the offline
        // arm above; default row on null. DELETE-WHEN the WDC data binding lands.
        lpInfo->muEventTypeIndex = (lpEventInfo != 0)
            ? static_cast<u32>(lpEventInfo->GetIconFrameBase()) + 6
            : 6u;
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

        // Build the texture-state init parameters from the loaded texture and create the
        // icon texture state. ⭐ H3b: the old "reduced to a named pointer store" FLAG body
        // stored the raw texture AS the state -- the PC dispatch reads TextureState::
        // mpRaster, so that bound garbage. Real renderengine::TextureState::Initialize
        // now (the CgsFont::CreateTextureState precedent), with the X360's own sampler
        // words (the @0x82466304-style descriptor: clamp/clamp, linear, aniso 13).
        {
            renderengine::TextureState::Parameters lParams;
            lParams.muAddressU      = 2u;
            lParams.muAddressV      = 2u;
            lParams.muAddressW      = 0u;
            lParams.muMagFilter     = 1u;
            lParams.muMinFilter     = 1u;
            lParams.muMipFilter     = 2u;
            lParams.muField6        = 0u;
            lParams.muField7        = 0u;
            lParams.muMaxAnisotropy = 13u;
            lParams.muField9        = 0u;
            lParams.muField10       = 1u;
            lParams.mfMipLodBias    = 0.0f;
            lParams.mfField12       = 0.0f;
            lParams.muField13       = 0u;
            lParams.muField14       = 0u;
            lParams.muField15       = 0u;
            lParams.mu8Field40      = 0u;
            lParams.mu8Field41      = 0u;
            lParams.mu8Field42      = 0u;
            lParams.mu8Field43     = 1u;
            lParams.mu8Field44     = 1u;
            lParams.mpTexture       = reinterpret_cast<renderengine::Texture*>(
                                          const_cast<void*>(lpTexture));
            maIconResources[luTextureIndex][0] = 0;   // the console's 5-dword descriptor slot (documented shape)
            mapIconTextureStates[luTextureIndex] = renderengine::TextureState::Initialize(
                &maIconTextureStateBacking[luTextureIndex], &lParams);
        }

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

    // [FLAG PC bring-up guard, 2026-08-26; RESHAPED 2026-08-27 -- see below]
    // The console fires the asserts above and then does the append UNCONDITIONALLY (its WDC is
    // always bound + display info always resolves before this runs). Only the OUT-OF-BOUNDS case
    // is a real memory fault for us, so only it still returns.
    //
    // ⭐⭐ WHY THE OLD `lpDisplay == 0 || lpRaceEventData == 0` EARLY-RETURN WAS A DEFECT, NOT A
    // GUARD -- measured 2026-08-27. It skipped the SLOT CLAIM and the COUNT INCREMENT as well as
    // the two dereferences, and those two stores are what BOUND this function on the console.
    // Read straight off the 0x824458A0 asm tail, which has NO branch over any of it:
    //   0x824459B8  blt   loc_824459D8          <- the LAST branch; the 1308 assert only
    //   0x824459F4  stwx  r27, r9, r31          <- miEventId  = liEventId  (idx*32 + 0x5C0)
    //   0x824459F8  stw   r25, 0x5C8(r11)       <- meSatNavIconType = 0
    //   0x824459FC  lbz   r9, 0xEC(r29)         <- FIRST deref of lpRaceEventData
    //   0x82445A10  lvx128 v0, r0, r28          <- FIRST deref of lpEventStart
    //   0x82445A38..A40  lwz/addi/stw r11, 0x1870  <- ++muNumberOfSatNavIcons
    // i.e. the cache key is claimed and the count bumped OUTSIDE every assert and BEFORE either
    // pointer is touched. With the slot never claimed on our side, the cache
    // scan at the top of this function could never hit, so EVERY repost of the same event id
    // redid the whole lookup -- and the console's producer reposts action 201 -> GUI event 311
    // EVERY SIM TICK for as long as the player car sits inside a traffic-light trigger region
    // (GameStateModule::CheckIfPlayerIsAtJunctionWithAnEvent @0x82390418 has no "changed" gate;
    // its arrival arm is `IsPlayerInTrafficLightRegion() && (show || mbAtJunctionWithEvent)`).
    // So one unresolvable event id became an unbounded per-tick assert storm.
    //
    // ⭐ MEASURED BEFORE/AFTER, same recipe both sides (flow_run.ps1 -MaxSeconds 400 -Drive
    // -Teleport "3390.2,0.2,-1620.0,182", default one-at-a-time assert release):
    //     BEFORE  scratch/flow_run/satnav_BEFORE   3,243 fires PER SITE, 12,972 total
    //             (an exact reproduction of scratch/flow_run/anchor_verify_E)
    //     AFTER   scratch/flow_run/satnav_AFTER        1 fire  PER SITE,      4 total
    // Those four sites are the ENTIRE assert census of both runs. The producer is provably still
    // live in the AFTER run (same teleport, car parked in the same junction, the JunctionInfo HUD
    // sound fires the same 2 times) -- only the redundant re-lookup is gone.
    // ⚠️ The counts are release-throttled by the harness, so they measure "how much the release
    // loop drained", not the true tick rate; they are comparable only because both runs used the
    // same release mode. The BEFORE run had already banked 2,188 fires within ~8 s of the
    // handover, so the AFTER run's shorter drive does not explain the drop.
    // ⚠️ PIXEL-NEUTRAL, checked not assumed: RenderIconsForSatNav returns at !mbRenderEventStarts
    // on this build (ZERO fires of :1013 / :1015 / :1265 in either run), so nothing reads
    // maCachedSatNavIcons at all today. Confirmed on matched driving frames -- both minimaps draw
    // map + player arrow only, no icon appears.
    // Restoring the console's two stores makes it fire ONCE PER DISTINCT EVENT ID, which is what
    // the console would do if its lookups ever missed.
    // ⚠️ CAVEAT: past 150 distinct unresolvable ids in one drive the bounds guard below starts
    // returning again and the storm resumes. The console has the same cliff (it asserts 1292/1308
    // and writes out of bounds anyway); 150 junctions in one drive is far outside any run so far.
    //
    // ⛔ The asserts themselves are NOT touched. They are the console's own, at the console's
    // sites, on the console's conditions, and they are still reporting three real gaps:
    // GuiCache::maEventStarts is never populated (GameStateModule::SendSetUpAllEventStartsMessage
    // @0x823759D0 unreconstructed), WorldDataController::mpProgressionData is never bound, and
    // that controller parks at PREPARING_ACQUIRING_STREET_DATA so its meState gate (:374) fails.
    if (luIconIndex >= KU_MAX_SATNAV_ICONS)
    {
        return;
    }

    IconRendererSatNavIconInfo& lIcon = maCachedSatNavIcons[luIconIndex];
    lIcon.miEventId        = liEventId;                      // X360: stored before any deref
    lIcon.meSatNavIconType = E_SATNAVICON_EVENT_NOTATTEMPTED;

    // [FLAG PC bring-up guard] Only the two stores that DEREFERENCE the looked-up records stay
    // conditional. Leaving muEventTypeIndex at 0 alongside meSatNavIconType == 0 is not an
    // invented value: that pair IS this renderer's own encoding of an unfilled slot --
    // RenderIconsForSatNav tests exactly `muEventTypeIndex == 0 && meSatNavIconType == 0`
    // (X360 @0x8245FB40, two loads off +0x5C4/+0x5C8) and skips drawing such a slot.
    // DELETE-WHEN the WDC data binding + the event-start producer land.
    lIcon.muEventTypeIndex = (lpRaceEventData != 0)
        ? KAU_EVENTTYPE_TO_ICONROW[lpRaceEventData->GetEventTypeByte()]
        : 0u;
    if (lpDisplay != 0)
    {
        lIcon.mv3Position = lpDisplay->mv3Position;
    }
    else
    {
        lIcon.mv3Position.SetZero();   // deterministic; empty slot, never drawn in the icon loop
    }

    // The X360 then overrides row 2 -> 0 when this event is the cache's current online event (it
    // compares two far GuiCache id members at +19192/+19196). Those members are not yet recovered
    // on GuiCache, so the override is conservatively NOT applied (matches GetIconInformation). FLAG.

    ++muNumberOfSatNavIcons;
}

// 0x8245F978 -- draw the visible sat-nav icons (those inside the map viewport) plus the closest
// off-screen icon clamped to the viewport edge. Heavily VMX-vectorised on X360; the per-icon
// world->device transform + viewport clamp is reconstructed at the semantic level via MapTransform.
void SatNavRenderer::RenderIconsForSatNav(
    CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex>* lpRenderBuffer)
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

        // World -> device, then device -> screen-normalised, then MINIMAP-LOCAL. ⭐ H3b
        // (asm @0x8245FB94..0x8245FC7C, h3b_dump3.txt): the real chain is
        //   s = Transform(WorldToDevice(pos, false), DEVICE space, NORMALISED space)
        //   u = (s - viewRect.origin) / viewRect.size
        // The old transcription used (WorldSpace, DeviceSpace) AND dropped the view-rect
        // renormalisation entirely -- every gate below compares against the 0..1
        // minimap-local band, so icons would have been placed/clipped in the wrong space.
        const Vector4& lv4ViewRect = MapTransform::GetSatNavViewRect();
        const f32 lfViewW = lv4ViewRect.z - lv4ViewRect.x;
        const f32 lfViewH = lv4ViewRect.w - lv4ViewRect.y;
        const Vector2 lv2Device = MapTransform::WorldToDevice(lIcon.mv3Position, false);
        Vector2 lv2Transformed  = MapTransform::Transform(
            lv2Device, MapTransform::GetDeviceSpace(), MapTransform::GetNormalisedSpace());
        lv2Transformed.x = (lv2Transformed.x - lv4ViewRect.x) / lfViewW;   // the asm's vrefp(w) product
        lv2Transformed.y = (lv2Transformed.y - lv4ViewRect.y) / lfViewH;

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
    // Same corrected chain as the in-loop transform (see the H3b note above), with the
    // clamped WorldToDevice (@0x8245FE0C passes true).
    const Vector4& lv4ViewRect = MapTransform::GetSatNavViewRect();
    const Vector2 lv2Device = MapTransform::WorldToDevice(lClosest.mv3Position, true);
    Vector2 lv2Transformed  = MapTransform::Transform(
        lv2Device, MapTransform::GetDeviceSpace(), MapTransform::GetNormalisedSpace());
    lv2Transformed.x = (lv2Transformed.x - lv4ViewRect.x) / (lv4ViewRect.z - lv4ViewRect.x);
    lv2Transformed.y = (lv2Transformed.y - lv4ViewRect.y) / (lv4ViewRect.w - lv4ViewRect.y);

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

// The immediate-mode layer's default blend / texture state singletons (X360 pointer-valued
// globals dword_83010F20 / dword_83010F3C -- the ImRendererBase state library's standard
// blend + untextured texture states). ⭐ H3b PC fold: the command dispatch's
// IM_CMD_SET_STATE_BLEND case is value-independent (it installs the standard alpha-over
// state) and a null TextureState resets stage 0 to the no-texture path, so both
// singletons fold to null -- the calls stay (they are the state RESET brackets the
// console emits), the pointers carry no payload on this host.
const renderengine::BlendState* const   gpDefaultImBlendState   = 0; // dword_83010F20
const renderengine::TextureState* const gpDefaultImTextureState = 0; // dword_83010F3C

// 0x8245A3A0 -- build the four-vertex quad (top-left, bottom-left, top-right, bottom-right) for
// one sat-nav icon and submit it through the immediate-mode render buffer. The quad colour is the
// renderer's map-quad colour byte-swapped to the vertex RGBA packing; the four UVs come from the
// per-(icon-type-row, event-type-column) corner tables -- the full-icon tables for the online
// display modes, the mini-icon tables for the profile (display-type-0) mode.
void SatNavRenderer::RenderSatNavIcon(f32 lfX, f32 lfHalfWidth, f32 lfY, f32 lfHalfHeight,
                                      ESatNavIconType leIconType, u32 luEventTypeIndex,
                                      CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex>* lpRenderBuffer)
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

// 0x82465EC0 -- the per-frame sat-nav draw: install the zoomed view, build the map quad
// (unit square, per-corner map-texture UVs), push the circular mask, draw the quad and
// the icons. Decoded end-to-end from the X360 asm (scratch h3b_dump.txt); the VMX inline
// matrix work is reconstructed through the named MapTransform helpers (same math -- see
// each step's note).
void SatNavRenderer::RenderComponent(CgsGui::ImRendererSet* lpRendererSet)
{
    // The 2D command buffer (set slot 0 -- the console v6 = *a2, drawing through the
    // buffer subobject at +4; the [tut-ticker] precedent on this host).
    CgsGui::AptIm2dRenderBuffer* lpAptBuffer =
        *reinterpret_cast<CgsGui::AptIm2dRenderBuffer* const*>(lpRendererSet);
    if (lpAptBuffer == 0)
        return;
    CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex>& lrCmd =
        lpAptBuffer->mCommandBuffer;

    // Gate: both the map and the mask texture must have arrived in the render payload
    // (X360 lwz +0x7C / +0x80). Without them the console clears its output surface
    // (SetClear on the lazily-zeroed clear colour) inside an empty Begin/End bracket;
    // the PC dispatch draws straight to the back buffer, so the bracket alone is the
    // faithful no-op.
    if (mRenderSatNavEvent.mpMapTexture == 0 || mRenderSatNavEvent.mpMaskTexture == 0)
    {
        // [DIAG] NOT IN THE X360 BINARY -- [satnav-diag] the no-texture bracket.
        static bool sbLoggedEmpty = false;
        if (!sbLoggedEmpty && CgsDev::Log::gpDebugPrint != 0)
        {
            sbLoggedEmpty = true;
            *CgsDev::Log::gpDebugPrint
                << "[satnav-diag] RenderComponent EMPTY bracket: mapnn="
                << static_cast<s32>(mRenderSatNavEvent.mpMapTexture != 0) << " masknn="
                << static_cast<s32>(mRenderSatNavEvent.mpMaskTexture != 0) << "\n";
        }
        lrCmd.BeginRendering();
        lrCmd.EndRendering();
        return;
    }

    if (miSatNavRendererPM >= 0)
        CgsDev::PerfMonCpu::StartMonitor(miSatNavRendererPM);

    // The icon zoom scale: (clamp01(mph/120) + 1) * 700 (X360 @0x82465F30-F68,
    // flt_82056EDC == 1/120, flt_8205820C == 700).
    {
        f32 lfRatio = mRenderSatNavEvent.mfCarSpeedMph * 0.0083333338f;
        if (lfRatio < 0.0f) lfRatio = 0.0f;
        if (lfRatio > 1.0f) lfRatio = 1.0f;
        mfZoomLevel = (lfRatio + 1.0f) * 700.0f;
    }

    // The zoomed world window for this frame's payload (the component's own static rect
    // builder -- the renderer calls it with the PAYLOAD values, not the component's).
    Vector3 lav3Corners[4];
    SatNavComponent::GetZoomedCarWorldRect(
        lav3Corners,
        mRenderSatNavEvent.mv3CarPosition,
        mRenderSatNavEvent.mfCarSpeedMph,
        mRenderSatNavEvent.mfCarOrientation,
        mRenderSatNavEvent.mbRotateMap,
        mRenderSatNavEvent.mbUseTrajectory,
        mRenderSatNavEvent.miZoomLevel);

    // Corners flattened to the (x, z) map plane (the vperm ctrl 0x82CDA450 lane pick).
    Vector2 lv2C0; lv2C0.x = lav3Corners[0].x; lv2C0.y = lav3Corners[0].z; lv2C0.z = 0.0f; lv2C0.w = 0.0f;
    Vector2 lv2C1; lv2C1.x = lav3Corners[1].x; lv2C1.y = lav3Corners[1].z; lv2C1.z = 0.0f; lv2C1.w = 0.0f;
    Vector2 lv2C2; lv2C2.x = lav3Corners[2].x; lv2C2.y = lav3Corners[2].z; lv2C2.z = 0.0f; lv2C2.w = 0.0f;

    // Install the zoomed spaces (X360 passes corners[0], corners[2], corners[1]).
    MapTransform::SetZoomedWorldRect(lv2C0, lv2C2, lv2C1);
    MapTransform::SetZoomedViewportRect(MapTransform::GetSatNavViewRect());

    // Per-corner map-texture UVs: unit corner -> world (the corner coord space) ->
    // world-rect-unit (inverse world space). The X360 folds both into one inline
    // matrix (cornerSpace o inv(worldSpace), then Transform's own inverse); composing
    // the two named steps below is the same product applied in the same order.
    // ⭐ [satnav rotation 2026-08-25] CORNER ORDER FIXED (was (C0, C1, C2) -- the map
    // quad's UV space must share the icon space's axes: xAxis = corners[2]-corners[0]
    // (screen right), yAxis = corners[1]-corners[0] (screen down), exactly the
    // SetZoomedWorldRect call above. The old order transposed the map texture under
    // correctly-placed icons. Pinning derivation: BrnMapUtils.cpp SetZoomedWorldRect.
    const Matrix33 lm33Corners = MapTransform::MakeCoordSpaceFromPoints(lv2C0, lv2C2, lv2C1);
    Vector2 lav2Uv[4];
    {
        static const f32 KAF_UNIT[4][2] = { {0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 1.0f} };
        for (u32 lu = 0; lu < 4; ++lu)
        {
            Vector2 lv2Unit;
            lv2Unit.x = KAF_UNIT[lu][0];
            lv2Unit.y = KAF_UNIT[lu][1];
            lv2Unit.z = 0.0f;
            lv2Unit.w = 0.0f;
            const Vector2 lv2World = MapTransform::Transform(lv2Unit, lm33Corners);
            lav2Uv[lu] = MapTransform::Transform(
                lv2World, MapTransform::GetWorldSpace(), MapTransform::GetNormalisedSpace());
        }
    }

    // The map-quad colour: mMapQuadColour byte-reversed into the vertex RGBA8 (the same
    // swap RenderSatNavIcon documents; 0xE5FFFFFF -> translucent white).
    CgsGraphics::RGBA8 lColour;
    lColour.r = static_cast<u8>(mMapQuadColour);
    lColour.g = static_cast<u8>(mMapQuadColour >> 8);
    lColour.b = static_cast<u8>(mMapQuadColour >> 16);
    lColour.a = static_cast<u8>(mMapQuadColour >> 24);

    // The map quad: the unit square in the renderer's transform space (mTransform maps
    // it onto the on-screen minimap rect), one map-window UV per corner. Strip order
    // TL, BL, TR, BR (X360 vertex build @0x824661A4-0x82466250).
    CgsGraphics::Basic2dColouredTexturedVertex laVerts[4];
    static const f32 KAF_POS[4][2] = { {0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 1.0f} };
    for (u32 lu = 0; lu < 4; ++lu)
    {
        laVerts[lu].mv2Pos.x    = KAF_POS[lu][0];
        laVerts[lu].mv2Pos.y    = KAF_POS[lu][1];
        laVerts[lu].mv4Colour   = lColour;
        laVerts[lu].mv2Tex0UV.x = lav2Uv[lu].x;
        laVerts[lu].mv2Tex0UV.y = lav2Uv[lu].y;
    }

    // [DIAG] NOT IN THE X360 BINARY -- [satnav-diag] the live draw path (once).
    {
        static bool sbLoggedDraw = false;
        if (!sbLoggedDraw && CgsDev::Log::gpDebugPrint != 0)
        {
            sbLoggedDraw = true;
            *CgsDev::Log::gpDebugPrint
                << "[satnav-diag] RenderComponent DRAW: origin=(" << mTransform.mOriginXYZ.x
                << "," << mTransform.mOriginXYZ.y << ") rightup=(" << mTransform.mRightUp.x
                << "," << mTransform.mRightUp.w << ") uv0=(" << lav2Uv[0].x
                << "," << lav2Uv[0].y << ") uv3=(" << lav2Uv[3].x << "," << lav2Uv[3].y
                << ") colour=" << mMapQuadColour << "\n";
        }
    }

    // ---- the draw bracket (X360 @0x82466254-0x82466280) ----
    lrCmd.BeginRendering();
    lrCmd.SetState(gpDefaultImBlendState);
    lrCmd.SetState(gpDefaultImTextureState);
    lrCmd.SetTransform(mTransform);

    // Lazy map / mask texture states from the payload textures (X360 @0x82466288 /
    // @0x8246645C; the same real-TextureState fold as InitResources -- see its H3b note).
    if (mpMapTextureState == 0)
        mpMapTextureState = CreatePayloadTextureState(
            &mMapTextureStateBacking, mRenderSatNavEvent.mpMapTexture);
    lrCmd.SetState(mpMapTextureState);

    // The map BLEND state is created here on console (@0x824663A4) but never bound in
    // this pass (the default blend stays active) -- on the PC fold the dispatch's blend
    // case is value-independent, so the unused creation folds away entirely.

    if (mpMaskTextureState == 0)
        mpMaskTextureState = CreatePayloadTextureState(
            &mMaskTextureStateBacking, mRenderSatNavEvent.mpMaskTexture);

    // Push the minimap mask over the viewport rect, full mask-texture UV range
    // (X360 builds the {0,0,1,1} uv vector @0x82466524-0x8246654C, then SetMaskRect).
    {
        Vector4 lv4MaskUv;
        lv4MaskUv.x = 0.0f; lv4MaskUv.y = 0.0f; lv4MaskUv.z = 1.0f; lv4MaskUv.w = 1.0f;
        SetMaskRect(lrCmd, mpMaskTextureState, MapTransform::GetSatNavViewRect(), lv4MaskUv);
    }

    // The map quad (prim type 6 == triangle strip, 4 vertices), then the icons, then
    // pop the mask and restore the default states (X360 @0x82466554-0x82466598).
    lrCmd.Render(static_cast<renderengine::PrimitiveType>(6), laVerts, 4);
    RenderIconsForSatNav(&lrCmd);
    lrCmd.PopMask();
    lrCmd.SetState(gpDefaultImBlendState);
    lrCmd.SetState(gpDefaultImTextureState);
    lrCmd.EndRendering();

    if (miSatNavRendererPM >= 0)
        CgsDev::PerfMonCpu::StopMonitor(miSatNavRendererPM);
}

// 0x8245F4D8 -- rebuild the screen-space mTransform from the on-screen viewport
// rectangle, in place. ⭐ H3b: the viewport rect is RECOVERED now (the live
// MapTransform::GetSatNavViewRect() static @0x82FB36A0, HD default {0.778125,
// 0.66527778, 0.93125, 0.86666667}) -- the old "unrecovered input, normalised-device
// default" FLAG body is retired.
//
// The X360 builds the CONSOLE NDC block from the rect:
//   origin = (2*x0 - 1, 1 - 2*y0), right = (2*w, 0), up = (0, -2*h)
// then folds the display aspect ratio (TransformByAspectRatio) because its GPU consumed
// NDC. The PC dispatch walk (CgsImRenderBufferTemplate.cpp RENDER_PRIMITIVES) consumes
// batch transforms in the Apt SetVertexMatrix convention instead -- local -> LOGICAL
// 1280x720 SCREEN pixels, which the dispatch then scales to the back buffer itself --
// and its colour lanes ride the CXForm fold whose identity scale is 255 (both are the
// [tut-ticker] InGameMessageRenderer::RenderComponent precedent, including the boot
// that collapsed every vertex to a sub-pixel when the console NDC block was published
// verbatim). So the PC-correct block maps the renderer's unit-square drawing space to
// the viewport's logical-pixel rectangle:
//   origin = (x0*1280, y0*720), right = (w*1280, 0), up = (0, h*720), colourScale = 255.
void SatNavRenderer::UpdateRendererTransform()
{
    const Vector4& lv4Rect = MapTransform::GetSatNavViewRect();   // {x0, y0, x1, y1}
    const f32 lfWidth  = lv4Rect.z - lv4Rect.x;
    const f32 lfHeight = lv4Rect.w - lv4Rect.y;

    // The engine's fixed logical screen (the dispatch's KF_DISPATCH_LOGICAL_W/H).
    const f32 KF_LOGICAL_W = 1280.0f;
    const f32 KF_LOGICAL_H = 720.0f;

    mTransform.mOriginXYZ.SetZero();
    mTransform.mRightUp.SetZero();
    mTransform.mColourShift.SetZero();
    mTransform.mColourScale.SetZero();

    mTransform.mOriginXYZ.x = lv4Rect.x * KF_LOGICAL_W;
    mTransform.mOriginXYZ.y = lv4Rect.y * KF_LOGICAL_H;
    mTransform.mRightUp.x   = lfWidth  * KF_LOGICAL_W;   // right = (w_px, 0)
    mTransform.mRightUp.w   = lfHeight * KF_LOGICAL_H;   // up    = (0, h_px)

    // CXForm identity on the PC dispatch is 255 (the ticker's folded=01010101 lesson).
    mTransform.mColourScale.x = 255.0f;
    mTransform.mColourScale.y = 255.0f;
    mTransform.mColourScale.z = 255.0f;
    mTransform.mColourScale.w = 255.0f;

    // The console's trailing TransformByAspectRatio() folds the display aspect into the
    // NDC basis; the PC logical-pixel dispatch performs its own back-buffer scale, so
    // folding it here would double-apply (the ticker precedent publishes without it).
}

} // namespace BrnGui

// BrnCustomRenderer.cpp
// BrnGui::CustomRendererManager -- the GUI custom-renderer set manager.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (0x82444040 .. 0x82450908 + the ctor
// @0x827E20A8). The manager owns ten polymorphic render components (NetworkPlayerImage,
// SatNav, MainMap, CrashNavIcon, BoostBar, AboveCar, ProgressBar, BlackBar,
// InGameMessage, CreditsText), drives their lifecycle, routes GUI events to them, and
// serves the Apt player's custom-control texture substitution. The canonical class home
// is GameSource/Gui/BrnCustomRendererManager.h.
//
// ---- 2026-08-16 MOUNT PASS: what the link and the asm forced ----------------------
// This TU compiled cleanly for weeks while being wrong in four ways that only reading
// the asm alongside the DWARF exposes. All four are fixed here:
//
//  1. THE COMPONENT ARRAY WAS NEVER POPULATED. Construct() commented that the slots were
//     "populated elsewhere in the full game" -- nothing populates them, and it did not
//     even zero them. RecvEvent, GetComponentID, SetComponentRenderable and Update then
//     dereferenced them unguarded. Mounting the TU in that state was a guaranteed AV
//     through a stack-garbage vptr. Construct() now wires the array exactly as the ctor
//     @0x827E20A8 + Construct @0x82444040 do, and every dispatch null-guards (slots whose
//     renderer is not reconstructed stay null -- see the header).
//
//  2. Prepare() PASSED THE WRONG FIRST ARGUMENT. The guest is
//     `(*(**v6 + 4))(*v6, a1 + 3, a1[1], a1[2])` -- `a1 + 3` is dword index 3, i.e.
//     byte +12, which is the BASE's mEventQueue, not `this`. The component Prepare
//     signature is Prepare(GuiEventQueueSmall*, IResourceAllocator*, IResourceAllocator*)
//     (DWARF) and it was being handed the manager pointer.
//
//  3. THREE INVENTED CALLS. RecvEvent case 213 called `lpMainMap->Update()` where the
//     guest calls component vtable +0x2C and +0x30 -- StartFade(bool, f32) and
//     ClearFadeState(). SetTextRenderer and SetLanguageManager each called
//     `mapCustomRenderComponents[E_INGAME_MESSAGE]->Update()` where the guest calls
//     vtable +0x38 / +0x3C on the CREDITS TEXT subobject (this+114720 == dword 28680),
//     which is not even the same component, and those slots are past the end of the
//     shared interface. Update() is not a stand-in for any of them: it is a real,
//     different, per-frame method, so calling it was an invented side effect on a live
//     object. The fade pair is now dispatched properly; the two CreditsText pings are
//     documented, not faked.
//
//  4. Release() BROKE OUT OF THE LOOP ON THE FIRST INCOMPLETE COMPONENT and the comment
//     claimed the guest "continues WHILE Release() is truthy". Kept (the guest's staged
//     release does return early), but re-derived rather than assumed.
//
// Vtable offsets used below (component = CgsGui::CustomRenderComponentInterface):
//   +0x00 Construct  +0x04 Prepare  +0x08 Release  +0x0C Destruct  +0x10 GetRenderOutput
//   +0x14 RecvEvent  +0x18 Update   +0x1C SetRenderEnabled         +0x20 GetRenderLayer
//   +0x24 GetID      +0x28 GetNumTextures  +0x2C StartFade  +0x30 ClearFadeState
//   +0x34 RenderComponent

#include "GameSource/Gui/BrnCustomRendererManager.h"
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasic2dColouredTexturedVertex.h" // mask corner vertices (SetMaskRect)
#include "pc/gcm/renderengine/renderstates.h"   // renderengine::TextureState::mpRaster (SetMaskRect)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // [DIAG] the satnav-diag prints

#include "GameShared/GameClasses/Core/CgsAssert.h" // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h" // CgsDev::Log::gpDebugPrint (the [tut-ticker] diag)

// CgsGui::SetGuiCamera @0x82847658 -- selects the active GUI camera (0 = full-screen-map,
// 1 = normal HUD). It used to be re-declared locally here as an "uncommitted, out of scope"
// extern; that is precisely the shadowing-redeclaration shape that only a LINK finds, and
// the link found it. It has a real home -- GameShared/GameClasses/Gui/CgsGuiShared.cpp,
// named by its own baked assert string -- and it is included from there now.
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"

// SetMaskRect (bottom of this TU) writes the 2-vertex clip-mask corner run and pushes it
// through the Im2d command buffer (opcode 17).
#include "BrnCommonTypes.h"  // rw::math::vpu::Vector4
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBufferTemplate.h"

namespace BrnGui
{

namespace
{
    // X360 event-type ids routed by RecvEvent (transcribed from the jump tables). Named for
    // readability; the underlying values are the raw guest case labels.
    enum ERecvEventId
    {
        E_EVT_14  = 14,   E_EVT_64  = 64,
        E_EVT_145 = 145,  E_EVT_204 = 204,  E_EVT_206 = 206,
        E_EVT_209 = 209,  E_EVT_212 = 212,  E_EVT_213 = 213,  E_EVT_214 = 214,
        E_EVT_215 = 215,  E_EVT_221 = 221,  E_EVT_223 = 223,  E_EVT_225 = 225,
        E_EVT_258 = 258,  E_EVT_311 = 311,  E_EVT_343 = 343,  E_EVT_345 = 345,
        E_EVT_346 = 346,  E_EVT_355 = 355,  E_EVT_363 = 363,  E_EVT_365 = 365,
        E_EVT_377 = 377,  E_EVT_394 = 394,  E_EVT_415 = 415,  E_EVT_427 = 427,
        E_EVT_505 = 505,  E_EVT_534 = 534,  E_EVT_535 = 535,  E_EVT_536 = 536,
        E_EVT_537 = 537,  E_EVT_539 = 539,  E_EVT_554 = 554,  E_EVT_556 = 556,
        E_EVT_557 = 557,  E_EVT_558 = 558,  E_EVT_559 = 559,  E_EVT_560 = 560,
        E_EVT_561 = 561,  E_EVT_562 = 562,  E_EVT_571 = 571,  E_EVT_587 = 587,
        E_EVT_588 = 588
    };

    // Forward a received event to one component's RecvEvent vtable slot (+0x14).
    // ⛔ NULL-GUARDED. Slots 1..9 have no reconstructed renderer yet; on the console every
    // slot is a live by-value subobject, so the guest needs no check. Skipping a null slot
    // is the honest host behaviour -- the alternative (a stub component that swallows the
    // event and reports success) is the silent-drop shape this tree keeps getting bitten by.
    inline void RouteEvent(CgsGui::CustomRenderComponentInterface* lpComponent,
                           const CgsModule::Event* lpEvent, s32 liEventType)
    {
        if (lpComponent != 0)
            lpComponent->RecvEvent(lpEvent, liEventType);
    }

    inline void SetEnabled(CgsGui::CustomRenderComponentInterface* lpComponent, bool lbEnabled)
    {
        if (lpComponent != 0)
            lpComponent->SetRenderEnabled(lbEnabled);
    }
}

// ================= ctor @ 0x827E20A8 =================
// The guest constructs the by-value renderer subobjects in place and installs the
// remaining components' vtables. On the host the sole embedded component is a real member
// with a real ctor; the array itself is seeded by Construct() (as on console -- the ctor
// does not write +0x101C).
CustomRendererManager::CustomRendererManager()
    : mbRenderingEnable(false)
    , mbSatNavRenderable(false)
    , mbMainMapRenderable(false)
    , mbThirdSlotRenderable(false)
    , mbComponentRenderableGate(false)
    , mbHaveValidMapPosition(false)
    , mePrepareStage(E_PREPARESTAGE_START)
    , meReleaseStage(E_RELEASESTAGE_START)
    , miHACK_NumberOfRendersWithoutUpdate(0)
    , mpFlaptRenderer(0)
    , mpTextRenderer(0)
    , mpLanguageManager(0)
    , mpReplaySerialiser(0)
{
    for (s32 liIndex = 0; liIndex < E_CUSTOM_RENDER_TYPES_COUNT; ++liIndex)
        mapCustomRenderComponents[liIndex] = 0;
}

// ================= Construct @ 0x82444040 =================
// Chain the base construct, wire the ten component pointers to their subobjects, clear the
// master rendering-enable flag, construct every component through vtable[+0x00], construct
// the debug component, then zero the stage markers and the HACK counter.
void CustomRendererManager::Construct()
{
    CgsGui::CustomRendererManager::Construct();

    // Component-pointer array setup. The guest stores, in this exact order:
    //   +4124 (slot 0 NetPlayerImg) = this+112192   +4128 (slot 1 SatNav)     = this+4176
    //   +4132 (slot 2 MainMap)      = this+10448    +4136 (slot 3 CrashNav)   = this+45152
    //   +4140 (slot 4 BoostBar)     = this+58656    +4144 (slot 5 AboveCar)   = this+112944
    //   +4148 (slot 6 ProgressBar)  = this+114672   +4152 (slot 7 BlackBar)   = this+114696
    //   +4156 (slot 8 InGameMsg)    = this+123120   +4160 (slot 9 CreditsText)= this+114720
    // Slot 0 is the embedded renderer; the rest have no reconstructed component to point
    // at and stay NULL (see the header for why they are not stubbed).
    mapCustomRenderComponents[E_NETWORK_PLAYER_IMAGE] = &mNetworkPlayerImageRenderer;
    // ⭐ [H3b] slot 1 is LIVE (2026-08-25): the reconstructed SatNavRenderer subobject
    // (guest this+0x1050), the minimap.
    mapCustomRenderComponents[E_SATNAV]               = &mSatNavRenderer;
    mapCustomRenderComponents[E_MAINMAP]              = 0;
    mapCustomRenderComponents[E_CRASHNAVICONS]        = 0;
    // ⭐ [boost-bar] slot 4 is LIVE (2026-08-25): the reconstructed BoostBarRenderer
    // subobject (guest this+0xE520), the in-game boost gauge.
    mapCustomRenderComponents[E_BOOSTBAR]             = &mBoostBarRenderer;
    mapCustomRenderComponents[E_ABOVECAR]             = 0;
    mapCustomRenderComponents[E_PROGRESSBAR]          = 0;
    mapCustomRenderComponents[E_BLACKBAR]             = 0;
    // ⭐ [tut-ticker] slot 8 is LIVE (2026-08-24): the reconstructed InGameMessageRenderer
    // subobject (guest this+0x1E0F0), the bottom-of-screen ticker.
    mapCustomRenderComponents[E_INGAME_MESSAGE]       = &mInGameMessageRenderer;
    mapCustomRenderComponents[E_CREDITS_TEXT]         = 0;

    // guest `stbx 0 -> +0x1F498`
    mbRenderingEnable = false;

    // guest `do { (***v2)(*v2); } while` -- component vtable[+0x00].
    for (s32 liIndex = 0; liIndex < E_CUSTOM_RENDER_TYPES_COUNT; ++liIndex)
    {
        if (mapCustomRenderComponents[liIndex] != 0)
            mapCustomRenderComponents[liIndex]->Construct();
    }

    // guest BrnGui::GuiCustRendererDebugComponent::Construct(this + 0x1F49C, this).
    // FLAG: the debug component is a developer-menu leaf with no gameplay effect; it is
    // not embedded here (its Construct binds itself into the global debug-menu registry,
    // which this build does not stand up).

    // Guest +0x1F490 / +0x1F494 / +0x1F4CC stores of 0.
    mePrepareStage = E_PREPARESTAGE_START;
    meReleaseStage = E_RELEASESTAGE_START;
    miHACK_NumberOfRendersWithoutUpdate = 0;
}

// ================= Prepare @ 0x82444140 =================
// State-machine guarded preparation. On the START stage it Prepares every component; if
// they all succeed it disables everything then re-enables NetworkPlayerImage, BlackBar and
// InGameMessage, and advances to DONE.
bool CustomRendererManager::Prepare(rw::IResourceAllocator* lpHeapAllocator,
                                    rw::IResourceAllocator* lpTextureAllocator)
{
    CgsGui::CustomRendererManager::Prepare(lpHeapAllocator, lpTextureAllocator);

    if (mePrepareStage == E_PREPARESTAGE_START)
    {
        bool lbAllPrepared = true;
        mePrepareStage = E_PREPARESTAGE_START;   // guest re-stores 0 before the loop

        // guest `(*(**v6 + 4))(*v6, a1 + 3, a1[1], a1[2])`:
        //   a1 + 3  == dword index 3 == byte +12 == the BASE's mEventQueue
        //   a1[1]   == mpHeapAllocator      a1[2] == mpTextureAllocator
        // The loop does NOT stop at the first failure; it prepares all ten and ANDs.
        for (s32 liIndex = 0; liIndex < E_CUSTOM_RENDER_TYPES_COUNT; ++liIndex)
        {
            if (mapCustomRenderComponents[liIndex] != 0)
            {
                if (!mapCustomRenderComponents[liIndex]->Prepare(
                        GetOutputEventQueue(), lpHeapAllocator, lpTextureAllocator))
                {
                    lbAllPrepared = false;
                }
            }
        }

        if (lbAllPrepared)
        {
            // guest: (*this+52)(this,0); (*this+32)(this,0,1); (*this+32)(this,7,1);
            //        (*this+32)(this,8,1)
            //   +52 == 0x34 == SetAllRenderingState, +32 == 0x20 == SetComponentRenderable
            SetAllRenderingState(false);
            SetComponentRenderable(E_NETWORK_PLAYER_IMAGE, true);
            SetComponentRenderable(E_BLACKBAR,             true);
            SetComponentRenderable(E_INGAME_MESSAGE,       true);

            mePrepareStage = E_PREPARESTAGE_DONE;
            // guest CgsDev::DebugComponent::Register(this + 0x1F49C) -- debug menu only.
            return true;
        }
        return false;
    }

    if (mePrepareStage == E_PREPARESTAGE_DONE)
    {
        // Already prepared: re-register the debug component and report success.
        mePrepareStage = E_PREPARESTAGE_DONE;
        return true;
    }

    CGS_ASSERT(false, " unknown prepare stage in CustomRenderManager ");
    return false;
}

// ================= Release @ 0x824442B0 =================
bool CustomRendererManager::Release()
{
    if (meReleaseStage == E_RELEASESTAGE_START)
    {
        meReleaseStage = E_RELEASESTAGE_START;
        for (s32 liIndex = 0; liIndex < E_CUSTOM_RENDER_TYPES_COUNT; ++liIndex)
        {
            if (mapCustomRenderComponents[liIndex] != 0)
            {
                // The staged release returns not-done at the first component that has not
                // finished; the next call resumes from the same stage.
                if (!mapCustomRenderComponents[liIndex]->Release())
                    return false;
            }
        }
        meReleaseStage = E_RELEASESTAGE_DONE;
        return true;
    }

    if (meReleaseStage == E_RELEASESTAGE_DONE)
    {
        meReleaseStage = E_RELEASESTAGE_DONE;
        return true;
    }

    CGS_ASSERT(false, " unknown release stage in CustomRenderManager ");
    return false;
}

// ================= Destruct @ 0x82444378 =================
void CustomRendererManager::Destruct()
{
    // guest BrnGui::GuiCustRendererDebugComponent::Destruct(this + 0x1F49C).

    for (s32 liIndex = 0; liIndex < E_CUSTOM_RENDER_TYPES_COUNT; ++liIndex)
    {
        if (mapCustomRenderComponents[liIndex] != 0)
            mapCustomRenderComponents[liIndex]->Destruct();
    }

    CgsGui::CustomRendererManager::Destruct();
}

// ================= RecvEvent @ 0x824443D0 =================
// Routes a module event to the component(s) interested in its type id. Transcribed from
// the X360 jump tables; the v4[dwordIndex] targets map to named components:
//   v4[1044]=SatNav  v4[2612]=MainMap  v4[11288]=CrashNavIcon  v4[14664]=BoostBar
//   v4[28048]=NetworkPlayerImage  v4[28236]=AboveCar  v4[28668]=ProgressBar
//   v4[28674]=BlackBar  v4[28680]=CreditsText  v4[30780]=InGameMessage
// and v4[1032..1036] are the component-array slots SatNav..AboveCar (events 213/214/215).
void CustomRendererManager::RecvEvent(const CgsModule::Event* lpEvent, s32 liEventType)
{
    CGS_ASSERT(lpEvent != 0, " null event passed ");

    CgsGui::CustomRenderComponentInterface* lpSatNav        = mapCustomRenderComponents[E_SATNAV];
    CgsGui::CustomRenderComponentInterface* lpMainMap       = mapCustomRenderComponents[E_MAINMAP];
    CgsGui::CustomRenderComponentInterface* lpCrashNav      = mapCustomRenderComponents[E_CRASHNAVICONS];
    CgsGui::CustomRenderComponentInterface* lpBoostBar      = mapCustomRenderComponents[E_BOOSTBAR];
    CgsGui::CustomRenderComponentInterface* lpAboveCar      = mapCustomRenderComponents[E_ABOVECAR];
    CgsGui::CustomRenderComponentInterface* lpProgressBar   = mapCustomRenderComponents[E_PROGRESSBAR];
    CgsGui::CustomRenderComponentInterface* lpBlackBar      = mapCustomRenderComponents[E_BLACKBAR];
    CgsGui::CustomRenderComponentInterface* lpInGameMessage = mapCustomRenderComponents[E_INGAME_MESSAGE];
    CgsGui::CustomRenderComponentInterface* lpCreditsText   = mapCustomRenderComponents[E_CREDITS_TEXT];
    CgsGui::CustomRenderComponentInterface* lpNetPlayerImg  = mapCustomRenderComponents[E_NETWORK_PLAYER_IMAGE];

    switch (liEventType)
    {
        // ---- multi-component fan-outs ----
        case E_EVT_14:  // AboveCar, MainMap, CrashNav, SatNav, CreditsText, InGameMessage
            RouteEvent(lpAboveCar,      lpEvent, liEventType);
            RouteEvent(lpMainMap,       lpEvent, liEventType);
            RouteEvent(lpCrashNav,      lpEvent, liEventType);
            RouteEvent(lpSatNav,        lpEvent, liEventType);
            RouteEvent(lpCreditsText,   lpEvent, liEventType);
            RouteEvent(lpInGameMessage, lpEvent, liEventType);
            break;

        case E_EVT_64:  // the GuiCache bind: SatNav, MainMap, CrashNav, BoostBar, AboveCar,
                        // NetPlayerImg, then InGameMessage (LABEL_21)
            RouteEvent(lpSatNav,        lpEvent, liEventType);
            RouteEvent(lpMainMap,       lpEvent, liEventType);
            RouteEvent(lpCrashNav,      lpEvent, liEventType);
            RouteEvent(lpBoostBar,      lpEvent, liEventType);
            RouteEvent(lpAboveCar,      lpEvent, liEventType);
            RouteEvent(lpNetPlayerImg,  lpEvent, liEventType);
            RouteEvent(lpInGameMessage, lpEvent, liEventType);
            break;

        // ---- LABEL_35: BlackBar(+InGameMessage). 145/345/346/355/505/534-537 ----
        // ⭐ 537 is the TUTORIAL TICKER event (GameBridgeGameStateToX case 148 ->
        //    GuiEventTickerCustomMessage -> here -> InGameMessageRenderer).
        case E_EVT_145:
        case E_EVT_345:
        case E_EVT_346:
        case E_EVT_355:
        case E_EVT_505:
        case E_EVT_534:
        case E_EVT_535:
        case E_EVT_536:
        case E_EVT_537:
            // [DIAG] NOT IN THE X360 BINARY -- the [tut-ticker] RECEPTION rung: proves the
            // tutorial-ticker event crossed the whole producer->bridge->GUI chain and reached
            // this dispatch (event payload +0x810 is the string count). First-N latched.
            {
                static s32 siTickerDiagLeft = 8;
                if (liEventType == E_EVT_537 &&
                    siTickerDiagLeft > 0 && CgsDev::Log::gpDebugPrint != 0)
                {
                    --siTickerDiagLeft;
                    *CgsDev::Log::gpDebugPrint
                        << "[tut-ticker] RecvEvent 537 reached CustomRendererManager"
                        << " (blackBar=" << (lpBlackBar != 0)
                        << " inGameMsg=" << (lpInGameMessage != 0) << ")\n";
                }
            }
            RouteEvent(lpBlackBar,      lpEvent, liEventType);   // v4[28674]
            RouteEvent(lpInGameMessage, lpEvent, liEventType);   // v4[30780]
            break;

        // ---- LABEL_37: SatNav. 204/212/311/415/556 ----
        case E_EVT_204:
        case E_EVT_212:
        case E_EVT_311:
        case E_EVT_415:
        case E_EVT_556:
            RouteEvent(lpSatNav,        lpEvent, liEventType);   // v4[1044]
            break;

        // ---- LABEL_23: BoostBar. 206/363/365 ----
        case E_EVT_206:
        case E_EVT_363:
        case E_EVT_365:
            RouteEvent(lpBoostBar,      lpEvent, liEventType);   // v4[14664]
            break;

        // ---- LABEL_34: AboveCar. 209/394/427 ----
        case E_EVT_209:
        case E_EVT_394:
        case E_EVT_427:
            RouteEvent(lpAboveCar,      lpEvent, liEventType);   // v4[28236]
            break;

        // ---- 377: BoostBar then AboveCar (LABEL_34) ----
        case E_EVT_377:
            RouteEvent(lpBoostBar,      lpEvent, liEventType);
            RouteEvent(lpAboveCar,      lpEvent, liEventType);
            break;

        // ---- 223: MainMap then CrashNav (LABEL_28) ----
        case E_EVT_223:
            RouteEvent(lpMainMap,       lpEvent, liEventType);   // v4[2612]
            RouteEvent(lpCrashNav,      lpEvent, liEventType);   // v4[11288]
            break;

        // ---- 221: BlackBar only ----
        case E_EVT_221:
            RouteEvent(lpBlackBar,      lpEvent, liEventType);   // v4[28674]
            break;

        // ---- 225: ProgressBar ----
        case E_EVT_225:
            RouteEvent(lpProgressBar,   lpEvent, liEventType);   // v4[28668]
            break;

        // ---- LABEL_32: NetPlayerImg. 258/571 ----
        // ⭐ 258 delivers a transmitted player image; 571 forces the DEFAULT texture
        //    (mbUseDefaultTexture = true) -- the licence card's silhouette fallback.
        case E_EVT_258:
        case E_EVT_571:
            RouteEvent(lpNetPlayerImg,  lpEvent, liEventType);   // v4[28048]
            break;

        // ---- LABEL_21: InGameMessage. 343/539 ----
        case E_EVT_343:
        case E_EVT_539:
            RouteEvent(lpInGameMessage, lpEvent, liEventType);   // v4[30780]
            break;

        // ---- LABEL_28: CrashNav. 554/558/559/560/561/562 ----
        case E_EVT_554:
        case E_EVT_558:
        case E_EVT_559:
        case E_EVT_560:
        case E_EVT_561:
        case E_EVT_562:
            RouteEvent(lpCrashNav,      lpEvent, liEventType);   // v4[11288]
            break;

        // ---- 557: CrashNav then SatNav (LABEL_37) ----
        case E_EVT_557:
            RouteEvent(lpCrashNav,      lpEvent, liEventType);
            RouteEvent(lpSatNav,        lpEvent, liEventType);
            break;

        // ---- 587: enable CreditsText (vtable[+0x1C]) then route the event to it ----
        case E_EVT_587:
            SetEnabled(lpCreditsText, true);                     // v4 + 28680, slot +0x1C
            RouteEvent(lpCreditsText,   lpEvent, liEventType);   // v4 + 28680, slot +0x14
            break;

        // ---- 588: disable CreditsText (LABEL_40) ----
        case E_EVT_588:
            SetEnabled(lpCreditsText, false);                    // v4 + 28680
            break;

        // ---- payload-driven SetRenderEnabled on a component pointer slot ----
        // guest LABEL_40: `v12 = *a2;  (*(*v11 + 28))(v11, v12);` -- a2 is `u8*`, so the
        // enable comes from payload byte [0] (NOT [8]; byte 8 is the event-213 layout).
        case E_EVT_214:  // v4[1035] == byte +4140 == component slot 4 (BoostBar)
            SetEnabled(lpBoostBar, reinterpret_cast<const u8*>(lpEvent)[0] != 0);
            break;

        case E_EVT_215:  // v4[1036] == byte +4144 == component slot 5 (AboveCar)
            SetEnabled(lpAboveCar, reinterpret_cast<const u8*>(lpEvent)[0] != 0);
            break;

        case E_EVT_213:
            RecvEvent_Event213(lpEvent);
            break;

        default:
            break;
    }
}

// ---- RecvEvent case 213: the SatNav/MainMap/CrashNav map toggle. -------------------
// Asm @0x8244482C..0x824448C8. Payload layout: byte[0] selects sub-mode (0 = MainMap
// show/hide, 1 = full-map toggle), float at +4 is the fade duration, byte[8] is the
// enable flag.
void CustomRendererManager::RecvEvent_Event213(const CgsModule::Event* lpEvent)
{
    const u8*  lpu8  = reinterpret_cast<const u8*>(lpEvent);
    const f32* lpf32 = reinterpret_cast<const f32*>(lpEvent);

    CgsGui::CustomRenderComponentInterface* lpSatNav   = mapCustomRenderComponents[E_SATNAV];
    CgsGui::CustomRenderComponentInterface* lpMainMap  = mapCustomRenderComponents[E_MAINMAP];
    CgsGui::CustomRenderComponentInterface* lpCrashNav = mapCustomRenderComponents[E_CRASHNAVICONS];

    const bool lbEnableFlag  = (lpu8[8] != 0);
    const f32  lfFadeSeconds = lpf32[1];        // `lfs f1, 4(r30)`

    // [DIAG] NOT IN THE X360 BINARY -- [satnav-diag] the map-toggle records as the
    // manager sees them (first 12), plus the resulting SatNav enable.
    {
        static s32 siLeft = 12;
        if (siLeft > 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            --siLeft;
            *CgsDev::Log::gpDebugPrint
                << "[satnav-diag] mgr 213: submode=" << static_cast<s32>(lpu8[0])
                << " flag=" << static_cast<s32>(lpu8[8])
                << " fade=" << lfFadeSeconds
                << " satnavEnabledBefore="
                << static_cast<s32>((lpSatNav != 0) ? lpSatNav->GetRenderEnabled() : -1)
                << "\n";
        }
    }

    if (lpu8[0] != 0)
    {
        // Sub-mode 1: full-map toggle on the SatNav slot (guest v4[1032] == array slot 1),
        // mirror its render flag, then pick the GUI camera (0 = full-screen map).
        if (lpu8[0] == 1)
        {
            SetEnabled(lpSatNav, lbEnableFlag);
            mbSatNavRenderable = (lpSatNav != 0) && lpSatNav->GetRenderEnabled();
            CgsGui::SetGuiCamera(lbEnableFlag ? 0 : 1);
        }
    }
    else
    {
        // Sub-mode 0: MainMap show/hide (guest v4[1033] == array slot 2), gated on the
        // fade duration against flt_82001CC0 (READ FROM THE IMAGE: 0.0f).
        bool lbEnable;
        if (lfFadeSeconds > 0.0f)
        {
            // Animated: StartFade (component vtable +0x2C). The asm loads f1 from
            // payload+4 BEFORE the compare and never reloads it, so the duration in f1 IS
            // the second argument; r4 carries the fade-in bool. Enabling ALSO forces the
            // MainMap on straight away (vtable +0x1C); disabling leaves it to the fade.
            if (lpMainMap != 0)
                lpMainMap->StartFade(lbEnableFlag, lfFadeSeconds);

            if (lbEnableFlag)
            {
                SetEnabled(lpMainMap, true);
                lbEnable = true;
            }
            else
            {
                lbEnable = false;
            }
        }
        else
        {
            // Immediate: ClearFadeState (component vtable +0x30), then apply the flag.
            if (lpMainMap != 0)
                lpMainMap->ClearFadeState();
            SetEnabled(lpMainMap, lbEnableFlag);
            lbEnable = lbEnableFlag;
        }

        // Apply the resolved enable to the CrashNavIcon slot (guest v4[1034] == slot 3)
        // and cache the MainMap flag.
        SetEnabled(lpCrashNav, lbEnable);
        mbMainMapRenderable = (lpMainMap != 0) && lpMainMap->GetRenderEnabled();
    }
}

// ================= Update @ 0x82450908 =================
// Clear the event queue, apply the per-component renderable cache (gated on the +0x1F4B1
// flag and ANDed with the +0x1F4AF gate), update every component, then reset the HACK
// counter.
void CustomRendererManager::Update()
{
    mEventQueue.Clear();

    if (mbHaveValidMapPosition)   // guest `if (*(a1 + 128177))` == +0x1F4B1
    {
        // guest: v2 = a1 + 128175 (+0x1F4AF, mbComponentRenderableGate); each component's
        // enable is `*v2 && <per-component flag>`.
        const bool lbGate = mbComponentRenderableGate;

        SetEnabled(mapCustomRenderComponents[E_SATNAV],   lbGate && mbSatNavRenderable);   // a1+4128
        SetEnabled(mapCustomRenderComponents[E_MAINMAP],  lbGate && mbMainMapRenderable);  // a1+4132
        SetEnabled(mapCustomRenderComponents[E_ABOVECAR], lbGate && mbThirdSlotRenderable);// a1+4144
    }

    // guest `(*(**v6 + 24))(*v6)` == vtable +0x18 == Update.
    for (s32 liIndex = 0; liIndex < E_CUSTOM_RENDER_TYPES_COUNT; ++liIndex)
    {
        if (mapCustomRenderComponents[liIndex] != 0)
            mapCustomRenderComponents[liIndex]->Update();
    }

    miHACK_NumberOfRendersWithoutUpdate = 0;
}

// ================= Render @ 0x82450848 =================
// Draw every enabled component whose render layer matches, mirror the MainMap enable flag,
// then bump the "rendered without an update" counter -- and if the manager has rendered
// more than ten times without an Update, drop the stale event queue.
void CustomRendererManager::Render(CgsGui::ImRendererSet* lpRendererSet,
                                   CgsGui::eCustomRenderLayer leLayer)
{
    for (s32 liIndex = 0; liIndex < E_CUSTOM_RENDER_TYPES_COUNT; ++liIndex)
    {
        CgsGui::CustomRenderComponentInterface* lpComponent = mapCustomRenderComponents[liIndex];
        if (lpComponent != 0 &&
            lpComponent->GetRenderEnabled() &&      // guest `*(*v6 + 4)`
            lpComponent->GetRenderLayer() == leLayer)  // guest `(*(**v6 + 32))(*v6) == a3`
        {
            lpComponent->Render(lpRendererSet);     // the NON-virtual base Render
        }
    }

    // guest `*(a1 + 128174) = *(*(a1 + 4132) + 4)` -- mirror slot 2's enable flag.
    CgsGui::CustomRenderComponentInterface* lpMainMap = mapCustomRenderComponents[E_MAINMAP];
    mbMainMapRenderable = (lpMainMap != 0) && lpMainMap->GetRenderEnabled();

    if (++miHACK_NumberOfRendersWithoutUpdate > 10)
        mEventQueue.Clear();
}

// ================= GetComponentID @ 0x82445378 =================
CgsID CustomRendererManager::GetComponentID(s32 liComponent) const
{
    CGS_ASSERT(liComponent >= 0 && liComponent < E_CUSTOM_RENDER_TYPES_COUNT,
               "liComponent>=0 && liComponent<E_CUSTOM_RENDER_TYPES_COUNT");
    if (liComponent < 0 || liComponent >= E_CUSTOM_RENDER_TYPES_COUNT)
        return 0;

    const CgsGui::CustomRenderComponentInterface* lpComponent =
        mapCustomRenderComponents[liComponent];
    return lpComponent ? lpComponent->GetID() : 0;
}

// ================= SetComponentRenderable @ 0x824453F8 =================
void CustomRendererManager::SetComponentRenderable(s32 liComponent, bool lbRenderable)
{
    CGS_ASSERT(liComponent >= 0 && liComponent < E_CUSTOM_RENDER_TYPES_COUNT,
               "liComponent>=0 && liComponent<E_CUSTOM_RENDER_TYPES_COUNT");
    if (liComponent < 0 || liComponent >= E_CUSTOM_RENDER_TYPES_COUNT)
        return;

    SetEnabled(mapCustomRenderComponents[liComponent], lbRenderable);
}

// ================= GetComponentRenderable @ 0x82445468 =================
// Reads the component's render-enabled flag directly (guest `lbz r3, 4(r11)`).
bool CustomRendererManager::GetComponentRenderable(s32 liComponent)
{
    CGS_ASSERT(liComponent >= 0 && liComponent < E_CUSTOM_RENDER_TYPES_COUNT,
               "liComponent>=0 && liComponent<E_CUSTOM_RENDER_TYPES_COUNT");
    if (liComponent < 0 || liComponent >= E_CUSTOM_RENDER_TYPES_COUNT)
        return false;

    const CgsGui::CustomRenderComponentInterface* lpComponent =
        mapCustomRenderComponents[liComponent];
    return lpComponent ? lpComponent->GetRenderEnabled() : false;
}

// ================= GetNumTexturesForComponent @ 0x82445658 =================
s32 CustomRendererManager::GetNumTexturesForComponent(s32 liComponent) const
{
    CGS_ASSERT(liComponent >= 0, "liComponent >= 0");
    CGS_ASSERT(liComponent < E_CUSTOM_RENDER_TYPES_COUNT,
               "liComponent < E_CUSTOM_RENDER_TYPES_COUNT");
    if (liComponent < 0 || liComponent >= E_CUSTOM_RENDER_TYPES_COUNT)
        return 0;

    const CgsGui::CustomRenderComponentInterface* lpComponent =
        mapCustomRenderComponents[liComponent];
    return lpComponent ? lpComponent->GetNumTextures() : 0;
}

// ================= SetAllRenderingState @ 0x824454E0 =================
void CustomRendererManager::SetAllRenderingState(bool lbRenderable)
{
    for (s32 liIndex = 0; liIndex < E_CUSTOM_RENDER_TYPES_COUNT; ++liIndex)
        SetComponentRenderable(liIndex, lbRenderable);

    mbRenderingEnable = lbRenderable;
}

// ================= GetComponentTexture @ 0x824452B0 =================
// ⭐ THE APT CUSTOM-CONTROL ENTRY POINT. CgsGui::AptCallbackCustom::ControlRender
// @0x8285BFA0 calls this with CgsIDCompress(szType) after parsing "_index=" out of the
// custom control's property string. Find the component whose CgsID matches; if it is
// currently renderable, hand back its texture for that index and let it choose the shader
// program through the out-parameter. Returns null on no match or a disabled component --
// and a null return makes ControlRender skip the draw entirely, which is exactly the
// state the licence card is in today.
renderengine::Texture* CustomRendererManager::GetComponentTexture(
    CgsID lComponentID, s32 liTextureIndex, s32* lpiShaderProgram,
    CgsGui::ImRendererSet* lpRendererSet)
{
    CGS_ASSERT(lpiShaderProgram != 0, "lpiShaderProgram != NULL");
    if (lpiShaderProgram == 0)
        return 0;

    for (s32 liIndex = 0; liIndex < E_CUSTOM_RENDER_TYPES_COUNT; ++liIndex)
    {
        CgsGui::CustomRenderComponentInterface* lpComponent = mapCustomRenderComponents[liIndex];
        if (lpComponent == 0)
            continue;

        // guest `while ((*(**v11 + 36))(*v11) != a2)` -- component vtable +0x24 == GetID.
        if (lpComponent->GetID() != lComponentID)
            continue;

        // guest `if (!*(*(v13 + a1) + 4)) return 0;` -- mbRenderEnabled.
        if (!lpComponent->GetRenderEnabled())
            return 0;

        // guest `(*(**(v13 + a1) + 16))(*(v13 + a1), a3, a4, a5)` -- +0x10 GetRenderOutput.
        return lpComponent->GetRenderOutput(liTextureIndex, lpiShaderProgram, lpRendererSet);
    }
    return 0;
}

// ================= EndOfFrame @ 0x82449930 =================
// `return BrnGui::NetworkPlayerImageRenderer::SwapBuffers(a1 + 112192);` -- a direct,
// non-virtual tail-call into the embedded NetworkPlayerImage subobject. Now that the
// subobject is real, this is the real call (it used to be a comment describing a call the
// TU could not make).
void CustomRendererManager::EndOfFrame()
{
    mNetworkPlayerImageRenderer.SwapBuffers();
}

// ================= SetFlaptRenderer @ 0x82449920 =================
// guest: `*(result + 112924) = a2; return result;`  (112924 == 0x1B91C) -- ONE store, and
// that is all.
// ⚠️ I nearly propagated this into the embedded NetworkPlayerImageRenderer's own
// mpFlaptRenderer "because it obviously needs one". It does not come from here: SwapBuffers
// @0x82445E58 reads the renderer's copy at `a1[183]` == its +0x2DC, i.e. absolute
// 0x1B660 + 0x2DC = 0x1B93C -- a DIFFERENT field from the 0x1B91C this writes. The
// renderer's copy is installed by its own NetworkPlayerImageRenderer::SetFlaptRenderer
// from another caller. Adding the hand-down would have been an invented side effect that
// happened to look sensible.
// ⇒ CONSEQUENCE, recorded rather than papered over: EndOfFrame() -> SwapBuffers() asserts
//   "mpFlaptRenderer" if that other caller has not run. Nothing in this build calls
//   EndOfFrame yet; whoever wires it must wire the renderer's Flapt setter first.
CustomRendererManager* CustomRendererManager::SetFlaptRenderer(BrnFlapt::FlaptRenderer* lpFlaptRenderer)
{
    mpFlaptRenderer = lpFlaptRenderer;   // guest stwx -> this+0x1B91C
    return this;
}

// ================= SetTextRenderer @ 0x82445538 =================
// guest:
//   a1[28593] = a2;                        ; +0x1BEC4  mpTextRenderer
//   (*(a1[28680] + 56))(a1 + 28680);       ; CREDITS TEXT subobject (+114720), vtable +0x38
//   a1[14372] = a2;                        ; +0xE090   (inside the CrashNavIcon subobject)
//   a1[32029] = a2;                        ; +0x1F474
// ⚠️ The +0x38 slot is PAST the end of CustomRenderComponentInterface (which ends at
// +0x34), i.e. it is a CreditsTextRenderer-specific virtual, and the two mirrored stores
// land inside renderer subobjects this build does not embed. All three are therefore
// documented rather than emulated. (The previous version of this function called
// `mapCustomRenderComponents[E_INGAME_MESSAGE]->Update()` here -- the wrong component AND
// a real, unrelated per-frame method. Deleted, not re-stubbed.)
void CustomRendererManager::SetTextRenderer(CgsGraphics::TextRenderer* lpTextRenderer)
{
    mpTextRenderer = lpTextRenderer;

    // ⭐ [tut-ticker] the guest's `a1[32029] = a2` -- +0x1F474 IS the embedded
    // InGameMessageRenderer's mpTextRenderer (renderer base 0x1E0F0 + its +4996), so the
    // "mirrored store into a renderer subobject this build does not embed" note above is
    // paid for this one: the subobject is embedded now and the store is the real member.
    mInGameMessageRenderer.SetTextRenderer(lpTextRenderer);
}

// ================= SetLanguageManager @ 0x824455A8 =================
// guest:
//   a1[28594] = a2;                        ; +0x1BEC8  mpLanguageManager
//   (*(a1[28680] + 60))(a1 + 28680);       ; CREDITS TEXT subobject, vtable +0x3C
//   assert(a2);
//   a1[11308] = a2;                        ; +0xB0B0   (inside the CrashNavIcon subobject)
//   return BrnGui::InGameMessageRenderer::SetLanguageManager(a1 + 30780, a2);
// The tail call targets the INGAME MESSAGE subobject (+123120) -- a non-virtual member on
// a renderer this build does not embed yet. Same treatment as SetTextRenderer: the
// manager-level store is authoritative here, the rest is named, not faked.
void CustomRendererManager::SetLanguageManager(CgsLanguage::LanguageManager* lpLanguageManager)
{
    mpLanguageManager = lpLanguageManager;

    CGS_ASSERT(lpLanguageManager != 0, "lpLanguageManager");

    // ⭐ [tut-ticker] the guest's tail call -- `return InGameMessageRenderer::
    // SetLanguageManager(a1 + 30780, a2)` targets the slot-8 subobject (+123120), embedded
    // and real as of 2026-08-24.
    mInGameMessageRenderer.SetLanguageManager(lpLanguageManager);
}

// ================= SetReplaySerialiser @ 0x82445648 =================
// Defined in the sibling TU GameSource/Gui/BrnCustomRendererManager.cpp (the original
// minimal-slice ledger function); not redefined here.

// ⭐ [H3b x boost-bar reconcile 2026-08-25] TWO SetMaskRect bodies follow -- a REAL X360
// overload pair: @0x82450BE0 (pointer buffer; the boost-bar callers) and @0x82450D28
// (reference buffer; the sat-nav caller). Distinct functions in the image, both named
// BrnGui::SetMaskRect, both homed here.
// ================= BrnGui::SetMaskRect @ 0x82450BE0 =================
// The shared GUI clip-mask helper (this TU is its home -- the assert below bakes
// BrnCustomRenderer.cpp:889). The X360 body:
//   1. asserts the "Mask Aspect Correction Matrix" (BSS 0x82FB3220, written at runtime by the
//      map manager's construct) is initialised ("Mask Aspect Correction Matrix not
//      initialised.", :889);
//   2. vperm-extracts the rect's two corners ({x,y} and {z,w}) and pushes each through
//      BrnGui::MapTransform::Transform(point, &0x82FB2FA0, &0x82FB3010) -- the runtime-built
//      proportion-space -> aspect-corrected-device-space pair;
//   3. orders the two transformed corners min/max per axis (the vcmpgtfp swap pair);
//   4. builds a 2-vertex corner run -- positions from the ordered corners, UVs from the caller's
//      mask-UV vector ({u0,v0} on the min corner, {u1,v1} on the max), the colour lane left as
//      filler (the console stores a NaN pattern; nothing reads it) -- and pushes it with
//      Im2dRenderBuffer::PushMask(lpTextureState, run) (opcode 17).
//
// [FLAG PC fold] step 1+2: the console's matrix pair maps 0..1 screen proportions into its
// aspect-corrected device space and exists only once the map manager has built it (hence the
// assert). The PC Im2d dispatch consumes mask corners in logical screen pixels, so the fold is
// the constant proportion -> 1280x720 logical-pixel scale -- always defined, so the
// not-initialised assert has no PC condition to fire on and is dropped with this note. The
// corner ordering, UV wiring and the opcode-17 push are transcribed as-is.
void SetMaskRect(CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex>* lpRenderBuffer,
                 const renderengine::TextureState* lpTextureState,
                 const rw::math::vpu::Vector4& lrv4Rect,
                 const rw::math::vpu::Vector4& lrv4MaskUVs)
{
    if (lpRenderBuffer == 0)
        return;

    // Corner A = {x,y}, corner B = {z,w}, proportion space -> logical pixels (the fold above).
    f32 lfAX = lrv4Rect.x * 1280.0f;
    f32 lfAY = lrv4Rect.y * 720.0f;
    f32 lfBX = lrv4Rect.z * 1280.0f;
    f32 lfBY = lrv4Rect.w * 720.0f;

    // Order min/max per axis (the console's two vcmpgtfp corner swaps).
    if (lfAX > lfBX) { const f32 lfSwap = lfAX; lfAX = lfBX; lfBX = lfSwap; }
    if (lfAY > lfBY) { const f32 lfSwap = lfAY; lfAY = lfBY; lfBY = lfSwap; }

    CgsGraphics::Basic2dColouredTexturedVertex laCorners[2];
    laCorners[0].mv2Pos.x    = lfAX;
    laCorners[0].mv2Pos.y    = lfAY;
    laCorners[0].mv2Tex0UV.x = lrv4MaskUVs.x;
    laCorners[0].mv2Tex0UV.y = lrv4MaskUVs.y;
    laCorners[1].mv2Pos.x    = lfBX;
    laCorners[1].mv2Pos.y    = lfBY;
    laCorners[1].mv2Tex0UV.x = lrv4MaskUVs.z;
    laCorners[1].mv2Tex0UV.y = lrv4MaskUVs.w;
    // The colour lane is filler on the console (a NaN bit pattern the dispatcher never reads);
    // stamped opaque here so the record carries no uninitialised bytes.
    *reinterpret_cast<u32*>(&laCorners[0].mv4Colour) = 0xFFFFFFFFu;
    *reinterpret_cast<u32*>(&laCorners[1].mv4Colour) = 0xFFFFFFFFu;

    lpRenderBuffer->PushMask(lpTextureState, laCorners);
}

// ================= SetMaskRect @ 0x82450D28 =================
// Push a clip mask over a normalised screen rect. The X360 asserts the mask aspect
// matrix (@0x82FB3220) is initialised, transforms the rect's two corners through the
// normalised->mask-NDC product (@0x82FB3010), min/max-orders them, then hands
// Im2dRenderBuffer::PushMask two {pos, colour 0xFFFFFFFF, uv} corner vertices.
// [H3b PC fold]: the dispatch's mask consumes LOGICAL 1280x720 pixel corners (the
// scissor + stage-1 alpha fold in CgsImRenderBufferTemplate.cpp), so the corner
// transform collapses to the logical-screen scale and the matrix dependency drops out.
void SetMaskRect(CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex>& lrCmd,
                 const renderengine::TextureState* lpMaskTextureState,
                 const Vector4& lv4Rect, const Vector4& lv4MaskUv)
{
    const f32 KF_LOGICAL_W = 1280.0f;
    const f32 KF_LOGICAL_H = 720.0f;

    // Corner ordering (the X360's two vcmpgtfp min/max picks).
    const f32 lfX0 = (lv4Rect.x < lv4Rect.z) ? lv4Rect.x : lv4Rect.z;
    const f32 lfX1 = (lv4Rect.x < lv4Rect.z) ? lv4Rect.z : lv4Rect.x;
    const f32 lfY0 = (lv4Rect.y < lv4Rect.w) ? lv4Rect.y : lv4Rect.w;
    const f32 lfY1 = (lv4Rect.y < lv4Rect.w) ? lv4Rect.w : lv4Rect.y;

    CgsGraphics::Basic2dColouredTexturedVertex laCorners[2];
    laCorners[0].mv2Pos.x    = lfX0 * KF_LOGICAL_W;
    laCorners[0].mv2Pos.y    = lfY0 * KF_LOGICAL_H;
    laCorners[0].mv4Colour.r = 0xFF; laCorners[0].mv4Colour.g = 0xFF;
    laCorners[0].mv4Colour.b = 0xFF; laCorners[0].mv4Colour.a = 0xFF;
    laCorners[0].mv2Tex0UV.x = lv4MaskUv.x;
    laCorners[0].mv2Tex0UV.y = lv4MaskUv.y;
    laCorners[1].mv2Pos.x    = lfX1 * KF_LOGICAL_W;
    laCorners[1].mv2Pos.y    = lfY1 * KF_LOGICAL_H;
    laCorners[1].mv4Colour.r = 0xFF; laCorners[1].mv4Colour.g = 0xFF;
    laCorners[1].mv4Colour.b = 0xFF; laCorners[1].mv4Colour.a = 0xFF;
    laCorners[1].mv2Tex0UV.x = lv4MaskUv.z;
    laCorners[1].mv2Tex0UV.y = lv4MaskUv.w;

    // PushMask binds the mask TEXTURE STATE directly (the DWARF :191 record variant;
    // the dispatcher unwraps it to its raster exactly like SET_STATE_TEXTURE).
    lrCmd.PushMask(lpMaskTextureState, laCorners);
}

// ================= SetMaskAspectCorrectionMatrix @ 0x82450A70 =================
// The X360 builds the mask NDC coord space ({-1,-1,1,1} + the display aspect fold,
// stored @0x82FB3220) and its composition with the normalised space (@0x82FB3010) --
// the matrices the console SetMaskRect corner transform consumes. On the PC fold
// SetMaskRect builds logical-pixel corners directly, so there is no matrix to store;
// the function remains as the (empty) call-order peer of GuiModule::Construct.
void SetMaskAspectCorrectionMatrix(GuiCache* lpGuiCache)
{
    (void)lpGuiCache;
}

} // namespace BrnGui

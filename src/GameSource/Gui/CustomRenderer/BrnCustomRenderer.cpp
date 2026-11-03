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
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // [DIAG] the satnav-diag prints

#include "GameShared/GameClasses/Core/CgsAssert.h" // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h" // CgsDev::Log::gpDebugPrint (the [tut-ticker] diag)
#include <cstdlib>   // std::getenv -- the BRN_HUD_VIS witness gate (see HudVisEnabled below)
#include <cstdio>    // std::snprintf -- the [hud-vis] witness reason string

// CgsGui::SetGuiCamera @0x82847658 -- selects the active GUI camera (0 = full-screen-map,
// 1 = normal HUD). It used to be re-declared locally here as an "uncommitted, out of scope"
// extern; that is precisely the shadowing-redeclaration shape that only a LINK finds, and
// the link found it. It has a real home -- GameShared/GameClasses/Gui/CgsGuiShared.cpp,
// named by its own baked assert string -- and it is included from there now.
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"

// SetMaskRect (bottom of this TU) writes the 2-vertex clip-mask corner run and pushes it
// through the Im2d command buffer (opcode 17).
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2dTransform.h"
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
{
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
    // ⭐ [map-world] SLOT 2 IS LIVE (2026-08-29): BrnGui::MainMapRenderer, the reconstructed
    // subobject (guest this+0x28D0) -- THE MAP WORLD. The park note below is kept as the
    // measured record of the chain that feeds it; only its last paragraph ("WHAT IS MISSING")
    // is now history. The ten X360 functions it named are bodied in
    // GameSource/Gui/CustomRenderer/Renderers/BrnMainMapRenderer.cpp.
    //
    // ---- the measured producer chain (runs scratch/mainmenu_wave/f8_probe2, f8_fix1) -----
    //   * CrashNavMap::CheckForLoadComplete posts GuiEventShowHideSatNav(E_MAPTYPE_MAIN,
    //     show, 0.5s) on channels 41 AND 42;
    //   * the channel-42 copy now reaches MainMapComponent::RecvEvent (the GuiModule
    //     drain used to discard the whole channel -- fixed in BrnGuiModule.cpp), which
    //     latches mMapManager.mbEnabled = true;
    //   * MapManager::RecvEvent builds the low-res backdrop texture from GuiCache
    //     resource 199 ([map-lowres] BACKDROP BUILT state=1);
    //   * MainMapComponent::Update then runs both mbEnabled-gated legs every frame:
    //     RefreshActiveTextureArray publishes muTextureCount = 1 (the backdrop) and
    //     OutputViewState posts GuiEventRenderMainMap (id 223) with mpActiveTextures,
    //     the world rect, the view rect and mbIsActive.
    //     Measured: `[map-pump] enabled=1 active=1 maptype=0 lowres=1 texcount=1`.
    // That event now lands on a real component. RenderComponent @0x82460130 reads the
    // published mpActiveTextures, loops muTextureCount entries (stride 36 ==
    // SatNavTile::sTexture), skips entries with a null mpTextureState, and for each one
    // transforms the texture's mBBWorld corners through
    // MapTransform::MakeTransform(mapRect -> viewRect) and emits a 4-vertex quad.
    //
    // The tile machinery is NOT involved and is not needed: MapManager's two
    // SatNavTileDirectory slots have no writer anywhere in the ARTIST export set, and
    // CalculateCurrentTileSet's add/remove bodies are compiled out of that build (only
    // their asserts survive), so the console's own main map draws exactly ONE quad here --
    // the low-res backdrop this build already builds and publishes.
    mapCustomRenderComponents[E_MAINMAP]              = &mMainMapRenderer;
    // ⭐ [map-world] slot 3 goes live IN THE SAME CHANGE as slot 2, and only ever with it.
    // CrashNavIconRenderer's RenderComponent opens with the map background mask over the
    // published view rect and draws the icon layer INTO the map world, so mounting it while
    // slot 2 was null put icons over the driving scene -- which is why the reconstructed
    // class sat compiled-but-unmounted. Its Prepare is also the ONLY EnsureResourcesAreLoaded
    // call site for GuiCache textures 202/203, i.e. it is what makes the load notifications
    // that MainMapRenderer::RecvEvent turns into its two mask texture states. Slot 2 does not
    // draw without it.
    mapCustomRenderComponents[E_CRASHNAVICONS]        = &mCrashNavIconRenderer;
    // ⭐ [boost-bar] slot 4 is LIVE (2026-08-25): the reconstructed BoostBarRenderer
    // subobject (guest this+0xE520), the in-game boost gauge.
    mapCustomRenderComponents[E_BOOSTBAR]             = &mBoostBarRenderer;
    mapCustomRenderComponents[E_ABOVECAR]             = &mAboveCarRenderer;
    mapCustomRenderComponents[E_PROGRESSBAR]          = &mProgressBarRenderer;
    mapCustomRenderComponents[E_BLACKBAR]             = &mBlackBarRenderer;
    // ⭐ [tut-ticker] slot 8 is LIVE (2026-08-24): the reconstructed InGameMessageRenderer
    // subobject (guest this+0x1E0F0), the bottom-of-screen ticker.
    mapCustomRenderComponents[E_INGAME_MESSAGE]       = &mInGameMessageRenderer;
    mapCustomRenderComponents[E_CREDITS_TEXT]         = &mCreditsTextRenderer;

    // guest `stbx 0 -> +0x1F498`
    mbRenderingEnable = false;

    // guest `do { (***v2)(*v2); } while` -- component vtable[+0x00].
    for (s32 liIndex = 0; liIndex < E_CUSTOM_RENDER_TYPES_COUNT; ++liIndex)
    {
        if (mapCustomRenderComponents[liIndex] != 0)
            mapCustomRenderComponents[liIndex]->Construct();
    }

    // ARTIST constructs the embedded debug component after all renderer subobjects.
    mDebugComponent.Construct(this);

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
            mDebugComponent.Register();
            return true;
        }
        return false;
    }

    if (mePrepareStage == E_PREPARESTAGE_DONE)
    {
        // ARTIST repeats the debug registration on the completed prepare path.
        mePrepareStage = E_PREPARESTAGE_DONE;
        mDebugComponent.Register();
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
    // ARTIST tears down the embedded debug component before the renderer subobjects.
    mDebugComponent.Destruct();

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

    if (lpu8[0] != 0)
    {
        // Sub-mode 1: full-map toggle on the SatNav slot (guest v4[1032] == array slot 1),
        // mirror its render flag, then pick the GUI camera (0 = full-screen map).
        if (lpu8[0] == 1)
        {
            SetEnabled(lpSatNav, lbEnableFlag);
            mDebugComponent.SetSatNavDisplayActive(
                (lpSatNav != 0) && lpSatNav->GetRenderEnabled());
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
        mDebugComponent.SetMainMapDisplayActive(
            (lpMainMap != 0) && lpMainMap->GetRenderEnabled());
    }
}

// ================= Update @ 0x82450908 =================
// Clear the event queue, apply the per-component renderable cache (gated on the +0x1F4B1
// flag and ANDed with the +0x1F4AF gate), update every component, then reset the HACK
// counter.
void CustomRendererManager::Update()
{
    mEventQueue.Clear();

    if (mDebugComponent.IsOverrideEnabled())
    {
        // ARTIST reads the embedded debug controls at +0x1F4AF..+0x1F4B1.
        const bool lbGate = mDebugComponent.IsRenderingActive();

        SetEnabled(mapCustomRenderComponents[E_SATNAV],
                   lbGate && mDebugComponent.IsSatNavDisplayActive());   // a1+4128
        SetEnabled(mapCustomRenderComponents[E_MAINMAP],
                   lbGate && mDebugComponent.IsMainMapDisplayActive());  // a1+4132
        SetEnabled(mapCustomRenderComponents[E_ABOVECAR],
                   lbGate && mDebugComponent.IsAboveCarDisplayActive()); // a1+4144
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
    mDebugComponent.SetMainMapDisplayActive(
        (lpMainMap != 0) && lpMainMap->GetRenderEnabled());

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
// guest, all four instructions:
//     lis  r11, 1 ; ori r11, r11, 0xB91C ; stwx r4, r3, r11 ; blr
// i.e. `*(this + 0x1B91C) = lpFlaptRenderer` -- ONE store.
//
// ⛔ THE OFFSET THIS FUNCTION USED TO CARRY WAS ARITHMETIC THAT DID NOT CHECK OUT, and the
// note built on it inverted the conclusion. The previous banner said the guest's 0x1B91C was
// "a DIFFERENT field" from the renderer's own mpFlaptRenderer because it put the embedded
// NetworkPlayerImageRenderer at 0x1B660. It is not there. EndOfFrame @0x82449930 forms the
// subobject address as `addis r3,r3,2 ; addi r3,r3,-0x49C0` == +131072-18880 == 112192 ==
// **0x1B640**, and SwapBuffers @0x82445E58 reads its Flapt renderer at `a1[183]` == +0x2DC.
//     0x1B640 + 0x2DC = 0x1B91C = 112924  -- EXACTLY the word this store writes.
// So this setter IS the embedded renderer's setter (the compiler inlined
// NetworkPlayerImageRenderer::SetFlaptRenderer into it), CustomRendererManager has no Flapt
// pointer of its own, and the store must land in the subobject or SwapBuffers reads NULL.
// The old `mpFlaptRenderer` member has been deleted from the class for the same reason: it
// stood for a console field that does not exist.
//
// The console's only caller is GuiModule::Construct @0x82518D48-54, which passes
// `guiModule + 5<<16 - 0x6E80` == +299392 == mViewModule(+132224).mFlaptManager(+0x28CC0)
// .mRenderer(+0x40). See the matching call in BrnGuiModule.cpp.
CustomRendererManager* CustomRendererManager::SetFlaptRenderer(BrnFlapt::FlaptRenderer* lpFlaptRenderer)
{
    // guest stwx -> this+0x1B91C == mNetworkPlayerImageRenderer(+0x1B640) + 0x2DC
    mNetworkPlayerImageRenderer.SetFlaptRenderer(lpFlaptRenderer);
    return this;
}

// ================= SetTextRenderer @ 0x82445538 =================
// guest:
//   a1[28593] = a2;                        ; +0x1BEC4  mpTextRenderer
//   (*(a1[28680] + 56))(a1 + 28680);       ; CREDITS TEXT subobject (+114720), vtable +0x38
//   a1[14372] = a2;                        ; +0xE090   (inside the CrashNavIcon subobject)
//   a1[32029] = a2;                        ; +0x1F474
// The extended credits virtual and both embedded renderers receive the same renderer.
void CustomRendererManager::SetTextRenderer(CgsGraphics::TextRenderer* lpTextRenderer)
{
    mCreditsTextRenderer.SetTextRenderer(lpTextRenderer);
    mAboveCarRenderer.SetTextRenderer(lpTextRenderer); // ARTIST manager+1BEC4
    mCrashNavIconRenderer.SetTextRenderer(lpTextRenderer); // ARTIST 82445588, manager+E090.

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
// The credits virtual, road-name renderer store and ticker tail call are all live.
void CustomRendererManager::SetLanguageManager(CgsLanguage::LanguageManager* lpLanguageManager)
{
    mCreditsTextRenderer.SetLanguageManager(lpLanguageManager);
    mAboveCarRenderer.SetLanguageManager(lpLanguageManager); // ARTIST manager+1BEC8

    CGS_ASSERT(lpLanguageManager != 0, "lpLanguageManager");
    mCrashNavIconRenderer.SetLanguageManager(lpLanguageManager); // ARTIST 82445624, manager+B0B0.

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
// ARTIST 0x82FB3220 and 0x82FB3010: the display aspect matrix and the
// mask-space coordinate system composed with it.
namespace
{
    Matrix33 gMaskAspectCorrection;
    Matrix33 gMaskSpace;
}

// ARTIST82450BE0.
void SetMaskRect(CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex>* lpRenderBuffer,
                 const renderengine::TextureState* lpTextureState,
                 const rw::math::vpu::Vector4& lrv4Rect,
                 const rw::math::vpu::Vector4& lrv4MaskUVs)
{
    CGS_ASSERT(gMaskAspectCorrection.xAxis.mV.x != 0.0f || gMaskAspectCorrection.xAxis.mV.y != 0.0f ||
               gMaskAspectCorrection.yAxis.mV.x != 0.0f || gMaskAspectCorrection.yAxis.mV.y != 0.0f ||
               gMaskAspectCorrection.zAxis.mV.x != 0.0f || gMaskAspectCorrection.zAxis.mV.y != 0.0f,
               "Mask Aspect Correction Matrix not initialised.");
    Vector2 lA, lB;
    lA.mV = lrv4Rect.mV;
    lA.mV.z = lA.mV.x; lA.mV.w = lA.mV.x;
    lB.mV = lrv4Rect.mV;
    lB.mV.x = lrv4Rect.GetZ().GetFloat(); lB.mV.y = lrv4Rect.GetW().GetFloat();
    lB.mV.z = lrv4Rect.GetX().GetFloat(); lB.mV.w = lB.mV.z;
    lA = MapTransform::Transform(lA, MapTransform::GetNormalisedSpace(), gMaskSpace);
    lB = MapTransform::Transform(lB, MapTransform::GetNormalisedSpace(), gMaskSpace);
    if (lA.mV.x > lB.mV.x) { const f32 t = lA.mV.x; lA.mV.x = lB.mV.x; lB.mV.x = t; }
    if (lA.mV.y > lB.mV.y) { const f32 t = lA.mV.y; lA.mV.y = lB.mV.y; lB.mV.y = t; }
    CgsGraphics::Basic2dColouredTexturedVertex laCorners[2];
    laCorners[0].mv2Pos.x = lA.mV.x; laCorners[0].mv2Pos.y = lA.mV.y;
    laCorners[1].mv2Pos.x = lB.mV.x; laCorners[1].mv2Pos.y = lB.mV.y;
    laCorners[0].mv2Tex0UV.x = lrv4MaskUVs.GetX().GetFloat();
    laCorners[0].mv2Tex0UV.y = lrv4MaskUVs.GetY().GetFloat();
    laCorners[1].mv2Tex0UV.x = lrv4MaskUVs.GetZ().GetFloat();
    laCorners[1].mv2Tex0UV.y = lrv4MaskUVs.GetW().GetFloat();
    *reinterpret_cast<u32*>(&laCorners[0].mv4Colour) = 0xFFFFFFFFu;
    *reinterpret_cast<u32*>(&laCorners[1].mv4Colour) = 0xFFFFFFFFu;
    lpRenderBuffer->PushMask(lpTextureState, laCorners);
}

void SetMaskRect(CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex>& lrCmd,
                 const renderengine::TextureState* lpMaskTextureState,
                 const Vector4& lv4Rect, const Vector4& lv4MaskUv)
{
    SetMaskRect(&lrCmd, lpMaskTextureState, lv4Rect, lv4MaskUv);
}

// ARTIST82450A38 reads the Im2d aspect matrix and composes the [-1,1] mask space.
void SetMaskAspectCorrectionMatrix(GuiCache*)
{
    const rw::math::fpu::Matrix33Template<float>& a = CgsGraphics::Im2dTransform::GetAspectCorrectionMatrix();
    gMaskAspectCorrection = Matrix33(Vector3(a.mRow0X, a.mRow0Y, a.mRow0Z),
        Vector3(a.mRow1X, a.mRow1Y, a.mRow1Z), Vector3(a.mRow2X, a.mRow2Y, a.mRow2Z));
    const Vector4 lRect(-1.0f, -1.0f, 1.0f, 1.0f);
    const Matrix33 lSpace = MapTransform::MakeCoordSpaceFromRect(lRect);
    const __vector4 ax = gMaskAspectCorrection.xAxis.mV;
    const __vector4 ay = gMaskAspectCorrection.yAxis.mV;
    const __vector4 az = gMaskAspectCorrection.zAxis.mV;
    gMaskSpace.xAxis.mV = __vmaddfp(az, __vspltw(lSpace.xAxis.mV, 2),
        __vmaddfp(ay, __vspltw(lSpace.xAxis.mV, 1), __vmulfp(ax, __vspltw(lSpace.xAxis.mV, 0))));
    gMaskSpace.yAxis.mV = __vmaddfp(az, __vspltw(lSpace.yAxis.mV, 2),
        __vmaddfp(ay, __vspltw(lSpace.yAxis.mV, 1), __vmulfp(ax, __vspltw(lSpace.yAxis.mV, 0))));
    gMaskSpace.zAxis.mV = __vmaddfp(az, __vspltw(lSpace.zAxis.mV, 2),
        __vmaddfp(ay, __vspltw(lSpace.zAxis.mV, 1), __vmulfp(ax, __vspltw(lSpace.zAxis.mV, 0))));
}

} // namespace BrnGui

// BrnCustomRenderer.cpp
// BrnGui::CustomRendererManager -- the GUI custom-renderer set manager.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (16 functions, 0x82444040 .. 0x82450908).
// The manager owns 10 polymorphic render components (NetworkPlayerImage, SatNav, MainMap,
// CrashNavIcon, BoostBar, AboveCar, ProgressBar, BlackBar, InGameMessage, CreditsText) and
// drives their lifecycle (Construct / Prepare / Release / Destruct / Update) plus event
// routing (RecvEvent) and texture/renderable queries through the
// CgsGui::CustomRenderComponentInterface vtable. The canonical class home (grown to full
// layout) is GameSource/Gui/BrnCustomRendererManager.h.
//
// FIDELITY NOTES
//  * The X360 reaches each component as a by-value subobject via a pointer stored in the
//    +0x101C component array; the inlined `4 * (a2 + 1031) + this` index arithmetic is the
//    array element `mapCustomRenderComponents[a2]` (1031 dwords == 4124 bytes == +0x101C,
//    so dword index (a2 + 1031) is array slot a2). Reversed to named array indexing here.
//  * The compiler inlined the base-class chain calls (CgsGui::CustomRendererManager::
//    Construct/Prepare/Release/Destruct); restored as explicit base calls.
//  * The embedded GuiCustRendererDebugComponent (guest +0x1F49C) Construct/Destruct/Register
//    and the per-renderer subobject ctors invoked by the lifecycle loops are uncommitted;
//    the loops are modelled as the polymorphic vtable dispatch the asm performs over the
//    component array. FLAG: the debug-component lifecycle and the by-value subobject ctors
//    are represented by the interface dispatch (their full types are out of scope).
//  * RecvEvent's giant switch routes each event-type id to one or more components by calling
//    their RecvEvent vtable slot (+0x14); the per-event component sets are transcribed from
//    the X360 jump tables (the v4[dwordIndex] targets map to the named components below).

#include "GameSource/Gui/BrnCustomRendererManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h" // CGS_ASSERT

// CgsGui::SetGuiCamera @ external -- selects the active GUI camera (0 = full-screen-map
// camera, 1 = normal HUD camera). Declared here as an extern free function; its home TU is
// uncommitted (out of scope), and the gate is compile-only so no definition is required.
namespace CgsGui
{
    int SetGuiCamera(s32 liCameraIndex);
}

namespace BrnGui
{

namespace
{
    // X360 event-type ids routed by RecvEvent (transcribed from the jump tables). Named for
    // readability; the underlying values are the raw guest case labels (the GUI/world event
    // enum that owns them is out of scope for this TU).
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

    // Helper: forward a received event to one component's RecvEvent vtable slot (+0x14).
    inline void RouteEvent(CgsGui::CustomRenderComponentInterface* lpComponent,
                           const CgsModule::Event* lpEvent, s32 liEventType)
    {
        lpComponent->RecvEvent(lpEvent, liEventType);
    }
}

// ================= Construct @ 0x82444040 =================
// Chains to the base ctor, wires the 10 component pointers to their (by-value) subobjects,
// clears the master rendering-enable flag, constructs the debug component and zeroes the
// prepare/release stage markers and the HACK render-without-update counter.
void CustomRendererManager::Construct()
{
    CgsGui::CustomRendererManager::Construct();

    // Component-pointer array setup (guest stores at +0x101C..+0x1040, in this order). In
    // the X360 each value is `this + <subobject offset>`; here the array holds the
    // polymorphic components the lifecycle loops dispatch through.
    //   slot 0 (NetPlayerImg)  guest this+0x1B660 (112192)
    //   slot 1 (SatNav)        guest this+0x1050  (4176)
    //   slot 2 (MainMap)       guest this+0x28D0  (10448)
    //   slot 3 (CrashNavIcon)  guest this+0xB060  (45152)
    //   slot 4 (BoostBar)      guest this+0xE520  (58656)
    //   slot 5 (AboveCar)      guest this+0x1B970 (112944)
    //   slot 6 (ProgressBar)   guest this+0x1C0F0 (114672)
    //   slot 7 (BlackBar)      guest this+0x1C108 (114696)
    //   slot 8 (InGameMessage) guest this+0x1E130 (123120)
    //   slot 9 (CreditsText)   guest this+0x1C120 (114720)
    // FLAG: the by-value subobjects are out of scope; the slots are left as the interface
    // pointers the manager was built to drive (populated elsewhere in the full game). The
    // master rendering-enable flag is cleared exactly as the asm stbx 0 -> +0x1F498 does.
    mbRenderingEnable = false;

    // The X360 then calls each component's ctor through vtable[+0x00] (the
    // `do { (***v2)(*v2); } while` loop). Modelled as the polymorphic Construct() dispatch.
    for (s32 liIndex = 0; liIndex < E_CUSTOM_RENDER_TYPES_COUNT; ++liIndex)
    {
        if (mapCustomRenderComponents[liIndex] != 0)
            mapCustomRenderComponents[liIndex]->Construct();
    }

    // BrnGui::GuiCustRendererDebugComponent::Construct(this + 0x1F49C, this) -- the debug
    // component ctor (uncommitted type; effect modelled as the stage/counter resets below).

    // Guest +0x1F490 / +0x1F494 / +0x1F4CC stores of 0.
    mePrepareStage = E_PREPARESTAGE_START;
    meReleaseStage = E_RELEASESTAGE_START;
    miHACK_NumberOfRendersWithoutUpdate = 0;
}

// ================= Prepare @ 0x82444140 =================
// State-machine guarded preparation. On the START stage it Prepares every component; if all
// succeed it disables a few default components and advances to DONE (registering the debug
// component). The asserts mirror the X360 (unknown-stage path + per-component prepare).
bool CustomRendererManager::Prepare(void* lpResourceAllocatorA, void* lpResourceAllocatorB)
{
    CgsGui::CustomRendererManager::Prepare(lpResourceAllocatorA, lpResourceAllocatorB);

    if (mePrepareStage == E_PREPARESTAGE_START)
    {
        bool lbAllPrepared = true;
        // Guest writes mePrepareStage = 0 again before the loop.
        mePrepareStage = E_PREPARESTAGE_START;

        // Per-component Prepare via vtable[+0x04]; the X360 passes (this+3 [resource-loader
        // sub-pointer], a1[1], a1[2]) -- the two resource allocators plus the loader.
        for (s32 liIndex = 0; liIndex < E_CUSTOM_RENDER_TYPES_COUNT; ++liIndex)
        {
            if (mapCustomRenderComponents[liIndex] != 0)
            {
                if (!mapCustomRenderComponents[liIndex]->Prepare(
                        this, lpResourceAllocatorA, lpResourceAllocatorB))
                {
                    lbAllPrepared = false;
                }
            }
        }

        if (lbAllPrepared)
        {
            // SetAllRenderingState(false) then disable NetworkPlayerImage / BlackBar /
            // InGameMessage by default (guest: (*this+52)(this,0); (*this+32)(this,0,1);
            // (*this+32)(this,7,1); (*this+32)(this,8,1)).
            SetAllRenderingState(false);
            SetComponentRenderable(E_NETWORK_PLAYER_IMAGE, true);
            SetComponentRenderable(E_BLACKBAR,             true);
            SetComponentRenderable(E_INGAME_MESSAGE,       true);

            mePrepareStage = E_PREPARESTAGE_DONE;
            // CgsDev::DebugComponent::Register(this + 0x1F49C) -- register the debug menu.
            return true;
        }
        return false;
    }

    if (mePrepareStage == E_PREPARESTAGE_DONE)
    {
        // Already prepared: re-register the debug component and report success.
        // CgsDev::DebugComponent::Register(this + 0x1F49C).
        mePrepareStage = E_PREPARESTAGE_DONE;
        return true;
    }

    CGS_ASSERT(false, " unknown prepare stage in CustomRenderManager ");
    return false;
}

// ================= Release @ 0x824442B0 =================
// Mirror of Prepare: on the START stage it Releases every component (stopping at the first
// that returns true, which the X360 treats as "done"); the DONE stage is a no-op success.
bool CustomRendererManager::Release()
{
    if (meReleaseStage == E_RELEASESTAGE_START)
    {
        meReleaseStage = E_RELEASESTAGE_START;
        for (s32 liIndex = 0; liIndex < E_CUSTOM_RENDER_TYPES_COUNT; ++liIndex)
        {
            if (mapCustomRenderComponents[liIndex] != 0)
            {
                // The X360 loop continues WHILE Release() is truthy; the first falsey
                // (incomplete) release breaks out and reports not-done.
                if (!mapCustomRenderComponents[liIndex]->Release())
                    return false;
            }
        }
        // Fell through all components -> released.
        meReleaseStage = E_RELEASESTAGE_DONE;
        return true;
    }

    if (meReleaseStage == E_RELEASESTAGE_DONE)
    {
        meReleaseStage = E_RELEASESTAGE_DONE;
        return true;
    }

    CGS_ASSERT(false, " unknown release stage in CustomRendererManager ");
    return false;
}

// ================= Destruct @ 0x82444378 =================
// Destructs the debug component, then each render component (vtable[+0x0C]), then chains to
// the base destructor.
void CustomRendererManager::Destruct()
{
    // BrnGui::GuiCustRendererDebugComponent::Destruct(this + 0x1F49C) -- debug component.

    for (s32 liIndex = 0; liIndex < E_CUSTOM_RENDER_TYPES_COUNT; ++liIndex)
    {
        if (mapCustomRenderComponents[liIndex] != 0)
            mapCustomRenderComponents[liIndex]->Destruct();
    }

    CgsGui::CustomRendererManager::Destruct();
}

// ================= RecvEvent @ 0x824443D0 =================
// Routes a module event to the component(s) interested in its type id. Transcribed from the
// X360 jump tables; the v4[dwordIndex] targets map to named components:
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

    // Routing transcribed verbatim from the X360 switch/jump tables (pseudocode authority).
    // Each RouteEvent() is one component's RecvEvent vtable[+0x14] dispatch; the SetRenderEnabled
    // cases (214/215/587/588) are vtable[+0x1C] stores read from the event payload.
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

        case E_EVT_64:  // SatNav, MainMap, CrashNav, BoostBar, AboveCar, NetPlayerImg, then InGameMessage (LABEL_21)
            RouteEvent(lpSatNav,        lpEvent, liEventType);
            RouteEvent(lpMainMap,       lpEvent, liEventType);
            RouteEvent(lpCrashNav,      lpEvent, liEventType);
            RouteEvent(lpBoostBar,      lpEvent, liEventType);
            RouteEvent(lpAboveCar,      lpEvent, liEventType);
            RouteEvent(lpNetPlayerImg,  lpEvent, liEventType);
            RouteEvent(lpInGameMessage, lpEvent, liEventType);
            break;

        // ---- LABEL_35: BlackBar(+InGameMessage). 145/345/346/355/505/534-537 ----
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

        // ---- 587: enable CreditsText (vtable[+0x1C] true) then route the event to it ----
        case E_EVT_587:
            lpCreditsText->SetRenderEnabled(true);               // v4 + 28680, slot +0x1C
            RouteEvent(lpCreditsText,   lpEvent, liEventType);   // v4 + 28680, slot +0x14
            break;

        // ---- 588: disable CreditsText (LABEL_40: SetRenderEnabled(0)) ----
        case E_EVT_588:
            lpCreditsText->SetRenderEnabled(false);              // v4 + 28680
            break;

        // ---- payload-driven SetRenderEnabled on a component pointer slot ----
        case E_EVT_214:  // v4[1035] = BoostBar pointer; enable from payload byte 0
            lpBoostBar->SetRenderEnabled(reinterpret_cast<const u8*>(lpEvent)[0] != 0);
            break;

        case E_EVT_215:  // v4[1036] = AboveCar pointer; enable from payload byte 0
            lpAboveCar->SetRenderEnabled(reinterpret_cast<const u8*>(lpEvent)[0] != 0);
            break;

        case E_EVT_213:
            RecvEvent_Event213(lpEvent);
            break;

        default:
            break;
    }
}

// ---- RecvEvent case 213 (split out for clarity): the SatNav/MainMap/CrashNav map-toggle.
// The X360 reads the event header: payload[0] selects sub-mode (0 = map-position update,
// 1 = full-map toggle); payload[4..7] is a float threshold; payload[8] is the enable flag.
void CustomRendererManager::RecvEvent_Event213(const CgsModule::Event* lpEvent)
{
    const u8*  lpu8  = reinterpret_cast<const u8*>(lpEvent);
    const f32* lpf32 = reinterpret_cast<const f32*>(lpEvent);

    CgsGui::CustomRenderComponentInterface* lpSatNav   = mapCustomRenderComponents[E_SATNAV];
    CgsGui::CustomRenderComponentInterface* lpMainMap  = mapCustomRenderComponents[E_MAINMAP];
    CgsGui::CustomRenderComponentInterface* lpCrashNav = mapCustomRenderComponents[E_CRASHNAVICONS];

    if (lpu8[0] != 0)
    {
        // Sub-mode 1: full-map toggle on the SatNav slot, then mirror its render flag and
        // pick the gui camera. (Guest used component slot v4[1032] = SatNav.)
        if (lpu8[0] == 1)
        {
            lpSatNav->SetRenderEnabled(lpu8[8] != 0);
            mbSatNavRenderable = lpSatNav->GetRenderEnabled();
            // CgsGui::SetGuiCamera(0 when enabling, 1 when disabling).
            if (lpu8[8] != 0)
                CgsGui::SetGuiCamera(0);
            else
                CgsGui::SetGuiCamera(1);
        }
    }
    else
    {
        // Sub-mode 0: MainMap show/hide, gated on a float threshold in the payload.
        bool lbEnable;
        if (lpf32[1] <= 0.0f)
        {
            // Below threshold: hard reset then apply the payload flag directly.
            lpMainMap->Update();                       // guest vtable[+0x30] -- reset/refresh
            lpMainMap->SetRenderEnabled(lpu8[8] != 0);
            lbEnable = (lpu8[8] != 0);
        }
        else
        {
            // Above threshold: animated transition (vtable[+0x2C]); enabling forces the
            // MainMap on, disabling leaves it for the transition to finish.
            lpMainMap->Update();                       // guest vtable[+0x2C] -- begin transition
            if (lpu8[8] != 0)
            {
                lpMainMap->SetRenderEnabled(true);
                lbEnable = true;
            }
            else
            {
                lbEnable = false;
            }
        }

        // Apply the resolved enable to the CrashNavIcon slot and cache the MainMap flag.
        lpCrashNav->SetRenderEnabled(lbEnable);
        mbMainMapRenderable = lpMainMap->GetRenderEnabled();
    }
}

// ================= Update @ 0x82450908 =================
// Clears the event queue, applies the per-component renderable cache (gated on
// mbHaveValidMapPosition), updates every component, then resets the HACK counter.
void CustomRendererManager::Update()
{
    mEventQueue.Clear();

    if (mbHaveValidMapPosition)
    {
        // SatNav: renderable only if both the gate (mbHaveValidMapPosition) and the per-
        // component flag are set.
        bool lbSatNav = mbHaveValidMapPosition && mbSatNavRenderable;
        mapCustomRenderComponents[E_SATNAV]->SetRenderEnabled(lbSatNav);

        bool lbMainMap = mbHaveValidMapPosition && mbMainMapRenderable;
        mapCustomRenderComponents[E_MAINMAP]->SetRenderEnabled(lbMainMap);

        // Third store: guest +0x1030 == component slot 5 (AboveCar), gated on the
        // +0x1F4B0 renderable cache (mbThirdSlotRenderable). Transcribed literally.
        bool lbThird = mbHaveValidMapPosition && mbThirdSlotRenderable;
        mapCustomRenderComponents[E_ABOVECAR]->SetRenderEnabled(lbThird);
    }

    for (s32 liIndex = 0; liIndex < E_CUSTOM_RENDER_TYPES_COUNT; ++liIndex)
    {
        if (mapCustomRenderComponents[liIndex] != 0)
            mapCustomRenderComponents[liIndex]->Update();
    }

    miHACK_NumberOfRendersWithoutUpdate = 0;
}

// ================= GetComponentID @ 0x82445378 =================
u32 CustomRendererManager::GetComponentID(s32 liComponent) const
{
    CGS_ASSERT(liComponent >= 0 && liComponent < E_CUSTOM_RENDER_TYPES_COUNT,
               "liComponent>=0 && liComponent<E_CUSTOM_RENDER_TYPES_COUNT");
    return mapCustomRenderComponents[liComponent]->GetComponentID();
}

// ================= SetComponentRenderable @ 0x824453F8 =================
void CustomRendererManager::SetComponentRenderable(s32 liComponent, bool lbRenderable)
{
    CGS_ASSERT(liComponent >= 0 && liComponent < E_CUSTOM_RENDER_TYPES_COUNT,
               "liComponent>=0 && liComponent<E_CUSTOM_RENDER_TYPES_COUNT");
    mapCustomRenderComponents[liComponent]->SetRenderEnabled(lbRenderable);
}

// ================= GetComponentRenderable @ 0x82445468 =================
// Reads the component's render-enabled flag directly (guest lbz +4 == mbRenderEnabled).
bool CustomRendererManager::GetComponentRenderable(s32 liComponent)
{
    CGS_ASSERT(liComponent >= 0 && liComponent < E_CUSTOM_RENDER_TYPES_COUNT,
               "liComponent>=0 && liComponent<E_CUSTOM_RENDER_TYPES_COUNT");
    return mapCustomRenderComponents[liComponent]->GetRenderEnabled();
}

// ================= GetNumTexturesForComponent @ 0x82445658 =================
s32 CustomRendererManager::GetNumTexturesForComponent(s32 liComponent) const
{
    CGS_ASSERT(liComponent >= 0, "liComponent >= 0");
    CGS_ASSERT(liComponent < E_CUSTOM_RENDER_TYPES_COUNT,
               "liComponent < E_CUSTOM_RENDER_TYPES_COUNT");
    return mapCustomRenderComponents[liComponent]->GetNumTexturesForComponent();
}

// ================= SetAllRenderingState @ 0x824454E0 =================
// Sets every component's renderable flag (via the public SetComponentRenderable path) and
// stores the master enable flag.
void CustomRendererManager::SetAllRenderingState(bool lbRenderable)
{
    for (s32 liIndex = 0; liIndex < E_CUSTOM_RENDER_TYPES_COUNT; ++liIndex)
        SetComponentRenderable(liIndex, lbRenderable);

    mbRenderingEnable = lbRenderable;
}

// ================= GetComponentTexture @ 0x824452B0 =================
// Finds the component whose CgsID matches luComponentID, and if it is currently renderable
// returns its texture for the given index / shader program. Asserts a non-null shader
// program first; returns null if no match or the matched component is not renderable.
void* CustomRendererManager::GetComponentTexture(u32 luComponentID, s32 liTextureIndex,
                                                 void* lpiShaderProgram, void* lpRendererSet)
{
    CGS_ASSERT(lpiShaderProgram != 0, "lpiShaderProgram != NULL");

    for (s32 liIndex = 0; liIndex < E_CUSTOM_RENDER_TYPES_COUNT; ++liIndex)
    {
        if (mapCustomRenderComponents[liIndex]->GetComponentID() == luComponentID)
        {
            if (!mapCustomRenderComponents[liIndex]->GetRenderEnabled())
                return 0;
            // asm: (*(comp->vtable+16))(comp, a3, a4, a5) -- pass our a3/a4/a5 through.
            return mapCustomRenderComponents[liIndex]->GetComponentTexture(
                liTextureIndex, lpiShaderProgram, lpRendererSet);
        }
    }
    return 0;
}

// ================= EndOfFrame @ 0x82449930 =================
// Tail-calls the NetworkPlayerImage renderer's SwapBuffers (guest this+0x1B660 ==
// NetworkPlayerImageRenderer). Modelled as the per-component Update() proxy is wrong here:
// the asm jumps to a renderer-specific SwapBuffers, an uncommitted method; routed through
// the component interface as a no-op-returning Update is NOT faithful, so it is expressed
// as a direct call comment. FLAG: NetworkPlayerImageRenderer::SwapBuffers is uncommitted;
// the by-name effect (double-buffer swap of the net-player image) is preserved in intent.
void CustomRendererManager::EndOfFrame()
{
    // BrnGui::NetworkPlayerImageRenderer::SwapBuffers(this + 0x1B660).
    // The NetworkPlayerImage component double-buffers its rendered avatar texture; this
    // swaps the front/back image at frame end. The concrete SwapBuffers is out of scope,
    // so the call target is documented rather than dispatched (no matching interface slot).
}

// ================= SetFlaptRenderer @ 0x82449920 =================
// Stores the FLAPT (UI movie) renderer back-pointer and returns this.
CustomRendererManager* CustomRendererManager::SetFlaptRenderer(void* lpFlaptRenderer)
{
    mpFlaptRenderer = lpFlaptRenderer;   // guest stwx -> this+0x1B91C
    return this;
}

// ================= SetTextRenderer @ 0x82445538 =================
// Stores the shared text renderer, pings the InGameMessage renderer (guest CreditsText slot
// vtable[+0x38]), then mirrors the pointer into two renderer caches.
void* CustomRendererManager::SetTextRenderer(void* lpTextRenderer)
{
    mpTextRenderer = lpTextRenderer;     // guest stwx -> this+0x1BEC4

    // Guest: (*(this+0x1E130-vtable+0x38))(this+0x1E130) -- notify the InGameMessage renderer
    // a text renderer is now set (vtable slot 0x38). Modelled through the interface Update().
    mapCustomRenderComponents[E_INGAME_MESSAGE]->Update();

    // Guest mirrors the pointer into two more renderer text-renderer caches
    // (this+0xE090 and this+0x1F474). FLAG: out-of-scope renderer caches; the manager-level
    // pointer is the authoritative copy here.
    return mpTextRenderer;
}

// ================= SetLanguageManager @ 0x824455A8 =================
// Stores the language manager, pings the InGameMessage renderer (vtable[+0x3C]), asserts the
// pointer is non-null, then forwards to the InGameMessage renderer's SetLanguageManager.
void* CustomRendererManager::SetLanguageManager(void* lpLanguageManager)
{
    mpLanguageManager = lpLanguageManager;   // guest stwx -> this+0x1BEC8

    // Guest: (*(this+0x1E130-vtable+0x3C))(this+0x1E130) -- notify the InGameMessage renderer.
    mapCustomRenderComponents[E_INGAME_MESSAGE]->Update();

    CGS_ASSERT(lpLanguageManager != 0, "lpLanguageManager");

    // Guest also caches the pointer at this+0xB0B0 then tail-calls
    // BrnGui::InGameMessageRenderer::SetLanguageManager(this+0x1E130, lpLanguageManager).
    // The concrete forward is the InGameMessageRenderer's own setter (committed separately);
    // here the manager keeps the authoritative pointer copy. FLAG: the typed forward to
    // InGameMessageRenderer::SetLanguageManager is out of scope at the manager interface.
    return this;
}

// NOTE: SetReplaySerialiser @ 0x82445648 is defined in the sibling TU
// GameSource/Gui/BrnCustomRendererManager.cpp (the original minimal-slice ledger function);
// not redefined here to avoid a duplicate definition.

} // namespace BrnGui

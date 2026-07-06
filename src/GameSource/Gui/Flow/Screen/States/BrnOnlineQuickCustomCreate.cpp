// ===================================================================================
// BrnGui::OnlineQuickCustomCreate -- out-of-line bodies (internal Update* slice).
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   ProcessSelectedMenuOption @0x824873C8, UpdateLoadComponents @0x8248DEF0,
//   UpdateRunning @0x824926A8, UpdatePermanent @0x82492728.
// (HandleControllerInputPressed / OnEnter / OnLeave / UpdateLoadResources are SKIPPED
//  this wave -- declared-only.)
// The drain-loop / expected-component wiring mirrors the committed OnlineMarkMan twin.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineQuickCustomCreate.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                        // CgsModule::Event
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // VariableEventQueue<18432,16>
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache, GuiFlow

namespace BrnGui
{
namespace
{
    // The state IN-queue is an 18KB variable event queue (X360 VariableEventQueue<18432,16>),
    // the same view ReplayMain / Credits cast mpInGuiEventQueue to.
    typedef CgsModule::VariableEventQueue<18432, 16> InQueue;

    // In-queue return codes / event ids the drain loops act on (X360 immediates).
    const s32 KI_EVENT_RESULT_CONTROLLER_INPUT_PRESSED = 6;    // UpdateRunning: result == 6
    const s32 KI_EVENT_RESULT_MENU_OPTION_SELECTED     = 44;   // UpdatePermanent: result == 0x2C
    const s32 KI_EVENT_RESULT_FRIENDS_LIST_RESPONSE    = 493;  // UpdatePermanent: result == 0x1ED

    // FLAG boundaries: the GuiCache apt-component watcher LIST-CLEAR + the picked-option store the
    // X360 reaches are not on the committed public APIs, so they are reached through this file-local
    // boundary (same convention as BrnGui::OnlineMarkManCacheBoundary).
    namespace OnlineQCCCacheBoundary
    {
        // X360 GuiCache::ClearExpectedAptComponentList(mpGuiCache, flow) (not on public API).
        void ClearExpectedAptComponentList(GuiCache* /*lpCache*/, s32 /*liFlow*/) {}
        // X360 *(mpGuiCache + 0x4B40) = picked menu option id (or 0 when the event has no payload).
        void StoreSelectedMenuOption(GuiCache* /*lpCache*/, s32 /*liOption*/) {}
    }
}   // anonymous namespace

// ---- statics -----------------------------------------------------------------
// The main-menu row localisation-key table (X360 off_82F268EC, 3 entries). Only the first
// pointer's rodata is attested in scope; the other two are the sibling CUSTOM/CREATE keys.
const char* const OnlineQuickCustomCreate::KAPC_MAIN_MENU_TEXT[E_MAIN_MENU_OPTIONS_COUNT] =
{
    "$ONLINE_MAIN_MENU_OPTION_QUICK_MATCH",   // @off_82F268EC (attested)
    "$ONLINE_MAIN_MENU_OPTION_CUSTOM_MATCH",  // FLAG: sibling key, string unattested in scope
    "$ONLINE_MAIN_MENU_OPTION_CREATE_MATCH",  // FLAG: sibling key, string unattested in scope
};

// The load-string apt-name key posted to the loader (X360 off_82F27B94).
const char* const OnlineQuickCustomCreate::KAC_LOAD_STRING_APT_NAME = "ON_QMCMCM";   // @off_82F27B94

// Picked-option -> state-event name table ProcessSelectedMenuOption walks (X360 off_82F268F8).
const char* const OnlineQuickCustomCreate::KAPC_MAIN_MENU_STATE_ACTIONS_TEXT[E_MAIN_MENU_OPTIONS_COUNT] =
{
    "TO_QWK_MAT",     // [0] (X360-attested @0x82F268F8)
    "TO_CUST_MAT",    // [1] (UNATTESTED placeholder)
    "TO_CRT_MAT",     // [2] (UNATTESTED placeholder)
};

// The state's static resource list (X360 .rdata @0x8205F9C8; count 2). The two tuples' id + type
// bytes are not attested in scope (only the base address + the count of 2); every sibling entry
// is a single {apt-id, E_GUI_RESOURCETYPE_APT} tuple, reproduced with placeholder ids.
const CgsGui::sResourceTuple OnlineQuickCustomCreate::maResourcesToLoad[] =
{
    { 0u, CgsGui::E_GUI_RESOURCETYPE_APT },   // FLAG: id unattested in scope
    { 0u, CgsGui::E_GUI_RESOURCETYPE_APT },   // FLAG: id unattested in scope
};
const u32 OnlineQuickCustomCreate::muNumResourcesToLoad = 2u;

// ------------------------------------------------ ProcessSelectedMenuOption @ 0x824873C8
void OnlineQuickCustomCreate::ProcessSelectedMenuOption(EMainMenuOptions leOption)
{
    // Tail-call through the option -> state-event table (X360 lwzx + b SendStateEvent).
    SendStateEvent(KAPC_MAIN_MENU_STATE_ACTIONS_TEXT[leOption]);
}

// ------------------------------------------------ UpdateLoadComponents @ 0x8248DEF0
// Once the cache reports every expected apt component initialised (flow 0), finalise the
// main-menu: clear the expected-component list, activate the three rows (SetupMenu(3, wrap)),
// localise each row from KAPC_MAIN_MENU_TEXT, drive the "new news" transition component's
// apt_Transition view to "Invisible", and report done. Otherwise keep waiting.
bool OnlineQuickCustomCreate::UpdateLoadComponents()
{
    if (mpGuiCache == 0 || !mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
    {
        return false;
    }

    OnlineQCCCacheBoundary::ClearExpectedAptComponentList(mpGuiCache, 0);

    mMainMenuComponent.SetupMenu(E_MAIN_MENU_OPTIONS_COUNT, true);
    for (s32 liRow = 0; liRow < E_MAIN_MENU_OPTIONS_COUNT; ++liRow)
    {
        mMainMenuComponent.SetText(liRow, KAPC_MAIN_MENU_TEXT[liRow]);
    }

    mNewNewsAnimation.AddOutputAptViewState("apt_Transition", "Invisible", false);
    return true;
}

// ------------------------------------------------ UpdateRunning @ 0x824926A8
// The RUNNING-state per-frame drain: forward every controller-input-pressed event (6) to
// HandleControllerInputPressed.
void OnlineQuickCustomCreate::UpdateRunning()
{
    InQueue* lpInQueue = reinterpret_cast<InQueue*>(mpInGuiEventQueue);

    const CgsModule::Event* lpEvent = 0;
    s32 liSize = 0;
    s32 liResult = lpInQueue->GetFirstEvent(&lpEvent, &liSize);

    while (lpEvent != 0)
    {
        if (liResult == KI_EVENT_RESULT_CONTROLLER_INPUT_PRESSED)
        {
            HandleControllerInputPressed(lpEvent);
        }

        const CgsModule::Event* lpNextEvent = 0;
        liResult = lpInQueue->GetNextEvent(lpEvent, &lpNextEvent, &liSize);
        lpEvent  = lpNextEvent;
    }
}

// ------------------------------------------------ UpdatePermanent @ 0x82492728
// Drained every frame regardless of internal state. On a menu-option-selected event (44), latch
// the picked option id into the cache and send GO_BACK; on the friends-list response (493) with
// no payload, fire the null-event assert.
void OnlineQuickCustomCreate::UpdatePermanent()
{
    InQueue* lpInQueue = reinterpret_cast<InQueue*>(mpInGuiEventQueue);

    const CgsModule::Event* lpEvent = 0;
    s32 liSize = 0;
    s32 liResult = lpInQueue->GetFirstEvent(&lpEvent, &liSize);

    while (lpEvent != 0)
    {
        if (liResult == KI_EVENT_RESULT_MENU_OPTION_SELECTED)
        {
            CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:543 (non-gating)

            const s32 liOption = (lpEvent != 0)
                ? *reinterpret_cast<const s32*>(lpEvent)
                : 0;
            OnlineQCCCacheBoundary::StoreSelectedMenuOption(mpGuiCache, liOption);

            SendStateEvent("GO_BACK");
        }
        else if (liResult == KI_EVENT_RESULT_FRIENDS_LIST_RESPONSE && lpEvent == 0)
        {
            CGS_ASSERT(lpEvent != 0, "lpEvent");   // cpp:597 (non-gating)
        }

        const CgsModule::Event* lpNextEvent = 0;
        liResult = lpInQueue->GetNextEvent(lpEvent, &lpNextEvent, &liSize);
        lpEvent  = lpNextEvent;
    }
}
}   // namespace BrnGui

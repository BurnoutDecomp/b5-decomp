// ===================================================================================
// BrnGui::OnlineQuickCustomCreate -- out-of-line bodies.
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   OnEnter @0x824A1420, OnLeave @0x824A1538, ProcessSelectedMenuOption @0x824873C8,
//   UpdateLoadComponents @0x8248DEF0, UpdateLoadResources @0x824A17D8,
//   UpdateRunning @0x824926A8, UpdatePermanent @0x82492728.
// (HandleControllerInputPressed is declared-only -- linked from another slice.)
// The drain-loop / expected-component / event-post wiring mirrors the committed
// OnlineMarkMan / OnlineNews / ReplayMain twins.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineQuickCustomCreate.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                        // CgsModule::Event, GuiEventQueueLarge
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface (Register/PlayAptMovie/GetOutputEventQueue)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // VariableEventQueue<18432,16>
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache, GuiFlow

// The Xbox XNotify listener plumbing OnEnter/OnLeave key on. The XNotify* / CloseHandle entry
// points are the Xbox 360 XDK C-API; declared extern "C" at file scope with plain types
// (void* / int / unsigned long long), mirroring the committed BrnNetworkNotificationManagerX360
// precedent -- the Xbox HANDLE / DWORD typedefs are NOT modelled in types.hpp.
extern "C"
{
    void* XNotifyCreateListener(unsigned long long luqwAreas);
    int   CloseHandle(void* lhObject);
}

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
        // X360 MenuComponent::AppendExpectedAptComponent(&mMainMenuComponent, flow, mpGuiCache):
        // register each active menu row's apt component with the cache watcher. That MenuComponent
        // method (@0x824E2DE0) is deferred (GuiCache::AppendExpectedAptComponent is not yet bodied),
        // so it is reached through this boundary (same convention as ClearExpectedAptComponentList).
        void AppendMenuExpectedAptComponents(MenuComponent* /*lpMenu*/, s32 /*liFlow*/,
                                             GuiCache* /*lpCache*/) {}
        // X360 sub_824F87C0(mpGuiCache, flow, lpacComponentName): register a single expected apt
        // component (the "new news" transition component) by name with the cache watcher. The
        // collaborator is un-homed in scope, so it is reached through this boundary.
        void AppendExpectedAptComponentByName(GuiCache* /*lpCache*/, s32 /*liFlow*/,
                                              const char* /*lpacName*/) {}
    }
}   // anonymous namespace

// ---- statics -----------------------------------------------------------------
// The four GUI event ids observed by this state (X360 dword_8205F9B4, count 4). FLAG: the id
// VALUES are not attested in scope (only the base address + count of 4); placeholders so the state
// links, adopted with the XEX-recovered ids when decoded.
const s32 OnlineQuickCustomCreate::maiEventToObserve[] = { 0, 0, 0, 0 };   // @0x8205F9B4 (values not decoded)
const s32 OnlineQuickCustomCreate::miNumEventsObserved = 4;

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

// ------------------------------------------------ OnEnter @ 0x824A1420
// Register the four observed GUI events, build the main-menu (3 rows) + "new news" transition
// components, null-latch the cache into GetCache, post the two "open screen" apt/view-state events
// onto the output queue and create the XNotify system listener.
void OnlineQuickCustomCreate::OnEnter()
{
    mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

    // Main-menu component: 3 rows, no parent name, apt-id 0xFFFFFFFF (X360 li -1 ; clrldi 32).
    mMainMenuComponent.Construct("MenuItem", mpStateInterface, E_MAIN_MENU_OPTIONS_COUNT, 0,
                                 0xFFFFFFFFull);
    // "New news" transition component (virtual GuiComponent::Construct, no parent name).
    mNewNewsAnimation.Construct("NewNewsTransition", mpStateInterface, 0);

    mpGuiCache      = 0;                          // this+0x38
    meInternalState = E_INTERNALSTATE_GETCACHE;   // this+0x3C = 0

    CgsGui::GuiStackEventQueue::GuiEventQueueLarge* lpOutQueue =
        mpStateInterface->GetOutputEventQueue();

    // Record 1: GuiEvent<191> { header0=8, type=191, header2=12, payload=0,0 } -- channel 40, 20 bytes.
    u32 lauRecord1[5] = { 8u, 191u, 12u, 0u, 0u };
    lpOutQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lauRecord1), 40, 20);

    // Record 2: GuiEvent<148> { header0=1, type=148, header2=12, payload=0 } -- channel 40, 16 bytes.
    u32 lauRecord2[4] = { 1u, 148u, 12u, 0u };
    lpOutQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lauRecord2), 40, 16);

    mpNotifyListenerHandle = XNotifyCreateListener(1);   // this+0x1190 (XNOTIFY_SYSTEM areas)
}

// ------------------------------------------------ OnLeave @ 0x824A1538
// Unregister the observed events, post the teardown apt-movie + "close screen" view-state events,
// clear the main-menu into the Left state, and close the XNotify listener handle.
void OnlineQuickCustomCreate::OnLeave()
{
    mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

    // Inlined GuiEventPlayAptMovie (type 18, channel 41, size 20): the movie name is the rodata
    // sentinel &unk_820046A7 (reconstructed as ""), level 3.
    mpStateInterface->PlayAptMovie("", 3);

    meInternalState = E_INTERNALSTATE_LEFT;   // this+0x3C = 4
    mMainMenuComponent.Clear();               // component vtable slot 6

    // Record: GuiEvent<536> { header0=2, type=536, header2=12, payload=0 } -- channel 40, 16 bytes.
    CgsGui::GuiStackEventQueue::GuiEventQueueLarge* lpOutQueue =
        mpStateInterface->GetOutputEventQueue();
    u32 lauRecord[4] = { 2u, 536u, 12u, 0u };
    lpOutQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lauRecord), 40, 16);

    if (mpNotifyListenerHandle != 0)
    {
        CloseHandle(mpNotifyListenerHandle);
        mpNotifyListenerHandle = 0;
    }
}

// ------------------------------------------------ ProcessSelectedMenuOption @ 0x824873C8
void OnlineQuickCustomCreate::ProcessSelectedMenuOption(EMainMenuOptions leOption)
{
    // Tail-call through the option -> state-event table (X360 lwzx + b SendStateEvent).
    SendStateEvent(KAPC_MAIN_MENU_STATE_ACTIONS_TEXT[leOption]);
}

// ------------------------------------------------ UpdateLoadResources @ 0x824A17D8
// Wait for the screen's static resources to finish loading. Once loaded, post the load-string
// apt movie ("ON_QMCMCM", level 3), clear + rebuild the expected-apt-component list (the menu rows
// plus the "new news" transition component) and report done. Keep waiting otherwise.
bool OnlineQuickCustomCreate::UpdateLoadResources()
{
    CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:361 (non-gating)

    if (!mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad))
    {
        return false;
    }

    // Inlined GuiEventPlayAptMovie (type 18, channel 41, size 20): load the screen's apt movie.
    mpStateInterface->PlayAptMovie(KAC_LOAD_STRING_APT_NAME, 3);

    OnlineQCCCacheBoundary::ClearExpectedAptComponentList(mpGuiCache, 0);
    OnlineQCCCacheBoundary::AppendMenuExpectedAptComponents(&mMainMenuComponent, 0, mpGuiCache);
    OnlineQCCCacheBoundary::AppendExpectedAptComponentByName(mpGuiCache, 0,
                                                             mNewNewsAnimation.GetName());
    return true;
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

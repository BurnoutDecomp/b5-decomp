// ===================================================================================
// BrnGui::OnlinePlay -- out-of-line bodies for the online-play main-menu screen state.
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   OnEnter                     @0x8249BC18   OnLeave                 @0x8249BDA8
//   HandleControllerInput       @0x824AD6A0   HandleControllerInputMainMenu @0x824A7488
//   HandleGuiCacheEvent         @0x824858A0   CheckPrivileges         @0x82485968
//   SelectOnlineMenuOption      @0x8249C090   ShowMainMenuOptions     @0x8249BF58
//   ShowMainMenuScreen          @0x8249BE98
// (Update / ShowFriendsMenu / CheckForCompletedLoads / HandlePlayerStatsEvent /
//  Handle*Event / ShowFriendsSysUtil / sign-in sys-util machinery are declared-only --
//  they link from other slices of this TU.)
//
// The event-post / expected-component / cache-boundary wiring mirrors the committed
// OnlineQuickCustomCreate / OnlineNews / OnlineMarkMan twins.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlinePlay.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsModule::Event, GuiEventQueueLarge
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface, GuiEventNetworkSuspension
#include "GameShared/GameClasses/System/CgsHardwareInit.h"               // CgsSystem::HardwareInit::IsHardDiskAvailable
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache, GuiFlow
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiOverlayRequest, GuiEventActivateCrashNav

// The Xbox XNotify listener plumbing OnEnter/OnLeave key on. The XNotify* / CloseHandle
// entry points are the Xbox 360 XDK C-API; declared extern "C" at file scope with plain
// types (mirroring the committed BrnOnlineQuickCustomCreate precedent -- the Xbox HANDLE /
// DWORD typedefs are NOT modelled in types.hpp).
extern "C"
{
    void* XNotifyCreateListener(unsigned long long luqwAreas);
    int   CloseHandle(void* lhObject);
}

namespace BrnGui
{
namespace
{
    // FLAG boundaries (same convention as BrnGui::OnlineQuickCustomCreate's OnlineQCCCacheBoundary):
    // the GuiCache apt-component watcher LIST-CLEAR + the menu/by-name expected-component
    // registrars the X360 reaches are not on the committed public APIs, so they are reached
    // through this file-local boundary. Bodies are no-ops here; they link from the GuiCache /
    // component TUs.

    // X360 GuiCache::ClearExpectedAptComponentList(mpGuiCache, flow).
    void ClearExpectedAptComponentList(GuiCache* /*lpCache*/, s32 /*liFlow*/) {}

    // X360 MenuComponent::AppendExpectedAptComponent(&menu, flow, cache) @0x824E2DE0: register each
    // active menu row's apt component with the cache watcher. That method is deferred (GuiCache::
    // AppendExpectedAptComponent is not yet bodied for the menu path), so it is reached here.
    void AppendMenuExpectedAptComponents(MenuComponent* /*lpMenu*/, s32 /*liFlow*/,
                                         GuiCache* /*lpCache*/) {}

    // X360 sub_824F87C0(mpGuiCache, flow, name): register a single expected apt component (the
    // "new news" transition component) by name with the cache watcher (un-homed in scope).
    void AppendExpectedAptComponentByName(GuiCache* /*lpCache*/, s32 /*liFlow*/,
                                          const char* /*lpacName*/) {}

    // FLAG: the X360 emits the overlay request through
    // CgsGui::StateInterface::OutputGuiEvent<BrnGui::GuiOverlayRequest> (@0x824A7694 / @0x824A76D0).
    // The committed GuiOverlayRequest (BrnGuiEventTypeDefs.h) is a standalone request payload with
    // no GuiEvent<N> event-type id recovered in scope, so the generic OutputGuiEvent template cannot
    // be instantiated on it without fabricating that id (worse than an honest gap). The request is
    // still fully built (Construct latches the compressed overlay id) and handed off through this
    // boundary; GROW to the real OutputGuiEvent<GuiOverlayRequest> once GuiOverlayRequest carries its
    // event-type id.
    void OutputOverlayRequest(CgsGui::StateInterface* /*lpStateInterface*/,
                              GuiOverlayRequest& /*lrRequest*/) {}
}   // anonymous namespace

// ---- statics ------------------------------------------------------------------------
// The eight GUI event ids observed by this state (X360 dword_8205EF64, count 8). FLAG: the id
// VALUES are not attested in scope (only the base address + count of 8); placeholders so the
// state links, adopted with the XEX-recovered ids when decoded.
const s32 OnlinePlay::maiEventToObserve[] = { 0, 0, 0, 0, 0, 0, 0, 0 };   // @0x8205EF64 (values not decoded)
const s32 OnlinePlay::miNumEventsObserved = 8;

// The state's static resource list (X360 .rdata @0x8205EF88; count 3). The tuples' id + type
// bytes are not attested in scope (only the base address + the count of 3); every sibling entry
// is a single {apt-id, E_GUI_RESOURCETYPE_APT} tuple, reproduced with placeholder ids.
const CgsGui::sResourceTuple OnlinePlay::maResourceTuplesToLoad[] =
{
    { 0u, CgsGui::E_GUI_RESOURCETYPE_APT },   // FLAG: id unattested in scope
    { 0u, CgsGui::E_GUI_RESOURCETYPE_APT },   // FLAG: id unattested in scope
    { 0u, CgsGui::E_GUI_RESOURCETYPE_APT },   // FLAG: id unattested in scope
};
const s32 OnlinePlay::miNumResourcesToLoad = 3;

// The main-menu row localisation-key table (X360 off_82F267E4, 7 entries). Only the first
// pointer's rodata is attested in scope; the rest are the option-named siblings.
const char* const OnlinePlay::KAPC_MAIN_MENU_TEXT[E_MAIN_MENU_OPTIONS_COUNT] =
{
    "$ONLINE_MAIN_MENU_OPTION_FREEBURN",         // @off_82F267E4 (attested)
    "$ONLINE_MAIN_MENU_OPTION_IMAGE_GALLERY",    // FLAG: option-named, string unattested in scope
    "$ONLINE_MAIN_MENU_OPTION_VIEW_CHALLENGES",  // FLAG: option-named, string unattested in scope
    "$ONLINE_MAIN_MENU_OPTION_UNRANKED",         // FLAG: option-named, string unattested in scope
    "$ONLINE_MAIN_MENU_OPTION_RANKED",           // FLAG: option-named, string unattested in scope
    "$ONLINE_MAIN_MENU_OPTION_SCOREBOARDS",      // FLAG: option-named, string unattested in scope
    "$ONLINE_MAIN_MENU_OPTION_NEWS",             // FLAG: option-named, string unattested in scope
};

// Picked-option -> state-event name table SelectOnlineMenuOption indexes (X360 off_82F26800).
const char* const OnlinePlay::KAPC_MAIN_MENU_STATE_ACTIONS_TEXT[E_MAIN_MENU_OPTIONS_COUNT] =
{
    "TO_FBURN",     // [0] FREEBURN        (X360-attested @0x82F26800)
    "TO_IMG_GAL",   // [1] IMAGE_GALLERY   FLAG: UNATTESTED placeholder
    "TO_VW_CHAL",   // [2] VIEW_CHALLENGES FLAG: UNATTESTED placeholder
    "TO_UNRANKED",  // [3] UNRANKED        FLAG: UNATTESTED placeholder
    "TO_RANKED",    // [4] RANKED          FLAG: UNATTESTED placeholder
    "TO_SCORES",    // [5] SCOREBOARDS     FLAG: UNATTESTED placeholder
    "TO_NEWS",      // [6] NEWS            FLAG: UNATTESTED placeholder
};

// ------------------------------------------------ OnEnter @ 0x8249BC18
void OnlinePlay::OnEnter()
{
    mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

    // "New news" transition component + the network player-stats panel (virtual Construct, no
    // parent name); then the main-menu component (7 rows, no parent name, apt-id 0xFFFFFFFF).
    mNewNewsAnimation.Construct("NewNewsTransition", mpStateInterface, 0);
    mPlayerStatsDisplay.Construct("PlayerStats", mpStateInterface, 0);
    mMainMenuComponent.Construct("MenuItem", mpStateInterface, E_MAIN_MENU_OPTIONS_COUNT, 0,
                                 0xFFFFFFFFull);

    meSubState = E_SUBSTATE_LOADING_SCREEN;   // +0x2414 = 0

    // X360: strncpy(macLocalPlayerName, &unk_820046A7, 16) -- the source is the empty-name rodata
    // sentinel, so the copy zero-fills the 16-byte name buffer.
    for (s32 liByte = 0; liByte < KI_PLAYER_NAME_LENGTH; ++liByte)
        macLocalPlayerName[liByte] = '\0';

    miLocalPlayerStatsValue = 0;   // +0x2400
    miLocalPlayerImageIndex = 0;   // +0x23FC
    mpGuiCache              = 0;    // +0x241C
    mbInviteInProgress      = false;
    mbPerformingInvite      = false;

    mPlayerStatsEvent.Construct();   // NetworkPlayerStats::Construct on the +0x2378 record

    mpNotifyListenerHandle = XNotifyCreateListener(1);   // +0x2420 (XNOTIFY_SYSTEM areas)

    CgsGui::GuiStackEventQueue::GuiEventQueueLarge* lpOutQueue =
        mpStateInterface->GetOutputEventQueue();

    // Record A: GuiEvent<264> { header0=8, type=264, header2=12, payload={1,0xFFFFFFFF} } -- channel 40, 20 bytes.
    u32 lauRecordA[5] = { 8u, 264u, 12u, 1u, 0xFFFFFFFFu };
    lpOutQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lauRecordA), 40, 20);

    // Record B: GuiEvent<191> (GuiEventActivateCrashNav shape) { 8, 191, 12, 0, 0 } -- channel 40, 20 bytes.
    u32 lauRecordB[5] = { 8u, 191u, 12u, 0u, 0u };
    lpOutQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lauRecordB), 40, 20);

    // Record C: GuiEvent<148> { 1, 148, 12, payload=0 } -- channel 40, 16 bytes.
    u32 lauRecordC[4] = { 1u, 148u, 12u, 0u };
    lpOutQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lauRecordC), 40, 16);
}

// ------------------------------------------------ OnLeave @ 0x8249BDA8
void OnlinePlay::OnLeave()
{
    mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

    // Inlined GuiEventPlayAptMovie (type 18, channel 41, size 20): the movie name is the empty
    // rodata sentinel (&unk_820046A7), level 3.
    mpStateInterface->PlayAptMovie("", 3);

    meSubState = E_SUBSTATE_LEFT_SCREEN;   // +0x2414 = 3
    mMainMenuComponent.Clear();            // component vtable slot 6
    mPlayerStatsDisplay.Destruct();

    // Record: GuiEvent<264> { header0=8, type=264, header2=12, payload={0,0xFFFFFFFF} } -- channel 40, 20 bytes.
    CgsGui::GuiStackEventQueue::GuiEventQueueLarge* lpOutQueue =
        mpStateInterface->GetOutputEventQueue();
    u32 lauRecord[5] = { 8u, 264u, 12u, 0u, 0xFFFFFFFFu };
    lpOutQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lauRecord), 40, 20);

    if (mpNotifyListenerHandle != 0)
    {
        CloseHandle(mpNotifyListenerHandle);
        mpNotifyListenerHandle = 0;
    }
}

// ------------------------------------------------ HandleControllerInput @ 0x824AD6A0
void OnlinePlay::HandleControllerInput(const CgsModule::Event* lpEvent)
{
    CGS_ASSERT(lpEvent != NULL, "Invalid event sent to OnlinePlay::HandleControllerInput");

    if (meSubState == E_SUBSTATE_MAIN)
        HandleControllerInputMainMenu(
            reinterpret_cast<const GuiEventControllerInputPressed*>(lpEvent));
}

// ------------------------------------------------ HandleControllerInputMainMenu @ 0x824A7488
void OnlinePlay::HandleControllerInputMainMenu(const GuiEventControllerInputPressed* lpEvent)
{
    CGS_ASSERT(lpEvent != NULL,
               "Invalid event sent to OnlinePlay::HandleControllerInputMainMenu");

    // The controller action id rides in the event's second word (raw payload view).
    const s32 liAction =
        *reinterpret_cast<const s32*>(reinterpret_cast<const u8*>(lpEvent) + 4);

    switch (liAction)
    {
        case ')':   // 0x29 -- navigate to the next menu row (component nav virtual, slot 0x38).
            mMainMenuComponent.HighlightNext();   // FLAG: slot->direction inferred
            break;

        case '*':   // 0x2A -- navigate to the previous menu row (component nav virtual, slot 0x34).
            mMainMenuComponent.HighlightPrevious();   // FLAG: slot->direction inferred
            break;

        case '-':   // 0x2D
        case '2':   // 0x32 -- back out: suspend network, activate CrashNav, GO_BACK, post the leave cmd.
        {
            CgsGui::GuiEventNetworkSuspension lSuspend(false);
            mpStateInterface->OutputGuiEvent(lSuspend);

            GuiEventActivateCrashNav lActivateCrashNav(true);
            mpStateInterface->OutputGuiEvent(lActivateCrashNav);

            SendStateEvent("GO_BACK");

            CgsGui::GuiStackEventQueue::GuiEventQueueLarge* lpOutQueue =
                mpStateInterface->GetOutputEventQueue();
            // GuiEvent<533> { 1, 533, 12, payload=0 } -- channel 40, 16 bytes.
            u32 lauRecord[4] = { 1u, 533u, 12u, 0u };
            lpOutQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lauRecord), 40, 16);
            break;
        }

        case '1':   // 0x31 -- confirm/select the highlighted option.
        {
            const EMainMenuOptions leOption =
                static_cast<EMainMenuOptions>(mMainMenuComponent.miHighlightedIndex);

            if (!CheckPrivileges(leOption))
            {
                GuiOverlayRequest lRequest;
                lRequest.Construct("CNOnlPrivErr");
                OutputOverlayRequest(mpStateInterface, lRequest);
                break;
            }

            CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");

            // The offline options select immediately; the online-start options (freeburn / unranked /
            // ranked) show the "start online?" confirmation first, unless a start is already in
            // progress or there is no active game mode.
            if (leOption == E_MAIN_MENU_OPTIONS_FREEBURN ||
                leOption == E_MAIN_MENU_OPTIONS_UNRANKED ||
                leOption == E_MAIN_MENU_OPTIONS_RANKED)
            {
                const bool lbConfirm =
                    (!mpGuiCache->IsOnlineStartInProgress() && mpGuiCache->GetGameMode() != -1);
                if (lbConfirm)
                {
                    GuiOverlayRequest lRequest;
                    lRequest.Construct("CNOnlStrtQn");
                    OutputOverlayRequest(mpStateInterface, lRequest);
                }
                else
                {
                    SelectOnlineMenuOption(leOption);
                }
            }
            else
            {
                SelectOnlineMenuOption(leOption);
            }
            break;
        }

        case '4':   // 0x34 -- open the friends menu.
            ShowFriendsMenu();
            break;

        case '6':   // 0x36
            SendStateEvent("TOGGLE_LEFT");
            break;

        case '7':   // 0x37
            SendStateEvent("TOGGLE_RIGHT");
            break;

        default:
            break;
    }
}

// ------------------------------------------------ HandleGuiCacheEvent @ 0x824858A0
void OnlinePlay::HandleGuiCacheEvent(const GuiEventCache* lpEvent)
{
    // The event's carried cache rides in its leading word (the X360 asserts on *a2).
    GuiCache* lpCache = *reinterpret_cast<GuiCache* const*>(lpEvent);
    CGS_ASSERT(lpCache != NULL, "Invalid cache in OnlinePlay::HandleGuiCacheEvent");

    if (mpGuiCache == NULL)
        mpGuiCache = lpCache;

    mbInviteInProgress = mpGuiCache->IsInviteInProgress();     // cache +0x4B4D
    mbPerformingInvite = mpGuiCache->IsPerformingInvite();     // cache +0x4B4F
}

// ------------------------------------------------ CheckPrivileges @ 0x82485968
bool OnlinePlay::CheckPrivileges(EMainMenuOptions leOption)
{
    const bool lbMultiplayerAllowed = mpGuiCache->IsMultiplayerAllowed();

    switch (leOption)
    {
        case E_MAIN_MENU_OPTIONS_FREEBURN:
        case E_MAIN_MENU_OPTIONS_UNRANKED:
        case E_MAIN_MENU_OPTIONS_RANKED:
            return lbMultiplayerAllowed;

        case E_MAIN_MENU_OPTIONS_IMAGE_GALLERY:
        case E_MAIN_MENU_OPTIONS_VIEW_CHALLENGES:
        case E_MAIN_MENU_OPTIONS_SCOREBOARDS:
        case E_MAIN_MENU_OPTIONS_NEWS:
            return true;

        default:
            CGS_ASSERT(false, "Invalid menu option");
            return false;
    }
}

// ------------------------------------------------ SelectOnlineMenuOption @ 0x8249C090
void OnlinePlay::SelectOnlineMenuOption(EMainMenuOptions leOption)
{
    SendStateEvent(KAPC_MAIN_MENU_STATE_ACTIONS_TEXT[leOption]);

    if (leOption == E_MAIN_MENU_OPTIONS_FREEBURN ||
        leOption == E_MAIN_MENU_OPTIONS_UNRANKED ||
        leOption == E_MAIN_MENU_OPTIONS_RANKED)
    {
        CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");

        if (leOption != E_MAIN_MENU_OPTIONS_FREEBURN)
        {
            mpGuiCache->SetOnlineMatchRanked(leOption != E_MAIN_MENU_OPTIONS_UNRANKED);   // +0x4B51
            mpGuiCache->SetOnlineMatchUnranked(false);                                    // +0x4B52
        }
        else
        {
            mpGuiCache->SetOnlineMatchRanked(false);
            mpGuiCache->SetOnlineMatchUnranked(true);
        }

        // GuiEvent<268> { 1, 268, 12, payload=0 } -- channel 40, 16 bytes.
        CgsGui::GuiStackEventQueue::GuiEventQueueLarge* lpOutQueue =
            mpStateInterface->GetOutputEventQueue();
        u32 lauRecord[4] = { 1u, 268u, 12u, 0u };
        lpOutQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lauRecord), 40, 16);
    }

    mpGuiCache->SetOnlineStartPending(false);   // +0x4B53 (cleared for every option)
}

// ------------------------------------------------ ShowMainMenuOptions @ 0x8249BF58
void OnlinePlay::ShowMainMenuOptions()
{
    mMainMenuComponent.SetupMenu(E_MAIN_MENU_OPTIONS_COUNT, true);
    for (s32 liRow = 0; liRow < E_MAIN_MENU_OPTIONS_COUNT; ++liRow)
        mMainMenuComponent.SetText(liRow, KAPC_MAIN_MENU_TEXT[liRow]);

    // With no hard disk the online / gallery / challenge / (un)ranked options are unavailable.
    if (!CgsSystem::HardwareInit::IsHardDiskAvailable())
    {
        mMainMenuComponent.DisableSelectable(E_MAIN_MENU_OPTIONS_FREEBURN);
        mMainMenuComponent.DisableSelectable(E_MAIN_MENU_OPTIONS_IMAGE_GALLERY);
        mMainMenuComponent.DisableSelectable(E_MAIN_MENU_OPTIONS_VIEW_CHALLENGES);
        mMainMenuComponent.DisableSelectable(E_MAIN_MENU_OPTIONS_UNRANKED);
        mMainMenuComponent.DisableSelectable(E_MAIN_MENU_OPTIONS_RANKED);
        // Re-settle the highlight onto a still-enabled row (component nav virtual, slot 0x34).
        mMainMenuComponent.HighlightPrevious();   // FLAG: slot->method inferred (same slot as '*')
    }

    mPlayerStatsDisplay.SetState(GuiNetworkPlayerStats::E_STATE_VISIBLE);

    // Show the local player's own record only once a name has been set.
    const BrnNetwork::NetworkPlayerStats* lpStats =
        (macLocalPlayerName[0] != '\0') ? &mPlayerStatsEvent : NULL;
    mPlayerStatsDisplay.SetInfo(macLocalPlayerName, miLocalPlayerStatsValue, lpStats,
                                0, false, mpGuiCache);

    // GuiEvent<259> { 1, 259, 12, payload=0 } -- channel 40, 16 bytes.
    CgsGui::GuiStackEventQueue::GuiEventQueueLarge* lpOutQueue =
        mpStateInterface->GetOutputEventQueue();
    u32 lauRecord[4] = { 1u, 259u, 12u, 0u };
    lpOutQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lauRecord), 40, 16);
}

// ------------------------------------------------ ShowMainMenuScreen @ 0x8249BE98
void OnlinePlay::ShowMainMenuScreen()
{
    // Inlined GuiEventPlayAptMovie (type 18, channel 41, size 20): play this screen's apt movie.
    mpStateInterface->PlayAptMovie("ON_MAIN", 3);

    meSubState = E_SUBSTATE_LOADING_COMPONENTS;   // +0x2414 = 1

    ClearExpectedAptComponentList(mpGuiCache, 0);
    AppendMenuExpectedAptComponents(&mMainMenuComponent, 0, mpGuiCache);
    mPlayerStatsDisplay.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);
    AppendExpectedAptComponentByName(mpGuiCache, 0, mNewNewsAnimation.GetName());
}
}   // namespace BrnGui

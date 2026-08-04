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

#include <cstddef>                                                        // offsetof (overlay wire)
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
    // OutputGuiEvent<BrnGui::GuiOverlayRequest>: both of this TU's call sites (@0x824A7698 /
    // @0x824A76D0) `bl` the SAME folded instantiation whose body is exported @0x82436BE0:
    //   memcpy(record+0x10, &request, 0x120); header {0x120=288, 0xB8=184, 0x10=16};
    //   AddEvent(iface+0xC, record, 40, 0x130=304)
    // (headless-IDA disasm, 2026-08-04). This wire mirrors the committed sibling
    // GuiOverlayRequestWire in BrnOnlineCustomMatch_wJ_04.cpp / BrnCarSelectMain_wG_02.cpp.
    struct GuiOverlayRequestWire : public CgsGui::GuiEvent<184>
    {
        u32               muPad0C;    // +0x0C (payload is 16-aligned past the header)
        GuiOverlayRequest mRequest;   // +0x10

        GuiOverlayRequestWire()
            : CgsGui::GuiEvent<184>(
                  static_cast<u32>(sizeof(GuiOverlayRequest)),                  // X360 288
                  static_cast<u32>(offsetof(GuiOverlayRequestWire, mRequest)))  // X360 16
            , muPad0C(0)
        {
        }
    };
}   // anonymous namespace

// ---- statics ------------------------------------------------------------------------
// The eight GUI event ids observed by this state (X360 dword_8205EF64, count 8; the count cell
// @0x8205EF84 == 8). Values dumped from the ARTIST rodata (headless IDA, 2026-08-04);
// corroborated by the committed siblings' recovered lists (BrnDebug observes {14, 6},
// Credits {6, 21}).
const s32 OnlinePlay::maiEventToObserve[] = { 14, 21, 6, 44, 248, 64, 189, 493 };   // @0x8205EF64 (XEX-attested)
const s32 OnlinePlay::miNumEventsObserved = 8;

// The state's static resource list (X360 .rdata @0x8205EF88; count cell @0x8205EFA0 == 3).
// Tuples dumped from the ARTIST rodata (headless IDA, 2026-08-04): three {apt-id,
// E_GUI_RESOURCETYPE_APT(4)} pairs, matching the sibling states' single-APT pattern.
const CgsGui::sResourceTuple OnlinePlay::maResourceTuplesToLoad[] =
{
    { 172u, CgsGui::E_GUI_RESOURCETYPE_APT },   // @0x8205EF88 (XEX-attested)
    { 190u, CgsGui::E_GUI_RESOURCETYPE_APT },   // @0x8205EF90 (XEX-attested)
    { 189u, CgsGui::E_GUI_RESOURCETYPE_APT },   // @0x8205EF98 (XEX-attested)
};
const s32 OnlinePlay::miNumResourcesToLoad = 3;

// The main-menu row localisation-key table (X360 off_82F267E4, 7 entries). Every pointer
// chased to its string in the ARTIST rodata (headless IDA dump, 2026-08-04). The table is
// bounded on both sides by foreign objects (the OnlinePause option keys before it, the
// actions table at +0x1C after it), confirming exactly 7 entries.
const char* const OnlinePlay::KAPC_MAIN_MENU_TEXT[E_MAIN_MENU_OPTIONS_COUNT] =
{
    "$ONLINE_MAIN_MENU_OPTION_FREEBURN",         // [0] @0x82F267E4 -> 0x8205CB3C (XEX-attested)
    "$ONLINE_MAIN_MENU_OPTION_IMAGE_GALLERY",    // [1] -> 0x8205CB14 (XEX-attested)
    "$ONLINE_MAIN_MENU_OPTION_VIEW_CHALLENGES",  // [2] -> 0x8205CAE8 (XEX-attested)
    "$ONLINE_MAIN_MENU_OPTION_UNRANKED",         // [3] -> 0x8205CAC4 (XEX-attested)
    "$ONLINE_MAIN_MENU_OPTION_RANKED",           // [4] -> 0x8205CAA4 (XEX-attested)
    "$ONLINE_MAIN_MENU_OPTION_SCOREBOARDS",      // [5] -> 0x8205CA7C (XEX-attested)
    "$ONLINE_MAIN_MENU_OPTION_NEWS",             // [6] -> 0x8205CA5C (XEX-attested)
};

// Picked-option -> state-event name table SelectOnlineMenuOption indexes (X360 off_82F26800).
// Every pointer chased to its string in the ARTIST rodata (headless IDA dump, 2026-08-04).
// Bound check: [7] onward (off_82F2681C/off_82F26820 = "Visible"/"Invisible") are separate
// named objects used by other screens -- the table is exactly 7 entries. The only readers of
// 0x82F26800 are this TU's ShowMainMenuOptions (loop bound) + SelectOnlineMenuOption (index);
// TO_IMG_GAL / TO_VIW_CHL / TO_SCOREB / TO_NEWS are additionally shared by a second screen's
// action table @0x82F272B0.., corroborating the exact spellings.
const char* const OnlinePlay::KAPC_MAIN_MENU_STATE_ACTIONS_TEXT[E_MAIN_MENU_OPTIONS_COUNT] =
{
    "TO_FBURN",     // [0] FREEBURN        @0x82F26800 -> 0x8205CA50 (XEX-attested)
    "TO_IMG_GAL",   // [1] IMAGE_GALLERY   -> 0x8205CA44 (XEX-attested)
    "TO_VIW_CHL",   // [2] VIEW_CHALLENGES -> 0x8205CA38 (XEX-attested)
    "TO_UNRANKED",  // [3] UNRANKED        -> 0x8205CA2C (XEX-attested)
    "TO_RANKED",    // [4] RANKED          -> 0x8205CA20 (XEX-attested)
    "TO_SCOREB",    // [5] SCOREBOARDS     -> 0x8205CA14 (XEX-attested)
    "TO_NEWS",      // [6] NEWS            -> 0x8205CA0C (XEX-attested)
};

// ---- OnlinePlay (ctor) @ 0x82508B40 -------------------------------------------------
// Compiler-emitted construction of the online-play main-menu screen state and its embedded
// GUI sub-objects. The X360 writes the state's own vtable (+0x000, off_820755A8), the
// "new news" transition component's GuiComponent vtable (this+0x38 = off_82072F68,
// mNewNewsAnimation), then calls MenuComponent::MenuComponent on the main-menu (this+0xC8,
// mMainMenuComponent). The player-stats panel (this+0x1188 = off_820747CC,
// mPlayerStatsDisplay) is constructed inline -- its own vtable plus the long run of per-row
// sub-widget vtables (the repeated off_82072F8C stores at +0x1214..+0x2280) -- and the local
// player-stats event record (this+0x2378, mPlayerStatsEvent) is zero-primed at its tail
// (+0x23D8/+0x23DC). Those inlined sub-object stores are exactly the embedded members' own
// default ctors; no scalar state field takes a store here (OnEnter primes meSubState / the
// local-player block / the cache + listener). Reconstructed from the X360 asm.
OnlinePlay::OnlinePlay()
    : CgsGui::State()        // state vtable + base bookkeeping (X360 +0x000)
    , mNewNewsAnimation()    // "new news" transition carrier (X360 this+0x38)
    , mMainMenuComponent()   // embedded main-menu, 7 rows    (X360 this+0xC8)
    , mPlayerStatsDisplay()  // embedded player-stats panel   (X360 this+0x1188)
    , mPlayerStatsEvent()    // local player-stats event rec  (X360 this+0x2378)
{
}

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
        case ')':   // 0x29 -- component nav virtual, vtable slot +0x38.
            // MEASURED (headless-IDA vtable dump, 2026-08-04): the MenuComponent vptr is
            // off_82074068; slot +0x38 = 0x824E4DE8 = MenuComponent::HighlightPrevious
            // (li r4,0 + b SelectableGroup::HighlightPrevious).
            mMainMenuComponent.HighlightPrevious();
            break;

        case '*':   // 0x2A -- component nav virtual, vtable slot +0x34.
            // MEASURED: slot +0x34 = 0x824E4DE0 = HighlightNext (the folded
            // li r4,0 + b SelectableGroup::HighlightNext forwarder).
            mMainMenuComponent.HighlightNext();
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
                // OutputGuiEvent<GuiOverlayRequest> @0x824A76D0: wire {288, 184, 16, request},
                // channel 40, 304 bytes.
                GuiOverlayRequestWire lRequest;
                lRequest.mRequest.Construct("CNOnlPrivErr");
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lRequest), 40,
                    static_cast<s32>(sizeof(lRequest)));
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
                    // OutputGuiEvent<GuiOverlayRequest> @0x824A7698: wire {288, 184, 16, request},
                    // channel 40, 304 bytes.
                    GuiOverlayRequestWire lRequest;
                    lRequest.mRequest.Construct("CNOnlStrtQn");
                    mpStateInterface->GetOutputEventQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lRequest), 40,
                        static_cast<s32>(sizeof(lRequest)));
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
        // Re-settle the highlight onto a still-enabled row (component nav virtual, slot +0x34).
        // MEASURED (headless-IDA vtable dump, 2026-08-04): slot +0x34 = 0x824E4DE0 =
        // HighlightNext -- walk FORWARD to the first enabled row (SCOREBOARDS/NEWS).
        mMainMenuComponent.HighlightNext();
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

    // X360: GuiCache::ClearExpectedAptComponentList(cache, 0) -> the committed per-flow clear;
    // MenuComponent::AppendExpectedAptComponent(&menu, 0, cache) @0x824E2DE0;
    // GuiNetworkPlayerStats::AppendExpectedAptComponent(&stats, 0, cache);
    // sub_824F87C0(cache, 0, name) == GuiCache::AppendExpectedAptComponent(GuiFlow, const char*)
    // (the name-taking entry, X360-attested @0x824F87C0 in BrnGuiCache.h).
    mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
    mMainMenuComponent.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);
    mPlayerStatsDisplay.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);
    mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mNewNewsAnimation.GetName());
}
}   // namespace BrnGui

// GameSource/Gui/Flow/Screen/States/BrnOnlineQuickCustomCreate.cpp
//
// BrnGui::OnlineQuickCustomCreate -- the online main menu (ON_QK_CST_CR: quick / custom /
// create match). The twelve bodies, read off the console asm:
//
//   OnEnter / OnLeave / Update          the lifecycle and the internal-state chain
//   UpdateGetCache                      adopt the cache, flag the game options for reset
//                                       and post the online ticker (GUI 537)
//   UpdateLoadResources                 load ON_QMCMCM + its package, register components
//   UpdateLoadComponents                fill the three menu rows
//   UpdateRunning                       controller presses
//   UpdatePermanent                     disconnected (GUI 44) latches the error and goes back
//   HandleControllerInputPressed        up / down / select / back / friends
//   ProcessSelectedMenuOption           the picked row's state event
//   ShowFriendsMenu                     the guide's friends list (sign-in first if needed)
//
// Out-queue records are the console's own wire records, posted on channel 40 through
// GetOutputEventQueue()->AddEvent with host sizeof sizes. The guide calls go through the
// PC platform layer (CgsXboxLivePC.cpp).

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineQuickCustomCreate.h"

#include <cstddef>                                                        // offsetof (wire records)
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N> / GuiEventWrapper
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // the state in-queue / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                     // GuiEventTickerCustomMessage / GuiEventTickerClearMessages
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiEventActivateCrashNav, GuiFlow
#include "GameSource/Gui/BrnGuiShared.h"                                  // gGuiResourceIdentifier
#include "GameSource/Input/GameInputActions.h"                            // EGameInputActions

// The guide entry points (PC bodies in the platform layer, CgsXboxLivePC.cpp family).
extern "C"
{
    void* XNotifyCreateListener(unsigned long long luqwAreas);
    int   XNotifyGetNext(void* lhListener, unsigned long ludwMsgFilter,
                         unsigned long* lpdwId, unsigned long* lpParam);
    int   CloseHandle(void* lhObject);
    u32   XUserGetSigninState(u32 luUserIndex);
    u32   XShowSigninUI(u32 luPanes, u32 luFlags);
    u32   XShowFriendsUI(u32 luUserIndex);
}

namespace BrnGui
{
    // ================================================================================
    //  Class statics (values read from the image)
    // ================================================================================

    // The events the screen observes: controller press, disconnected, gui cache, 493.
    const s32 OnlineQuickCustomCreate::maiEventToObserve[4] = { 6, 44, 64, 493 };
    const s32 OnlineQuickCustomCreate::miNumEventsObserved  = 4;

    // ON_QMCMCM and the matchmaking package.
    const CgsGui::sResourceTuple OnlineQuickCustomCreate::maResourcesToLoad[] =
    {
        { 173, CgsGui::E_GUI_RESOURCETYPE_APT },
        { 190, CgsGui::E_GUI_RESOURCETYPE_APT }
    };
    const u32 OnlineQuickCustomCreate::muNumResourcesToLoad = 2;

    const char OnlineQuickCustomCreate::KAC_NEW_NEWS_ANIMATION_COMPONENT[18] = "NewNewsTransition";
    const char OnlineQuickCustomCreate::KAC_MAIN_MENU_COMPONENT[9]           = "MenuItem";

    const char* const OnlineQuickCustomCreate::KAPC_MAIN_MENU_TEXT[E_MAIN_MENU_OPTIONS_COUNT] =
    {
        "$ONLINE_MAIN_MENU_OPTION_QUICK_MATCH",
        "$ONLINE_MAIN_MENU_OPTION_CUSTOM_MATCH",
        "$ONLINE_MAIN_MENU_OPTION_CREATE_MATCH"
    };

    const char* const OnlineQuickCustomCreate::KAPC_MAIN_MENU_STATE_ACTIONS_TEXT[E_MAIN_MENU_OPTIONS_COUNT] =
    {
        "TO_QWK_MAT", "TO_CUST_MAT", "TO_GAME_OPT"
    };

    namespace
    {
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;   // mpInGuiEventQueue's real type

        const s32 KI_CHANNEL_GUI_OUT = 40;

        const s32 KI_EVENT_CONTROLLER_INPUT       = 6;
        const s32 KI_EVENT_NETWORK_DISCONNECTED   = 44;
        const s32 KI_EVENT_GUI_CACHE              = 64;
        const s32 KI_EVENT_COLLISION_WORLD        = 493;

        // The movie UpdateLoadResources plays: the screen's own package.
        const u32 KU_MAIN_MENU_MOVIE_RESOURCE = 173;
        const s32 KI_APT_MOVIE_LEVEL          = 3;
        const char* const KPC_EMPTY_STRING    = "";

        const u64 KU64_NO_APT_ID = 0xFFFFFFFFull;   // the menu's "no apt id" (32-bit -1, zero-extended)

        const char KAC_APT_TRANSITION_NAME[] = "apt_Transition";
        const char KAC_INVISIBLE_STATE[]     = "Invisible";

        const char KAC_GO_BACK_EVENT[] = "GO_BACK";

        // The ticker line, by the kind of online game being joined; ticker string type 2.
        const char KAC_RANKED_TICKER_TEXT[]   = "ONLINE_RANKED_TICKER_TEXT";
        const char KAC_FREEBURN_TICKER_TEXT[] = "ONLINE_FREEBURN_TICKER_TEXT";
        const char KAC_UNRANKED_TICKER_TEXT[] = "ONLINE_UNRANKED_TICKER_TEXT";
        const s32  KI_TICKER_STRING_TYPE      = 2;

        // The guide: system notification areas, the sign-in states, the "UI closed" notice
        // and the invite-accepted notice; the sign-in panel shows one pane, online-enabled
        // profiles only.
        const unsigned long long KU64_NOTIFY_AREA_SYSTEM = 1;
        const u32 KU_SIGNIN_STATE_SIGNED_IN_TO_LIVE      = 2;
        const unsigned long KU_NOTIFY_SYSTEM_UI          = 9;
        const unsigned long KU_NOTIFY_LIVE_INVITE_ACCEPTED = 0x2000002;
        const u32 KU_SIGNIN_UI_PANES                     = 1;
        const u32 KU_SIGNIN_UI_ONLINE_ENABLED_ONLY       = 2;

        // ---- in-queue payload views (the queue hands out the header-stripped payload) ----
        struct ControllerButtonPayload : public CgsModule::Event
        {
            s32 miPadId;      // +0x00
            s32 miButtonId;   // +0x04 (the input action id)
        };

        // The gui-cache event (the assert text names its field mpCachePointer).
        struct GuiEventCachePayload : public CgsModule::Event
        {
            GuiCache* mpCachePointer;   // +0x00
        };

        // ---- out-queue wire records ------------------------------------------------------
        // { 1, 148, 12, <show byte> }, channel 40, 16 bytes: show / hide the HUD.
        struct GuiEventShowHideHudWire : public CgsGui::GuiEvent<148>
        {
            bool mbShowHud;   // +0x0C

            explicit GuiEventShowHideHudWire(bool lbShowHud)
                : CgsGui::GuiEvent<148>(
                      static_cast<u32>(sizeof(bool)),
                      static_cast<u32>(offsetof(GuiEventShowHideHudWire, mbShowHud)))
                , mbShowHud(lbShowHud)
            {
            }
        };

        // { 2072, 537, 12, the ticker message }, channel 40, 2084 bytes.
        typedef CgsGui::GuiEventWrapper<GuiEventTickerCustomMessage, 40> GuiEventTickerCustomMessageWire;
        // { 2, 536, 12, <two zero bytes> }, channel 40, 16 bytes.
        typedef CgsGui::GuiEventWrapper<GuiEventTickerClearMessages, 40> GuiEventTickerClearMessagesWire;

        static_assert(sizeof(GuiEventActivateCrashNav) == 20, "activate-crashnav record is 20 bytes");
        static_assert(sizeof(GuiEventShowHideHudWire) == 16, "show/hide-hud record is 16 bytes");
        static_assert(sizeof(GuiEventTickerCustomMessageWire) == 2084, "ticker record is 2084 bytes");
        static_assert(sizeof(GuiEventTickerClearMessagesWire) == 16, "ticker-clear record is 16 bytes");
    }

    // ================================================================================
    //  OnEnter
    // ================================================================================
    void OnlineQuickCustomCreate::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        mMainMenuComponent.Construct(KAC_MAIN_MENU_COMPONENT, mpStateInterface,
                                     E_MAIN_MENU_OPTIONS_COUNT, 0, KU64_NO_APT_ID);
        mNewNewsAnimation.Construct(KAC_NEW_NEWS_ANIMATION_COMPONENT, mpStateInterface, 0);

        mpGuiCache      = 0;
        meInternalState = E_INTERNALSTATE_GETCACHE;

        // The menu owns the screen: CrashNav down, HUD hidden.
        GuiEventActivateCrashNav lDeactivateCrashNav(false);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lDeactivateCrashNav), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lDeactivateCrashNav)));

        const GuiEventShowHideHudWire lHideHud(false);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lHideHud), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lHideHud)));

        mhNotifyListener = XNotifyCreateListener(KU64_NOTIFY_AREA_SYSTEM);
    }

    // ================================================================================
    //  OnLeave
    // ================================================================================
    void OnlineQuickCustomCreate::OnLeave()
    {
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // The console inlines StateInterface::PlayAptMovie here: the empty name at level 3
        // clears the level.
        mpStateInterface->PlayAptMovie(KPC_EMPTY_STRING, KI_APT_MOVIE_LEVEL);

        meInternalState = E_INTERNALSTATE_LEFT;
        mMainMenuComponent.Clear();

        GuiEventTickerClearMessages lClearTicker = {};
        const GuiEventTickerClearMessagesWire lClearTickerWire(lClearTicker);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lClearTickerWire), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lClearTickerWire)));

        if (mhNotifyListener != 0)
        {
            CloseHandle(mhNotifyListener);
            mhNotifyListener = 0;
        }
    }

    // ================================================================================
    //  Update -- each step stores its state on entry and drops into the next one once it
    //  completes. The sign-in wait polls the guide. The permanent handlers then run, the
    //  in-queue is cleared and the menu updates.
    // ================================================================================
    void OnlineQuickCustomCreate::Update()
    {
        switch (meInternalState)
        {
        case E_INTERNALSTATE_GETCACHE:
            UpdateGetCache();
            // fall through

        case E_INTERNALSTATE_LOADSCREEN:
            meInternalState = E_INTERNALSTATE_LOADSCREEN;
            if (!UpdateLoadResources())
            {
                break;
            }
            // fall through

        case E_INTERNALSTATE_LOADCOMPONENTS:
            meInternalState = E_INTERNALSTATE_LOADCOMPONENTS;
            if (!UpdateLoadComponents())
            {
                break;
            }
            // fall through

        case E_INTERNALSTATE_RUNNING:
            meInternalState = E_INTERNALSTATE_RUNNING;
            UpdateRunning();
            break;

        case E_INTERNALSTATE_LEFT:
            break;

        case E_SUBSTATE_WAIT_SIGN_IN_FINISH:
        {
            unsigned long luNotificationId = 0;
            unsigned long luNotificationParam = 0;
            if (XNotifyGetNext(mhNotifyListener, 0, &luNotificationId, &luNotificationParam) != 0)
            {
                if (luNotificationId == KU_NOTIFY_SYSTEM_UI)
                {
                    // The sign-in panel closed: open the friends list if the profile is now
                    // on the service, and go back to the menu either way.
                    if (luNotificationParam == 0)
                    {
                        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");
                        if (XUserGetSigninState(static_cast<u32>(mpGuiCache->GetActiveControllerIndex())) ==
                            KU_SIGNIN_STATE_SIGNED_IN_TO_LIVE)
                        {
                            XShowFriendsUI(static_cast<u32>(mpGuiCache->GetActiveControllerIndex()));
                        }
                        meInternalState = E_INTERNALSTATE_RUNNING;
                    }
                }
                else if (luNotificationId == KU_NOTIFY_LIVE_INVITE_ACCEPTED)
                {
                    CGS_ASSERT(false, "Invite notification processed by Online main menu screen. "
                                      "This could break cross game invites\n");
                }
            }
            break;
        }

        default:
            CGS_ASSERT(false, "Invalid internal state : ");
            break;
        }

        UpdatePermanent();

        reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue)->Clear();

        mMainMenuComponent.Update();
    }

    // ================================================================================
    //  UpdateGetCache -- take the cache from the first gui-cache event in the in-queue,
    //  flag the online game options for a reset and post the ticker line.
    // ================================================================================
    void OnlineQuickCustomCreate::UpdateGetCache()
    {
        CGS_ASSERT(0 == mpGuiCache, "NULL == mpGuiCache");

        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            if (liEventId == KI_EVENT_GUI_CACHE)
            {
                const GuiEventCachePayload* lpCacheEvent =
                    static_cast<const GuiEventCachePayload*>(lpEvent);
                CGS_ASSERT(0 != lpCacheEvent->mpCachePointer,
                           "NULL != lpCacheEvent->mpCachePointer");

                mpGuiCache = lpCacheEvent->mpCachePointer;
                mpGuiCache->SetResetOnlineGameOptions(true);

                GuiEventTickerCustomMessage lTicker;
                lTicker.Construct(true, false, true, false);

                const char* lpacTickerText = KAC_UNRANKED_TICKER_TEXT;
                if (mpGuiCache->GetDoJoinOnlineRankedGame())
                {
                    lpacTickerText = KAC_RANKED_TICKER_TEXT;
                }
                else if (mpGuiCache->GetDoJoinOnlineFreeburnGame())
                {
                    lpacTickerText = KAC_FREEBURN_TICKER_TEXT;
                }
                lTicker.AddString(lpacTickerText, KI_TICKER_STRING_TYPE);

                const GuiEventTickerCustomMessageWire lTickerWire(lTicker);
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lTickerWire), KI_CHANNEL_GUI_OUT,
                    static_cast<s32>(sizeof(lTickerWire)));
                break;
            }
        }

        CGS_ASSERT(0 != mpGuiCache, "NULL != mpGuiCache");
    }

    // ================================================================================
    //  UpdateLoadResources -- load the packages, play the menu and register its components.
    // ================================================================================
    bool OnlineQuickCustomCreate::UpdateLoadResources()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

        if (!mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad))
        {
            return false;
        }

        mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[KU_MAIN_MENU_MOVIE_RESOURCE],
                                       KI_APT_MOVIE_LEVEL);

        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
        mMainMenuComponent.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mNewNewsAnimation.GetName());
        return true;
    }

    // ================================================================================
    //  UpdateLoadComponents -- once every component is up, fill the three menu rows.
    // ================================================================================
    bool OnlineQuickCustomCreate::UpdateLoadComponents()
    {
        if (mpGuiCache == 0 || !mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
        {
            return false;
        }

        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);

        mMainMenuComponent.SetupMenu(E_MAIN_MENU_OPTIONS_COUNT, true);
        for (s32 liRow = 0; liRow < E_MAIN_MENU_OPTIONS_COUNT; ++liRow)
        {
            mMainMenuComponent.SetText(liRow, KAPC_MAIN_MENU_TEXT[liRow]);
        }

        mNewNewsAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME, KAC_INVISIBLE_STATE, false);
        return true;
    }

    // ================================================================================
    //  UpdateRunning -- controller presses.
    // ================================================================================
    void OnlineQuickCustomCreate::UpdateRunning()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            if (liEventId == KI_EVENT_CONTROLLER_INPUT)
            {
                HandleControllerInputPressed(lpEvent);
            }
        }
    }

    // ================================================================================
    //  UpdatePermanent -- disconnected latches the error for the popup and goes back.
    // ================================================================================
    void OnlineQuickCustomCreate::UpdatePermanent()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            if (liEventId == KI_EVENT_NETWORK_DISCONNECTED)
            {
                CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");
                mpGuiCache->SetDoDisconnectPopup(lpEvent);
                SendStateEvent(KAC_GO_BACK_EVENT);
            }
            else if (liEventId == KI_EVENT_COLLISION_WORLD)
            {
                // The collision-world request handler, inlined: only its event assert remains.
                CGS_ASSERT(lpEvent != 0, "lpEvent");
            }
        }
    }

    // ================================================================================
    //  HandleControllerInputPressed -- only the running menu takes input.
    // ================================================================================
    void OnlineQuickCustomCreate::HandleControllerInputPressed(const CgsModule::Event* lpEvent)
    {
        if (meInternalState != E_INTERNALSTATE_RUNNING)
        {
            return;
        }

        const ControllerButtonPayload* lpInput = static_cast<const ControllerButtonPayload*>(lpEvent);
        switch (lpInput->miButtonId)
        {
        case E_GAMEINPUTACTIONS_GUI_UP:
            mMainMenuComponent.HighlightPrevious();
            break;

        case E_GAMEINPUTACTIONS_GUI_DOWN:
            mMainMenuComponent.HighlightNext();
            break;

        case E_GAMEINPUTACTIONS_GUI_SELECT:
            ProcessSelectedMenuOption(
                static_cast<EMainMenuOptions>(mMainMenuComponent.GetHighlightedIndex()));
            break;

        case E_GAMEINPUTACTIONS_GUI_CANCEL:
            SendStateEvent(KAC_GO_BACK_EVENT);
            break;

        case E_GAMEINPUTACTIONS_GUI_OPTION1:
            ShowFriendsMenu();
            break;

        default:
            break;
        }
    }

    // ================================================================================
    //  ProcessSelectedMenuOption -- the picked row's state event.
    // ================================================================================
    void OnlineQuickCustomCreate::ProcessSelectedMenuOption(EMainMenuOptions leOption)
    {
        SendStateEvent(KAPC_MAIN_MENU_STATE_ACTIONS_TEXT[leOption]);
    }

    // ================================================================================
    //  ShowFriendsMenu -- the friends list needs a profile on the service; without one the
    //  sign-in panel goes up first and the sign-in wait takes over.
    // ================================================================================
    void OnlineQuickCustomCreate::ShowFriendsMenu()
    {
        if (mpGuiCache == 0)
        {
            return;
        }

        if (XUserGetSigninState(static_cast<u32>(mpGuiCache->GetActiveControllerIndex())) !=
            KU_SIGNIN_STATE_SIGNED_IN_TO_LIVE)
        {
            XShowSigninUI(KU_SIGNIN_UI_PANES, KU_SIGNIN_UI_ONLINE_ENABLED_ONLY);
            meInternalState = E_SUBSTATE_WAIT_SIGN_IN_FINISH;
            return;
        }

        XShowFriendsUI(static_cast<u32>(mpGuiCache->GetActiveControllerIndex()));
    }
}

// GameSource/Gui/Flow/Screen/States/BrnOnlineQuickMatch.cpp
//
// BrnGui::OnlineQuickMatch -- the online "quick match" screen (ON_QWK_MAT): Easy Drive's
// Freeburn -> Quick Match lands here after the sign-in screen. The twelve bodies, read
// store for store off the console asm:
//
//   OnEnter / OnLeave / Update               the lifecycle
//   CheckForCompletedLoads                   load ON_QWKM, then post the quick-match request
//                                            (GUI 251) and show "searching"
//   HandleGuiCacheEvent                      adopt the cache; leave when multiplayer is not
//                                            allowed, else register the expected components
//   HandleSearchResultsEvent                 the search found nothing: "no games found"
//   HandleControllerInput(+NoGames/+InGame)  the "search again?" / OK pages
//   ShowMessage / ShowNoGamesFound(+InGame)  the message-panel dressings
//
// The network answers GUI 50 once the quick match has put the player in a game; the
// screen then raises the CrashNav flow again and advances (the FSM edge goes to the game
// room). GUI 44 (disconnected) latches the error for the popup and takes the DISCONNECT edge.
//
// Out-queue records are the console's own wire records, posted on channel 40 through
// GetOutputEventQueue()->AddEvent with host sizeof sizes (the committed OutputGuiEvent
// template does not use channel 40; see CgsGuiStateInterface.h).

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineQuickMatch.h"

#include <cstddef>                                                        // offsetof (wire records)
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N> / GuiEventWrapper
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface / GuiEventNetworkSuspension
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // the state in-queue / AddEvent
#include "GameShared/GameClasses/System/PC/BrnNetHarnessPC.h"             // [netui] witness lines (LAN / harness only)
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                     // GuiEventNetworkQuickMatch / GuiEventNetworkCustomMatchResults
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiEventActivateCrashNav, GuiFlow

namespace BrnGui
{
    // ================================================================================
    //  Class statics (values read from the image)
    // ================================================================================

    // The events the screen observes: 14, apt ONLOAD, controller press, gui cache, the
    // search results, in-game, disconnected.
    const s32 OnlineQuickMatch::maiEventToObserve[7] = { 14, 21, 6, 64, 254, 50, 44 };
    const s32 OnlineQuickMatch::miNumEventsObserved  = 7;

    // The one apt package the screen loads (ON_QWKM).
    const CgsGui::sResourceTuple OnlineQuickMatch::maResourceTuplesToLoad[] =
    {
        { 174, CgsGui::E_GUI_RESOURCETYPE_APT }
    };
    const s32 OnlineQuickMatch::miNumResourcesToLoad = 1;

    const char OnlineQuickMatch::KAC_MESSAGE_BUTTONS_COMPONENT[7]            = "Button";
    const char OnlineQuickMatch::KAC_MESSAGE_TEXT_COMPONENT[12]              = "MessageText";
    const char OnlineQuickMatch::KAC_MESSAGE_ANIMATION_COMPONENT[22]         = "MessageTextTransition";
    const char OnlineQuickMatch::KAC_MESSAGE_BUTTONS_ANIMATION_COMPONENT[24] = "ButtonPromptsTransition";
    const char OnlineQuickMatch::KAC_NO_GAMES_FOUND_STRING_ID[29]            = "$ONLINE_GAME_SEARCH_NO_GAMES";
    const char OnlineQuickMatch::KAC_SEARCHING_STRING_ID[30]                 = "$ONLINE_GAME_SEARCH_SEARCHING";

    const char* const OnlineQuickMatch::KAPC_ANIMATION_STATES[2] =
    {
        "Visible", "Invisible"
    };
    const char* const OnlineQuickMatch::KAPC_YES_NO_BUTTON_STRING_ID[2] =
    {
        "$GENERAL_OPTION_YES", "$GENERAL_OPTION_NO"
    };
    const char* const OnlineQuickMatch::KAPC_OK_BUTTON_STRING_ID[1] =
    {
        "$GENERAL_OPTION_OK"
    };

    namespace
    {
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;   // mpInGuiEventQueue's real type

        const s32 KI_CHANNEL_GUI_OUT = 40;

        // The observed events Update dispatches on.
        const s32 KI_EVENT_CONTROLLER_INPUT      = 6;
        const s32 KI_EVENT_OBSERVED_NO_ARM_14    = 14;
        const s32 KI_EVENT_APT_ONLOAD            = 21;   // observed, no arm
        const s32 KI_EVENT_NETWORK_DISCONNECTED  = 44;
        const s32 KI_EVENT_ENTER_GAME            = 50;
        const s32 KI_EVENT_GUI_CACHE             = 64;
        const s32 KI_EVENT_CUSTOM_MATCH_RESULTS  = 254;

        // The controller actions the two message pages react to (the press payload's
        // second word).
        const s32 KI_ACTION_GUI_UP     = 41;
        const s32 KI_ACTION_GUI_DOWN   = 42;
        const s32 KI_ACTION_GUI_SELECT = 49;
        const s32 KI_ACTION_GUI_CANCEL = 50;

        // Row 0 of the two message menus: YES on the "search again?" page, OK on the
        // in-game page (the assert text names it E_OK_BUTTON_OK).
        const s32 KI_MESSAGE_BUTTON_AFFIRMATIVE = 0;

        const s32 KI_NUM_MESSAGE_BUTTONS = 2;
        // The "no apt id" value the button menu is constructed with (the 32-bit -1,
        // zero-extended).
        const u64 KU64_NO_APT_ID = 0xFFFFFFFFull;

        const char KAC_QUICK_MATCH_MOVIE[]   = "ON_QWKM";   // the pooled movie-name literal
        const s32  KI_APT_MOVIE_LEVEL        = 3;
        const char* const KPC_EMPTY_STRING   = "";

        const char KAC_APT_TRANSITION_NAME[] = "apt_Transition";
        const s32  KI_ANIMATION_STATE_VISIBLE   = 0;
        const s32  KI_ANIMATION_STATE_INVISIBLE = 1;

        const char KAC_ADVANCE_EVENT[]      = "ADVANCE";
        const char KAC_DISCONNECT_EVENT[]   = "DISCONNECT";
        const char KAC_GO_BACK_EVENT[]      = "GO_BACK";
        const char KAC_GO_BACK_EASY_EVENT[] = "GO_BACK_EASY";
        const char KAC_TO_GAME_OPT_EVENT[]  = "TO_GAME_OPT";

        // ---- in-queue payload views (the queue hands out the header-stripped payload) ----
        struct ControllerButtonPayload : public CgsModule::Event
        {
            s32 miPadId;      // +0x00
            s32 miButtonId;   // +0x04 (the input action id)
        };

        struct GuiCachePayload : public CgsModule::Event
        {
            GuiCache* mpCache;   // +0x00
        };

        // ---- out-queue wire records ----------------------------------------------------
        // { 1, 253, 12, <one byte> }, channel 40, 16 bytes: stop the search. The console
        // never writes the payload byte (zero here).
        struct CustomMatchSearchStopWire : public CgsGui::GuiEvent<253>
        {
            u8 muUnwrittenPayload;   // +0x0C

            CustomMatchSearchStopWire()
                : CgsGui::GuiEvent<253>(
                      static_cast<u32>(sizeof(u8)),
                      static_cast<u32>(offsetof(CustomMatchSearchStopWire, muUnwrittenPayload)))
                , muUnwrittenPayload(0)
            {
            }
        };

        // { 1, 533, 12, <one byte> }, channel 40, 16 bytes -- the record the screens post
        // when they hand the front end back (the pause screen and the online play screen
        // post the same one). The console never writes the payload byte (zero here).
        struct GuiEvent533Wire : public CgsGui::GuiEvent<533>
        {
            u8 muUnwrittenPayload;   // +0x0C

            GuiEvent533Wire()
                : CgsGui::GuiEvent<533>(
                      static_cast<u32>(sizeof(u8)),
                      static_cast<u32>(offsetof(GuiEvent533Wire, muUnwrittenPayload)))
                , muUnwrittenPayload(0)
            {
            }
        };

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

        // { 2, 251, 12, ranked, freeburn }, channel 40, 16 bytes: the quick-match request.
        typedef CgsGui::GuiEventWrapper<GuiEventNetworkQuickMatch, 40> GuiEventNetworkQuickMatchWire;

        static_assert(sizeof(CustomMatchSearchStopWire) == 16, "search-stop record is 16 bytes");
        static_assert(sizeof(GuiEvent533Wire) == 16, "id-533 record is 16 bytes");
        static_assert(sizeof(GuiEventShowHideHudWire) == 16, "show/hide-hud record is 16 bytes");
        static_assert(sizeof(GuiEventNetworkQuickMatchWire) == 16, "quick-match record is 16 bytes");
        static_assert(sizeof(GuiEventActivateCrashNav) == 20, "activate-crashnav record is 20 bytes");
        static_assert(sizeof(CgsGui::GuiEventNetworkSuspension) == 16, "suspension record is 16 bytes");
    }

    // ================================================================================
    //  OnEnter
    // ================================================================================
    void OnlineQuickMatch::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        mMessageAnimation.Construct(KAC_MESSAGE_ANIMATION_COMPONENT, mpStateInterface, 0);
        mMessageButtonsAnimation.Construct(KAC_MESSAGE_BUTTONS_ANIMATION_COMPONENT, mpStateInterface, 0);
        mMessageButtons.Construct(KAC_MESSAGE_BUTTONS_COMPONENT, mpStateInterface,
                                  KI_NUM_MESSAGE_BUTTONS, 0, KU64_NO_APT_ID);
        mMessageText.Construct(KAC_MESSAGE_TEXT_COMPONENT, mpStateInterface, 0);

        meSubState = E_SUBSTATE_LOADING_SCREEN;
        mpGuiCache = 0;
        mMessageText.SetAutoSize(true);

        // The quick-match page owns the screen: CrashNav down, HUD hidden.
        GuiEventActivateCrashNav lDeactivateCrashNav(false);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lDeactivateCrashNav), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lDeactivateCrashNav)));

        const GuiEventShowHideHudWire lHideHud(false);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lHideHud), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lHideHud)));
    }

    // ================================================================================
    //  OnLeave
    // ================================================================================
    void OnlineQuickMatch::OnLeave()
    {
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // The console inlines StateInterface::PlayAptMovie here: the empty name at level 3
        // clears the level ({ 8, 18, 12, name, level } on channel 41).
        mpStateInterface->PlayAptMovie(KPC_EMPTY_STRING, KI_APT_MOVIE_LEVEL);

        // Stop any search still in flight.
        const CustomMatchSearchStopWire lStop;
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lStop), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lStop)));

        if (mpGuiCache != 0)
        {
            mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
        }
    }

    // ================================================================================
    //  Update
    // ================================================================================
    void OnlineQuickMatch::Update()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            switch (liEventId)
            {
            case KI_EVENT_CONTROLLER_INPUT:
                HandleControllerInput(lpEvent);
                break;

            case KI_EVENT_OBSERVED_NO_ARM_14:
            case KI_EVENT_APT_ONLOAD:
                break;

            case KI_EVENT_GUI_CACHE:
                HandleGuiCacheEvent(lpEvent);
                break;

            case KI_EVENT_ENTER_GAME:
            {
                // The quick match put the player in a game: raise the CrashNav flow again,
                // hand the front end back and take the ADVANCE edge.
                GuiEventActivateCrashNav lActivateCrashNav(true);
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lActivateCrashNav), KI_CHANNEL_GUI_OUT,
                    static_cast<s32>(sizeof(lActivateCrashNav)));

                const GuiEvent533Wire lHandBack;
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lHandBack), KI_CHANNEL_GUI_OUT,
                    static_cast<s32>(sizeof(lHandBack)));

                BrnNetHarnessPC::WitnessTag("netui", "quick-match", "in game (GUI 50) -> ADVANCE");
                SendStateEvent(KAC_ADVANCE_EVENT);
                break;
            }

            case KI_EVENT_CUSTOM_MATCH_RESULTS:
                // The results record is headerless: the payload IS the record.
                HandleSearchResultsEvent(reinterpret_cast<const GuiEventNetworkCustomMatchResults*>(lpEvent));
                break;

            case KI_EVENT_NETWORK_DISCONNECTED:
                CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");
                // A null event still latches (0).
                mpGuiCache->SetDoDisconnectPopup(lpEvent);
                BrnNetHarnessPC::WitnessTag("netui", "quick-match", "disconnected (GUI 44) -> DISCONNECT");
                SendStateEvent(KAC_DISCONNECT_EVENT);
                break;

            default:
                // The console streams the event id and its file/line (line 208) after this
                // text; lowered to the static text.
                CGS_ASSERT(false, "Unexpected event received : ");
                break;
            }
        }

        lpInQueue->Clear();

        CheckForCompletedLoads();

        // Component vtable slot 5 on the button menu.
        mMessageButtons.Update();
    }

    // ================================================================================
    //  CheckForCompletedLoads
    //
    //  Two sub-states: wait for the apt package, then for the components HandleGuiCacheEvent
    //  registered. Once both are in, the quick-match request goes out and the "searching"
    //  message goes up.
    // ================================================================================
    void OnlineQuickMatch::CheckForCompletedLoads()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

        if (meSubState == E_SUBSTATE_LOADING_SCREEN)
        {
            if (mpGuiCache->EnsureResourcesAreLoaded(maResourceTuplesToLoad,
                                                     static_cast<u32>(miNumResourcesToLoad)))
            {
                mpStateInterface->PlayAptMovie(KAC_QUICK_MATCH_MOVIE, KI_APT_MOVIE_LEVEL);
                meSubState = E_SUBSTATE_LOADING_COMPONENTS;
            }
        }
        else if (meSubState == E_SUBSTATE_LOADING_COMPONENTS)
        {
            // The console re-tests the cache after the assert (the assert does not stop).
            if (mpGuiCache != 0 && mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
            {
                mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);

                CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

                // Both flags go out clear when the player is already online (the byte the
                // cache keeps at +0x4B4C); otherwise they are the flags the online menu set.
                GuiEventNetworkQuickMatch lQuickMatch;
                lQuickMatch.mbRanked   = !mpGuiCache->IsOnline() && mpGuiCache->GetDoJoinOnlineRankedGame();
                lQuickMatch.mbFreeburn = !mpGuiCache->IsOnline() && mpGuiCache->GetDoJoinOnlineFreeburnGame();

                GuiEventNetworkQuickMatchWire lQuickMatchWire(lQuickMatch);
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lQuickMatchWire), KI_CHANNEL_GUI_OUT,
                    static_cast<s32>(sizeof(lQuickMatchWire)));
                BrnNetHarnessPC::WitnessTag("netui", "quick-match", "post 251 ranked=%d freeburn=%d",
                                            lQuickMatch.mbRanked ? 1 : 0, lQuickMatch.mbFreeburn ? 1 : 0);

                meSubState = E_SUBSTATE_SEARCHING;
                ShowMessage(KAC_SEARCHING_STRING_ID);
            }
        }
    }

    // ================================================================================
    //  CheckPrivileges -- inlined into HandleGuiCacheEvent on the console.
    // ================================================================================
    bool OnlineQuickMatch::CheckPrivileges()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");
        return mpGuiCache->IsMultiplayerAllowed();
    }

    // ================================================================================
    //  HandleGuiCacheEvent
    //
    //  The first cache offered is adopted. Without multiplayer privileges the screen backs
    //  straight out (lifting the network suspension a pending online start put in place);
    //  otherwise it registers the apt components it will wait for.
    // ================================================================================
    void OnlineQuickMatch::HandleGuiCacheEvent(const CgsModule::Event* lpEvent)
    {
        const GuiCachePayload* lpPayload = reinterpret_cast<const GuiCachePayload*>(lpEvent);

        // Streamed on the console; non-fatal (the adoption below runs either way).
        CGS_ASSERT(lpPayload->mpCache != 0, "Invalid cache in HandleGuiCacheEvent::Update");

        if (mpGuiCache != 0)
        {
            return;
        }

        mpGuiCache = lpPayload->mpCache;

        if (!CheckPrivileges())
        {
            if (mpGuiCache->IsOnlineStartPending())
            {
                CgsGui::GuiEventNetworkSuspension lNetworkSuspension(false);
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lNetworkSuspension), KI_CHANNEL_GUI_OUT,
                    static_cast<s32>(sizeof(lNetworkSuspension)));

                mpGuiCache->SetOnlineStartPending(false);
                SendStateEvent(KAC_GO_BACK_EASY_EVENT);
            }
            else
            {
                SendStateEvent(KAC_GO_BACK_EVENT);
            }
            return;
        }

        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
        mMessageButtons.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mMessageText.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mMessageAnimation.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mMessageButtonsAnimation.GetName());
    }

    // ================================================================================
    //  HandleSearchResultsEvent
    //
    //  A quick match that joined a game answers with GUI 50, so results only arrive here
    //  when the search found nothing.
    // ================================================================================
    void OnlineQuickMatch::HandleSearchResultsEvent(const GuiEventNetworkCustomMatchResults* lpSearchResults)
    {
        CGS_ASSERT(lpSearchResults != 0, "Invalid event in HandleSearchResultsEvent");
        CGS_ASSERT(lpSearchResults->miNumGames == 0,
                   "Should only get this in quickmatch if the search failed!");

        if (mpGuiCache->IsOnline())
        {
            ShowNoGamesFoundInGame();
        }
        else
        {
            ShowNoGamesFound();
        }
    }

    // ================================================================================
    //  HandleControllerInput -- only the two "no games found" pages take presses.
    // ================================================================================
    void OnlineQuickMatch::HandleControllerInput(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0, "Invalid event sent to OnlineQuickMatch::HandleControllerInput");

        if (meSubState == E_SUBSTATE_NO_GAMES_FOUND)
        {
            HandleControllerInputNoGames(lpEvent);
        }
        else if (meSubState == E_SUBSTATE_NO_GAMES_FOUND_IN_GAME)
        {
            HandleControllerInputNoGamesInGame(lpEvent);
        }
    }

    // ================================================================================
    //  HandleControllerInputNoGames -- "no games found, search again?" (YES / NO).
    //
    //  YES goes to the game options screen; NO and cancel both back out.
    // ================================================================================
    void OnlineQuickMatch::HandleControllerInputNoGames(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0, "Invalid event sent to OnlineQuickMatch::HandleControllerInputNoGames");

        const ControllerButtonPayload* lpInput = reinterpret_cast<const ControllerButtonPayload*>(lpEvent);

        switch (lpInput->miButtonId)
        {
        case KI_ACTION_GUI_UP:
            mMessageButtons.HighlightPrevious();   // virtual slot +0x38
            break;

        case KI_ACTION_GUI_DOWN:
            mMessageButtons.HighlightNext();       // virtual slot +0x34
            break;

        case KI_ACTION_GUI_SELECT:
            if (mMessageButtons.GetHighlightedIndex() == KI_MESSAGE_BUTTON_AFFIRMATIVE)
            {
                SendStateEvent(KAC_TO_GAME_OPT_EVENT);
                break;
            }
            // NO leaves the way cancel does (the console branches into the cancel arm).
            // fall through

        case KI_ACTION_GUI_CANCEL:
            if (mpGuiCache->IsOnlineStartPending())
            {
                CgsGui::GuiEventNetworkSuspension lNetworkSuspension(false);
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lNetworkSuspension), KI_CHANNEL_GUI_OUT,
                    static_cast<s32>(sizeof(lNetworkSuspension)));

                mpGuiCache->SetOnlineStartPending(false);
                SendStateEvent(KAC_GO_BACK_EASY_EVENT);
            }
            else
            {
                SendStateEvent(KAC_GO_BACK_EVENT);
            }
            break;

        default:
            break;
        }
    }

    // ================================================================================
    //  HandleControllerInputNoGamesInGame -- "no games found" with a single OK button.
    //
    //  Select asserts that OK is highlighted and leaves exactly the way cancel does. The
    //  assert texts are the console's own, the first one naming the custom-match screen.
    // ================================================================================
    void OnlineQuickMatch::HandleControllerInputNoGamesInGame(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event sent to OnlineCustomMatch::HandleControllerInputNoGamesInGame");

        const ControllerButtonPayload* lpInput = reinterpret_cast<const ControllerButtonPayload*>(lpEvent);

        switch (lpInput->miButtonId)
        {
        case KI_ACTION_GUI_UP:
            mMessageButtons.HighlightPrevious();   // virtual slot +0x38
            break;

        case KI_ACTION_GUI_DOWN:
            mMessageButtons.HighlightNext();       // virtual slot +0x34
            break;

        case KI_ACTION_GUI_SELECT:
            CGS_ASSERT(mMessageButtons.GetHighlightedIndex() == KI_MESSAGE_BUTTON_AFFIRMATIVE,
                       "mMessageButtons.GetHighlightedIndex() == E_OK_BUTTON_OK");
            // fall through

        case KI_ACTION_GUI_CANCEL:
            if (mpGuiCache->IsOnlineStartPending())
            {
                CgsGui::GuiEventNetworkSuspension lNetworkSuspension(false);
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lNetworkSuspension), KI_CHANNEL_GUI_OUT,
                    static_cast<s32>(sizeof(lNetworkSuspension)));

                mpGuiCache->SetOnlineStartPending(false);
                SendStateEvent(KAC_GO_BACK_EASY_EVENT);
            }
            else
            {
                SendStateEvent(KAC_GO_BACK_EVENT);
            }
            break;

        default:
            break;
        }
    }

    // ================================================================================
    //  ShowMessage -- the message text alone: the button prompts go away.
    // ================================================================================
    void OnlineQuickMatch::ShowMessage(const char* lpacMessage)
    {
        mMessageButtonsAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                       KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_INVISIBLE], false);
        mMessageAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_VISIBLE], false);
        mMessageText.SetText(lpacMessage);
    }

    // ================================================================================
    //  ShowNoGamesFound -- "no games found" with the YES / NO pair.
    // ================================================================================
    void OnlineQuickMatch::ShowNoGamesFound()
    {
        mMessageAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_VISIBLE], false);
        mMessageButtonsAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                       KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_VISIBLE], false);
        mMessageText.SetText(KAC_NO_GAMES_FOUND_STRING_ID);

        mMessageButtons.SetupMenu(KI_NUM_MESSAGE_BUTTONS, false);
        for (s32 li = 0; li < KI_NUM_MESSAGE_BUTTONS; ++li)
        {
            mMessageButtons.SetText(li, KAPC_YES_NO_BUTTON_STRING_ID[li]);
        }

        meSubState = E_SUBSTATE_NO_GAMES_FOUND;
    }

    // ================================================================================
    //  ShowNoGamesFoundInGame -- "no games found" with a single OK button.
    // ================================================================================
    void OnlineQuickMatch::ShowNoGamesFoundInGame()
    {
        mMessageAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_VISIBLE], false);
        mMessageButtonsAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                       KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_VISIBLE], false);
        mMessageText.SetText(KAC_NO_GAMES_FOUND_STRING_ID);

        mMessageButtons.SetupMenu(1, false);
        mMessageButtons.SetText(0, KAPC_OK_BUTTON_STRING_ID[0]);

        meSubState = E_SUBSTATE_NO_GAMES_FOUND_IN_GAME;
    }
}

#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameSource/Gui/BrnGuiTextField.h"                              // BrnGui::TextField (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h" // BrnGui::AnimationComponent (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuComponent.h"      // BrnGui::MenuComponent (by value)

// BrnGui::OnlineQuickMatch - the online "quick match" flow state (ON_QWK_MAT). It loads
// its apt movie, posts the quick-match request (GUI 251) to the network, shows a
// "searching" message and advances to the game room once the network reports the game
// joined (GUI 50). A failed search offers "search again?" (or, when the search was started
// from inside a game, a plain OK).
// The base derivation (CgsGui::State), the member names and order and the virtual set are
// the original declaration's; member placement is the console's.
namespace CgsModule { struct Event; }

namespace BrnGui
{
    class GuiCache;   // pointer member only
    struct GuiEventNetworkCustomMatchResults;

    struct OnlineQuickMatch : public CgsGui::State
    {
        // The screen's sub-state machine (console +0x38).
        enum ESubState
        {
            E_SUBSTATE_LOADING_SCREEN          = 0,
            E_SUBSTATE_LOADING_COMPONENTS      = 1,
            E_SUBSTATE_SEARCHING               = 2,
            E_SUBSTATE_NO_GAMES_FOUND          = 3,
            E_SUBSTATE_NO_GAMES_FOUND_IN_GAME  = 4,
            E_SUBSTATE_COUNT                   = 5,
        };

        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @ 0x825004E0 - hands this screen's static resource list to the loader
        // (X360: *r4 = &maResourceTuplesToLoad; *r5 = miNumResourcesToLoad, count = 1).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourceTuplesToLoad;
            *lpuNumberOfResources = (u32)miNumResourcesToLoad;
        }

    private:
        // The in-queue hands the handlers the header-stripped payload, so the controller
        // and gui-cache handlers take the bare event (declared over GuiEventControllerInputPressed /
        // GuiEventCache); the search-results record is headerless, so it keeps its type.
        void HandleControllerInput(const CgsModule::Event* lpEvent);
        void HandleControllerInputNoGames(const CgsModule::Event* lpEvent);
        void HandleControllerInputNoGamesInGame(const CgsModule::Event* lpEvent);
        void HandleSearchResultsEvent(const GuiEventNetworkCustomMatchResults* lpSearchResults);
        void HandleGuiCacheEvent(const CgsModule::Event* lpEvent);
        void CheckForCompletedLoads();
        void ShowMessage(const char* lpacMessage);
        void ShowNoGamesFound();
        void ShowNoGamesFoundInGame();
        // No out-of-line copy on the console: HandleGuiCacheEvent inlines it (its assert
        // fires at the console's line 655).
        bool CheckPrivileges();

        static const s32                    maiEventToObserve[7];
        static const s32                    miNumEventsObserved;      // == 7
        static const CgsGui::sResourceTuple maResourceTuplesToLoad[]; // @ 0x8205F71C (unk_8205F71C, .rdata)
        static const s32                    miNumResourcesToLoad;     // @ 0x8205F724 (dword_8205F724, .rdata) == 1
        static const char KAC_MESSAGE_BUTTONS_COMPONENT[7];           // "Button"
        static const char KAC_MESSAGE_TEXT_COMPONENT[12];             // "MessageText"
        static const char KAC_MESSAGE_ANIMATION_COMPONENT[22];        // "MessageTextTransition"
        static const char KAC_MESSAGE_BUTTONS_ANIMATION_COMPONENT[24];// "ButtonPromptsTransition"
        static const char KAC_NO_GAMES_FOUND_STRING_ID[29];           // "$ONLINE_GAME_SEARCH_NO_GAMES"
        static const char KAC_SEARCHING_STRING_ID[30];                // "$ONLINE_GAME_SEARCH_SEARCHING"
        static const char* const KAPC_ANIMATION_STATES[2];            // { "Visible", "Invisible" }
        static const char* const KAPC_YES_NO_BUTTON_STRING_ID[2];
        static const char* const KAPC_OK_BUTTON_STRING_ID[1];

        // ---- data members (declaration order; console offsets in the comments) -------
        ESubState          meSubState;                  // +0x38
        AnimationComponent mMessageAnimation;           // +0x3C   "MessageTextTransition"
        AnimationComponent mMessageButtonsAnimation;    // +0xC8   "ButtonPromptsTransition"
        MenuComponent      mMessageButtons;             // +0x158  "Button" (2 rows)
        TextField          mMessageText;                // +0x1218 "MessageText"
        GuiCache*          mpGuiCache;                  // +0x1340
    };
}

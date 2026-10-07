#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameSource/Gui/BrnGuiTextField.h"                                // by value
#include "GameSource/Gui/Flow/Screen/Components/BrnGuiNetworkRouteInfo.h"  // by value
#include "GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h"  // by value
#include "GameSource/Gui/Flow/Shared/Components/BrnHelpBar.h"             // by value
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuComponent.h"       // by value

// BrnGui::OnlineGameOptionsSummary - the online game-options summary screen flow state
// (ON_CRSUM): the route info for the chosen options, round by round, with "done" (create
// the game) and "save" (file the options in one of the saved slots).
// The base derivation (CgsGui::State), the member names and order and the virtual set are
// the original declaration's; member placement is the console's.
namespace CgsModule { struct Event; }

namespace BrnGui
{
    class GuiCache;   // pointer member only

    struct OnlineGameOptionsSummary : public CgsGui::State
    {
        // The screen's sub-state machine (console +0x38).
        enum ESubState
        {
            E_SUBSTATE_LOADING_SCREEN     = 0,
            E_SUBSTATE_LOADING_COMPONENTS = 1,
            E_SUBSTATE_SELECTING_PARAMS   = 2,
            E_SUBSTATE_SAVE_OPTIONS       = 3,
            E_SUBSTATE_WAIT_IN_GAME       = 4,
            E_SUBSTATE_COUNT              = 5,
        };

        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @ 0x8251B038 - hands the online-game-options-summary state's static resource list
        // to the loader (X360: *r4 = &maResourceTuplesToLoad; *r5 = miNumResourcesToLoad).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourceTuplesToLoad;
            *lpuNumberOfResources = static_cast<u32>(miNumResourcesToLoad);
        }

    private:
        static const s32 KI_MAX_CREATE_GAME_OPTIONS = 7;
        static const s32 KI_NUM_MAIN_MENU_OPTIONS   = 2;
        static const s32 KI_MAX_HELP_BAR_ITEMS      = 2;

        // The in-queue hands the handlers the header-stripped payload, so they take the
        // bare event (declared over GuiEventControllerInputPressed / GuiEventCache /
        // GuiEventNetworkInGameFailed / GuiOverlayCompleteEvent).
        void HandleControllerInput(const CgsModule::Event* lpEvent);
        void HandleControllerInputCreateGame(const CgsModule::Event* lpEvent);
        void HandleControllerInputSaveOptions(const CgsModule::Event* lpEvent);
        void HandleInGameEvent(const CgsModule::Event* lpInGameEvent);
        void HandleInGameFailedEvent(const CgsModule::Event* lpEvent);
        void HandleGuiCacheEvent(const CgsModule::Event* lpEvent);
        void HandleOverlayComplete(const CgsModule::Event* lpOverlayCompleteEvent);
        void CheckForCompletedLoads();
        void SetupHelpBar(bool lbUnused);
        void ShowGameOptionsScreen();
        void ShowSaveScreen();
        void SetupSaveMenuText();
        void FinishedCreatingGame();

        static const s32                    maiEventToObserve[9];
        static const s32                    miNumEventsObserved;      // == 9
        static const CgsGui::sResourceTuple maResourceTuplesToLoad[]; // @ 0x8205F1FC (.rdata, 2 entries)
        static const s32                    miNumResourcesToLoad;     // @ 0x8205F20C (.rdata, == 2)
        static const char                   KAC_TITLE_TEXT_COMPONENT[11];          // "Title_text"
        static const char                   KAC_MENU_OPTIONS_COMPONENT[9];         // "MenuItem"
        static const char                   KAC_ROUTE_INFO_NAME[10];               // "RouteInfo"
        static const char                   KAC_UP_ARROW_COMPONENT[13];            // "ArrowUp_anim"
        static const char                   KAC_DOWN_ARROW_COMPONENT[15];          // "ArrowDown_anim"
        static const char                   KAC_HELP_BAR_COMPONENT[7];             // "Button"
        static const char* const            KPC_ARROW_ANIMATION_STATES[3];         // invisible / visible / animate
        static const char                   KAC_SUMMARY_TITLE_STRING_ID[34];
        static const char                   KAC_SAVE_OPTIONS_TITLE_STRING_ID[32];
        static const char* const            KPC_MAIN_MENU_OPTION_STRING_IDS[KI_NUM_MAIN_MENU_OPTIONS];
        static const char                   KPC_EMPTY_SLOT_STRING_IDS[30];         // "ONLINE_GAME_OPTION_EMPTY_SLOT"
        static const char                   KPC_SLOT_STRING_FORMAT_ID[24];         // "ONLINE_GAME_OPTION_SLOT"
        static const char                   KPC_SLOT_STRING_ID[28];                // "$ONLINE_GAME_OPTION_SLOT_%d"

        // ---- data members (declaration order; console offsets in the comments) -------
        ESubState           meSubState;           // +0x38
        MenuComponent       mMenuOptions;         // +0x40
        AnimationComponent  mUpArrowAnimator;     // +0x1100
        AnimationComponent  mDownArrowAnimator;   // +0x118C
        GuiNetworkRouteInfo mRouteInfoDisplay;    // +0x1220
        TextField           mTitleText;           // +0x33D0
        HelpBar             mHelpBar;             // +0x3500
        GuiCache*           mpGuiCache;           // +0x5350
        s32                 miCurrentRound;       // +0x5354
        s32                 miStartSaveItem;      // +0x5358
    };
}

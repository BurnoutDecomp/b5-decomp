#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameSource/Gui/BrnGuiTextField.h"                                       // by value
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                             // GuiEventNetworkLobbyPlayerList (by value)
#include "GameSource/Gui/Events/BrnGuiEventNetworkGameParams.h"                   // by value
#include "GameSource/Gui/Flow/Screen/Components/BrnGuiNetworkRouteInfo.h"         // by value
#include "GameSource/Gui/Flow/Screen/Components/BrnOnlineLoadingPlayer.h"         // by value
#include "GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h"         // by value

// BrnGui::OnlineLoading - the online loading / player-list screen flow state (ON_LOAD): the
// title for the game mode, the eight player rows, the route info for the current round,
// and the hand-over to the game once the loading screen has been up long enough.
// The base derivation (CgsGui::State), the member names and order and the virtual set are
// the original declaration's; member placement is the console's, which also carries the
// title text field and the chevron-colour clip.
namespace CgsModule { struct Event; }

namespace BrnGui
{
    class GuiCache;   // pointer member only

    struct OnlineLoading : public CgsGui::State
    {
        // The screen's sub-state machine (console +0x4AF0).
        enum ESubState
        {
            E_SUBSTATE_LOADING_SCREEN        = 0,
            E_SUBSTATE_LOADING_COMPONENTS    = 1,
            E_SUBSTATE_LOADING_ROUTE_INFO    = 2,
            E_SUBSTATE_MAIN                  = 3,
            E_SUBSTATE_WAIT_GAME_MODE_START  = 4,
            E_SUBSTATE_COUNT                 = 5,
        };

        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @ 0x82515308 - hands the online-loading state's static resource list to the loader
        // (X360: *r4 = &maResourceTuplesToLoad; *r5 = miNumResourcesToLoad).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourceTuplesToLoad;
            *lpuNumberOfResources = static_cast<u32>(miNumResourcesToLoad);
        }

    private:
        static const s32 KI_MAX_PLAYERS = 8;

        void SetupGameModeInfo();
        void SetupPlayerList();
        void UpdatePlayerList();
        void CheckForCompletedLoads();
        // The in-queue hands the handlers the header-stripped payload, so they take the
        // bare event (declared over GuiEventCache / GuiEventAptTrigger / GuiEventInviteFailed).
        void HandleGuiCacheEvent(const CgsModule::Event* lpEvent);
        void HandleAptTriggers(const CgsModule::Event* lpEvent);
        void HandlePlayerLobbyListEvent(const CgsModule::Event* lpEvent);
        void HandleGameParamsChangedEvent(const CgsModule::Event* lpEvent);
        void HandleInviteFailed(const CgsModule::Event* lpInviteFailedEvent);

        static const s32                    maiEventToObserve[11];
        static const s32                    miNumEventsObserved;      // == 11
        static const CgsGui::sResourceTuple maResourceTuplesToLoad[]; // @ 0x8205EE70 (.rdata, 2 entries)
        static const s32                    miNumResourcesToLoad;     // @ 0x8205EE80 (.rdata, == 2)
        static const f32                    KF_LOADING_SCREEN_UPDATE_TIMEOUT;
        static const char                   KAC_PLAYER_TEMPLATE[13];            // "Player_%i_mc"
        static const char                   KAC_ROUTE_INFO_NAME[10];            // "RouteInfo"
        static const char                   KAC_ANIMATION_COMPONENT_NAME[15];   // "PlayersHeading"
        static const char* const            KAPC_ANIMATION_STATES[2];           // { "visible", "invisible" }

        // ---- data members (declaration order; console offsets in the comments) -------
        TextField                      mTitleText;                     // +0x38   "Title_mc"
        OnlineLoadingPlayer            maPlayer[KI_MAX_PLAYERS];       // +0x160  (0x4A0 stride)
        GuiNetworkRouteInfo            mRouteInfoDisplay;              // +0x2660
        AnimationComponent             mPlayerListHeadingComponent;    // +0x4810
        AnimationComponent             mChevronsColourComponent;       // +0x489C "ChevronsColour"
        GuiEventNetworkLobbyPlayerList mLobbyPlayerInfoList;           // +0x4928 (the 456-byte roster)
        ESubState                      meSubState;                     // +0x4AF0
        GuiCache*                      mpGuiCache;                     // +0x4AF4
        s32                            miNumFilesToLoad;               // +0x4AF8
        s32                            miNumLoadedComponents;          // +0x4AFC
        s32                            miCurrentRoundDisplayed;        // +0x4B00
        GuiEventNetworkGameParams      mCurrentGameParams;             // +0x4B04
        f32                            mfUpdateTimer;                  // +0x4CE4
        bool                           mbDisconnected;                 // +0x4CE8
    };
}

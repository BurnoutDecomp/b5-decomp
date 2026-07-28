// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo -- wave-H partfile 02: the load ladder.
//   CheckForCompletedLoads  @0x824A3FB8  (DWARF cpp:965)
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (pseudocode + asm,
// scratchpad/waveB/GameSource_Gui_Flow_Screen_States_BrnOnlineGameRoomPlayerInfo.cpp.asm.txt).
//
// The screen's per-frame loading state machine: each E_SUBSTATE_LOADING_* rung either
// waits on the GUI cache's resource loader (EnsureResourcesAreLoaded over the rung's
// tuple table, then names the components it expects and starts the rung's apt movie) or
// waits on the cache's expected-apt-component list (AreAllAptComponentsInitialised, then
// clears the list and brings the rung's page up). The map rungs hand over to the
// CrashNavMap base.
//
// The two SetExpected*Components rungs this ladder calls
// (SetExpectedLobbyComponents @0x82484E78 / SetExpectedPauseComponents @0x82484FE0) are
// declared in the owning header; their bodies are in BrnOnlineGameRoomPlayerInfo_wH_17.cpp
// (they were blocked on a missing MenuComponent::AppendExpectedAptComponent declaration,
// which has since been added).

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface::PlayAptMovie
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"       // GuiComponent::AddOutputAptViewState
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache / GuiFlow
#include "GameSource/GameState/BrnGameStateSharedIO.h"                    // GameStateModuleIO::IsOnlineFreeBurnLobby

namespace BrnGui
{
    namespace
    {
        // The apt clip the options page transitions through (X360 aAptTransition_1, the
        // AddOutputAptViewState first argument at 0x824A4124).
        const char KAC_APT_TRANSITION[] = "apt_Transition";

        // The empty player-stats name the ladder pushes when it resets the panel
        // (X360 unk_820046A7, the zero-length rodata string).
        const char KAC_EMPTY_STRING[] = "";

        // The apt movie names PlayAptMovie is handed per rung (X360 off_82F27B7C /
        // off_82F27B84 / off_82F27BCC / off_82F27B14 / off_82F27BD0).
        const char KAC_LOBBY_MOVIE[]      = "ON_GR_PI";
        const char KAC_PAUSE_MOVIE[]      = "ON_PAUSE";
        const char KAC_CHALLENGE_MOVIE[]  = "ON_CHAL";
        const char KAC_SETTINGS_MOVIE[]   = "BrnCrashNavOptions";
        const char KAC_VIEW_EVENT_MOVIE[] = "ON_VWOPT";

        // The apt level PlayAptMovie is called with on every rung (li r5, 3).
        const s32 KI_APT_MOVIE_LEVEL = 3;

        // The live row count SetupGroup activates on the 4-slot settings toggle group
        // (li r4, 4 at 0x824A4464 -- the group's item count, not a layout constant).
        const s32 KI_SETTINGS_TOGGLE_ITEMS = 4;
    }

    // @0x824A3FB8 -- DWARF cpp:965. Advance whichever load is in flight.
    void OnlineGameRoomPlayerInfo::CheckForCompletedLoads()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");                                  // cpp:979

        switch (meSubState)
        {
        case E_SUBSTATE_LOADING_SCREEN:
            if (mpGuiCache->EnsureResourcesAreLoaded(maLobbyResourceTuplesToLoad,
                                                     static_cast<u32>(miNumLobbyResourcesToLoad)))
            {
                SetExpectedLobbyComponents();
                mpStateInterface->PlayAptMovie(KAC_LOBBY_MOVIE, KI_APT_MOVIE_LEVEL);
                meSubState = E_SUBSTATE_LOADING_COMPONENTS;
            }
            break;

        case E_SUBSTATE_LOADING_PAUSE_SCREEN:
            if (mpGuiCache->EnsureResourcesAreLoaded(maPauseResourceTuplesToLoad,
                                                     static_cast<u32>(miNumPauseResourcesToLoad)))
            {
                SetExpectedPauseComponents();
                mpStateInterface->PlayAptMovie(KAC_PAUSE_MOVIE, KI_APT_MOVIE_LEVEL);
                meSubState = E_SUBSTATE_LOADING_PAUSE_COMPONENTS;
            }
            break;

        case E_SUBSTATE_LOADING_PAUSE_COMPONENTS:
            if (mpGuiCache != 0 && mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
            {
                mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
                ShowPauseOptions();
                meSubState = E_SUBSTATE_PAUSE;
            }
            break;

        case E_SUBSTATE_LOADING_CHALLENGES_SCREEN:
            if (mpGuiCache->EnsureResourcesAreLoaded(maChallengeResourceTuplesToLoad,
                                                     static_cast<u32>(miNumChallengeResourcesToLoad)))
            {
                SetExpectedChallengesComponents();
                mpStateInterface->PlayAptMovie(KAC_CHALLENGE_MOVIE, KI_APT_MOVIE_LEVEL);
                meSubState = E_SUBSTATE_LOADING_CHALLENGES_COMPONENTS;
            }
            break;

        case E_SUBSTATE_LOADING_CHALLENGES_COMPONENTS:
            if (mpGuiCache != 0 && mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
            {
                mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
                ShowChallengesOptions();
                meSubState = E_SUBSTATE_CHALLENGES;
            }
            break;

        case E_SUBSTATE_LOADING_SETTINGS_SCREEN:
            if (mpGuiCache->EnsureResourcesAreLoaded(maSettingsResourceTuplesToLoad,
                                                     static_cast<u32>(miNumSettingsResourcesToLoad)))
            {
                SetExpectedSettingsComponents();
                mpStateInterface->PlayAptMovie(KAC_SETTINGS_MOVIE, KI_APT_MOVIE_LEVEL);
                meSubState = E_SUBSTATE_LOADING_SETTINGS_COMPONENTS;
            }
            break;

        case E_SUBSTATE_LOADING_SETTINGS_COMPONENTS:
            if (mpGuiCache != 0 && mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
            {
                mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
                meSubState = E_SUBSTATE_SETTINGS;
                CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");                          // cpp:1153
                mSettingToggle.SetupGroup(KI_SETTINGS_TOGGLE_ITEMS, false);
                ShowSettingsOptions();
            }
            break;

        case E_SUBSTATE_LOADING_VIEW_EVENT_SCREEN:
            if (mpGuiCache->EnsureResourcesAreLoaded(maViewEventResourceTuplesToLoad,
                                                     static_cast<u32>(miNumViewEventResourcesToLoad)))
            {
                SetExpectedViewEventComponents();
                mpStateInterface->PlayAptMovie(KAC_VIEW_EVENT_MOVIE, KI_APT_MOVIE_LEVEL);
                meSubState = E_SUBSTATE_LOADING_VIEW_EVENT_COMPONENTS;
            }
            break;

        case E_SUBSTATE_LOADING_VIEW_EVENT_COMPONENTS:
            if (mpGuiCache != 0 && mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
            {
                mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);

                // Inlined by the X360 into a pair of compares against 15/16 (0x824A450C).
                if (BrnGameState::GameStateModuleIO::IsOnlineFreeBurnLobby(
                        static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(mpGuiCache->meOnlineGameMode)))
                {
                    if (mbShownFromPause)
                    {
                        UnloadViewEventResources();
                        HideHUDAndEnterMenus();
                        ShowPauseScreen();
                    }
                    else
                    {
                        UnloadViewEventResources();
                        HideViewEventPage();
                    }
                }
                else
                {
                    meSubState = E_SUBSTATE_VIEW_EVENT;
                    CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");                      // cpp:1208
                    ShowViewEventOptions();
                }
            }
            break;

        case E_SUBSTATE_LOADING_COMPONENTS:
            if (mpGuiCache != 0 && mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
            {
                mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
                meSubState = E_SUBSTATE_MAIN;
                CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");                          // cpp:1011

                mOptionsAnimation.AddOutputAptViewState(KAC_APT_TRANSITION,
                                                        KAPC_ANIMATION_STATES[1], false);
                mSelectedPlayerID = -1;

                // Clamp the displayed round into the game's round count (upper clamp
                // stored first, then the lower clamp -- two stores in the asm).
                if (miCurrentRoundDisplayed >= mpGuiCache->miOnlineNumRounds - 1)
                {
                    miCurrentRoundDisplayed = mpGuiCache->miOnlineNumRounds - 1;
                }
                if (miCurrentRoundDisplayed <= 0)
                {
                    miCurrentRoundDisplayed = 0;
                }

                mMessageText.SetText(KAC_PLAYER_INFO_STRING_ID);
                mRouteInfoDisplay.miNumComponentsLoaded = GuiNetworkRouteInfo::KI_NUM_COMPONENTS_TO_LOAD;
                mPlayerStatsDisplay.SetComponentLoaded();
                mPlayerStatsDisplay.SetState(GuiNetworkPlayerStats::E_STATE_VISIBLE);
                mRouteInfoDisplay.SetState(GuiNetworkRouteInfo::E_STATE_INVISIBLE);
                mPlayerStatsDisplay.SetInfo(KAC_EMPTY_STRING, 0, 0, 0, false, mpGuiCache);
                mbHighlightedNewPlayer = false;
            }
            break;

        case E_SUBSTATE_LOADING_MAP_SCREEN:
            meSubState = E_SUBSTATE_LOADING_MAP_SCREEN_COMPONENTS;
            break;

        case E_SUBSTATE_LOADING_MAP_SCREEN_COMPONENTS:
            CrashNavMap::OnEnter();
            if (mbShownFromPause)
            {
                meMapState   = E_MAPSTATE_MAP;
                meScreenType = E_SCREEN_TYPE_ONLINE_FROM_PAUSE;
            }
            else
            {
                meScreenType = E_SCREEN_TYPE_ONLINE;
                meMapState   = E_MAPSTATE_MAP;
            }
            meSubState = E_SUBSTATE_MAP;
            break;

        default:
            break;
        }
    }
}

// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo -- wave-N3 partfile 01: the page transitions.
//   ShowLobby / HideLobby
//   HideHUDAndEnterMenus
//   ShowMapScreen
//   ShowChallengesScreen / HideChallengePage
//   HideSettingsPage
//   ShowViewEventScreen / HideViewEventPage / UnloadViewEventResources
//
// Every page change is the same handful of GUI commands in a fixed order: the lobby apt
// movie is unmounted (PlayAptMovie with the empty name), the front-end map is
// (de)activated (191), the HUD is shown or hidden (148), car control is handed to or
// taken from the player (65), and the pages that return the player to driving also post
// the screen-flow command 533 and reset the main-map view state (213, on the view-state
// and internal channels). The "show" transitions only arm a loading sub-state; the load
// itself runs in CheckForCompletedLoads.
//
// Records go out on the GUI channel as the console's StateInterface::OutputGuiEvent<T>
// builds them: a GuiEventWrapper<T, 40> around the payload (size, id, payload offset,
// payload). The in-tree StateInterface::OutputGuiEvent<T> passes the event straight
// through on a channel equal to its id, so this file posts the wrapper itself.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEventWrapper
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue / PlayAptMovie
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event
#include "GameSource/GameState/Progression/BrnProfile.h"                  // Profile::HasPlayerSeenTrainingType
#include "GameSource/Gui/BrnGuiCache.h"                                   // GuiCache::UnloadResources / GetProfile
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                     // ShowHideHud / RequestCarControlChange / PlayerTexture / RequestTraining / ShowHideSatNav
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiEventActivateCrashNav
#include "SharedClasses/Progression/BrnTrainingTypes.h"                   // BrnProgression::ETrainingType

namespace BrnGui
{
    namespace
    {
        // The GUI-out channel every command here is posted on.
        const s32 KI_CHANNEL_GUI_OUT = 40;

        // The unmount of the page's apt movie: the empty movie name at level 3.
        const char KAC_EMPTY_MOVIE[]   = "";
        const s32  KI_APT_MOVIE_LEVEL  = 3;

        // The console's OutputGuiEvent<T> body: box the payload in a GuiEventWrapper<T,40>
        // and queue the wrapper on the GUI-out channel.
        template <class TEvent>
        void OutputGuiEventRecord(CgsGui::StateInterface* lpStateInterface, TEvent& lrEvent)
        {
            CgsGui::GuiEventWrapper<TEvent, KI_CHANNEL_GUI_OUT> lRecord(lrEvent);
            lpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lRecord), lRecord.GetChannel(),
                static_cast<s32>(sizeof(lRecord)));
        }

        // Id 533, payload size 1 -- the screen-flow command the "back to driving" pages
        // post (the committed BrnOnlinePlay.cpp / wave-H 07 post the same record). FLAG:
        // no type home; the console builds only the header, and the payload byte it
        // publishes is the 0 the previous record left in the reused stack slot.
        struct GuiEventScreenFlowCommandPayload
        {
            u8 mu8Payload;
            s32 GetEventType() const { return 533; }
        };

        // Id 580, payload size 1 -- posted as the challenge page opens (the game-state
        // bridge turns it into the every-player challenge-status request). FLAG: no type
        // home; payload byte not written by the console (the stale 0 of the record before).
        struct GuiEventRequestChallengeStatusPayload
        {
            u8 mu8Payload;
            s32 GetEventType() const { return 580; }
        };

        // Record-size pins: every payload here is pointer-free, so the wrapper must come
        // out at the console's AddEvent size.
        static_assert(sizeof(CgsGui::GuiEventWrapper<GuiEventShowHideHud, 40>) == 16, "148 record is 16 bytes");
        static_assert(sizeof(CgsGui::GuiEventWrapper<GuiRequestCarControlChangeEvent, 40>) == 16, "65 record is 16 bytes");
        static_assert(sizeof(CgsGui::GuiEventWrapper<GuiEventScreenFlowCommandPayload, 40>) == 16, "533 record is 16 bytes");
        static_assert(sizeof(CgsGui::GuiEventWrapper<GuiEventRequestChallengeStatusPayload, 40>) == 16, "580 record is 16 bytes");
        static_assert(sizeof(CgsGui::GuiEventWrapper<GuiEventNetworkOutputPlayerTexture, 40>) == 20, "264 record is 20 bytes");
        static_assert(sizeof(CgsGui::GuiEventWrapper<GuiEventRequestTraining, 40>) == 16, "572 record is 16 bytes");
        static_assert(sizeof(GuiEventActivateCrashNav) == 20, "191 record is 20 bytes");

        // ---- the shared command sequences ---------------------------------------------
        void PostActivateCrashNav(CgsGui::StateInterface* lpStateInterface, bool lbActivate)
        {
            GuiEventActivateCrashNav lActivateCrashNav(lbActivate);
            lpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lActivateCrashNav), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(lActivateCrashNav)));
        }

        void PostShowHideHud(CgsGui::StateInterface* lpStateInterface, bool lbShowHud)
        {
            GuiEventShowHideHud lShowHideHud;
            lShowHideHud.maData[0] = lbShowHud ? 1 : 0;
            OutputGuiEventRecord(lpStateInterface, lShowHideHud);
        }

        void PostCarControlChange(CgsGui::StateInterface* lpStateInterface, bool lbEnableCarControl)
        {
            GuiRequestCarControlChangeEvent lCarControl;
            lCarControl.maData[0] = lbEnableCarControl ? 1 : 0;
            OutputGuiEventRecord(lpStateInterface, lCarControl);
        }

        void PostScreenFlowCommand(CgsGui::StateInterface* lpStateInterface)
        {
            GuiEventScreenFlowCommandPayload lCommand;
            lCommand.mu8Payload = 0;
            OutputGuiEventRecord(lpStateInterface, lCommand);
        }

        // The main-map view-state reset (213 { main map, no fade, hidden }) on the
        // view-state channel and then on the internal channel.
        void PostHideMainMap(CgsGui::StateInterface* lpStateInterface)
        {
            GuiEventShowHideSatNav lShowHideSatNav;
            lShowHideSatNav.Construct(GuiEventShowHideSatNav::E_MAPTYPE_MAIN, false, 0.0f);
            lpStateInterface->OutputViewState(lShowHideSatNav);
            lpStateInterface->OutputInternalState(lShowHideSatNav);
        }

        // Id 264: the player-texture feed; the lobby turns it to the player image on
        // entry and off on exit, with no particular player (-1).
        void PostPlayerTexture(CgsGui::StateInterface* lpStateInterface,
                               GuiEventNetworkOutputPlayerTexture::EOutput leOutput)
        {
            GuiEventNetworkOutputPlayerTexture lPlayerTexture;
            lPlayerTexture.meOutput          = leOutput;
            lPlayerTexture.mPlayerIDToOutput = -1;
            OutputGuiEventRecord(lpStateInterface, lPlayerTexture);
        }
    }

    // ------------------------------------------------------------ ShowLobby
    // Arm the lobby page load (from pause or from driving): feed the player image, take
    // the front-end map, the HUD and car control away.
    void OnlineGameRoomPlayerInfo::ShowLobby()
    {
        CGS_ASSERT((meSubState == E_SUBSTATE_PAUSE) || (meSubState == E_SUBSTATE_FREE_BURNING),
                   "(meSubState == E_SUBSTATE_PAUSE) || (meSubState == E_SUBSTATE_FREE_BURNING)");

        meSubState = E_SUBSTATE_LOADING_SCREEN;

        PostPlayerTexture(mpStateInterface, GuiEventNetworkOutputPlayerTexture::E_OUTPUT_PLAYER_IMAGE);
        PostActivateCrashNav(mpStateInterface, false);
        PostShowHideHud(mpStateInterface, false);
        PostCarControlChange(mpStateInterface, false);
    }

    // ------------------------------------------------------------ HideLobby
    // Leave the lobby page for driving: unmount the movie, drop the selection and the two
    // lobby panels, stop the player image, and give the map, the HUD and car control back.
    void OnlineGameRoomPlayerInfo::HideLobby()
    {
        CGS_ASSERT(meSubState == E_SUBSTATE_MAIN, "meSubState == E_SUBSTATE_MAIN");

        meSubState = E_SUBSTATE_FREE_BURNING;

        mpStateInterface->PlayAptMovie(KAC_EMPTY_MOVIE, KI_APT_MOVIE_LEVEL);

        mSelectedPlayerID = -1;
        mPlayerStatsDisplay.Destruct();
        mRouteInfoDisplay.Destruct();

        PostPlayerTexture(mpStateInterface, GuiEventNetworkOutputPlayerTexture::E_OUTPUT_OFF);
        PostActivateCrashNav(mpStateInterface, true);
        PostScreenFlowCommand(mpStateInterface);
        PostShowHideHud(mpStateInterface, true);
        PostCarControlChange(mpStateInterface, true);
        PostHideMainMap(mpStateInterface);
    }

    // ------------------------------------------------------------ HideHUDAndEnterMenus
    // Take the front-end map, the HUD and car control away before a menu page loads; the
    // main map's view state is reset in between. The sub-state is left to the caller.
    void OnlineGameRoomPlayerInfo::HideHUDAndEnterMenus()
    {
        CGS_ASSERT((meSubState == E_SUBSTATE_FREE_BURNING) || (meSubState == E_SUBSTATE_MAIN) ||
                   (meSubState == E_SUBSTATE_CHALLENGES) || (meSubState == E_SUBSTATE_VIEW_EVENT) ||
                   (meSubState == E_SUBSTATE_SETTINGS),
                   "(meSubState == E_SUBSTATE_FREE_BURNING) || (meSubState == E_SUBSTATE_MAIN) || "
                   "(meSubState == E_SUBSTATE_CHALLENGES) || (meSubState == E_SUBSTATE_VIEW_EVENT) || "
                   "(meSubState == E_SUBSTATE_SETTINGS)");

        PostActivateCrashNav(mpStateInterface, false);
        PostShowHideHud(mpStateInterface, false);
        PostHideMainMap(mpStateInterface);
        PostCarControlChange(mpStateInterface, false);
    }

    // ------------------------------------------------------------ ShowMapScreen
    // Arm the map page load; the first map update after it loads snaps the map onto the
    // local player.
    void OnlineGameRoomPlayerInfo::ShowMapScreen()
    {
        CGS_ASSERT((meSubState == E_SUBSTATE_FREE_BURNING) || (meSubState == E_SUBSTATE_MAIN) ||
                   (meSubState == E_SUBSTATE_PAUSE),
                   "( meSubState == E_SUBSTATE_FREE_BURNING ) || ( meSubState == E_SUBSTATE_MAIN ) || "
                   "( meSubState == E_SUBSTATE_PAUSE )");

        PostActivateCrashNav(mpStateInterface, false);
        PostShowHideHud(mpStateInterface, false);
        PostCarControlChange(mpStateInterface, false);

        meSubState       = E_SUBSTATE_LOADING_MAP_SCREEN;
        mbFirstMapUpdate = true;
    }

    // ------------------------------------------------------------ ShowChallengesScreen
    // Arm the challenge page load, ask for the every-player challenge status, and raise
    // the first of the three challenge training tips the player has not seen yet.
    void OnlineGameRoomPlayerInfo::ShowChallengesScreen()
    {
        CGS_ASSERT((meSubState == E_SUBSTATE_PAUSE) || (meSubState == E_SUBSTATE_FREE_BURNING),
                   "(meSubState == E_SUBSTATE_PAUSE) || (meSubState == E_SUBSTATE_FREE_BURNING)");

        meSubState = E_SUBSTATE_LOADING_CHALLENGES_SCREEN;

        PostActivateCrashNav(mpStateInterface, false);
        PostShowHideHud(mpStateInterface, false);

        GuiEventRequestChallengeStatusPayload lStatusRequest;
        lStatusRequest.mu8Payload = 0;
        OutputGuiEventRecord(mpStateInterface, lStatusRequest);

        const BrnProgression::Profile* lpProfile = mpGuiCache->GetProfile();
        BrnProgression::ETrainingType leTrainingType;
        if (!lpProfile->HasPlayerSeenTrainingType(BrnProgression::E_TRAINING_TYPE_OPEN_ONLINE_CHALLENGES))
        {
            leTrainingType = BrnProgression::E_TRAINING_TYPE_OPEN_ONLINE_CHALLENGES;
        }
        else if (!lpProfile->HasPlayerSeenTrainingType(BrnProgression::E_TRAINING_TYPE_OPEN_ONLINE_CHALLENGES_2))
        {
            leTrainingType = BrnProgression::E_TRAINING_TYPE_OPEN_ONLINE_CHALLENGES_2;
        }
        else if (!lpProfile->HasPlayerSeenTrainingType(BrnProgression::E_TRAINING_TYPE_OPEN_ONLINE_CHALLENGES_3))
        {
            leTrainingType = BrnProgression::E_TRAINING_TYPE_OPEN_ONLINE_CHALLENGES_3;
        }
        else
        {
            return;
        }

        GuiEventRequestTraining lRequestTraining;
        lRequestTraining.meTrainingType = static_cast<s32>(leTrainingType);
        OutputGuiEventRecord(mpStateInterface, lRequestTraining);
    }

    // ------------------------------------------------------------ HideChallengePage
    // Leave the challenge page for driving.
    void OnlineGameRoomPlayerInfo::HideChallengePage()
    {
        CGS_ASSERT(meSubState == E_SUBSTATE_CHALLENGES, "meSubState == E_SUBSTATE_CHALLENGES");

        meSubState = E_SUBSTATE_FREE_BURNING;

        mpStateInterface->PlayAptMovie(KAC_EMPTY_MOVIE, KI_APT_MOVIE_LEVEL);
        PostActivateCrashNav(mpStateInterface, true);
        PostScreenFlowCommand(mpStateInterface);
        PostShowHideHud(mpStateInterface, true);
        PostCarControlChange(mpStateInterface, true);
    }

    // ------------------------------------------------------------ HideSettingsPage
    // Leave the settings page for driving.
    void OnlineGameRoomPlayerInfo::HideSettingsPage()
    {
        CGS_ASSERT(meSubState == E_SUBSTATE_SETTINGS, "meSubState == E_SUBSTATE_SETTINGS");

        meSubState = E_SUBSTATE_FREE_BURNING;

        mpStateInterface->PlayAptMovie(KAC_EMPTY_MOVIE, KI_APT_MOVIE_LEVEL);
        PostActivateCrashNav(mpStateInterface, true);
        PostScreenFlowCommand(mpStateInterface);
        PostShowHideHud(mpStateInterface, true);
        PostCarControlChange(mpStateInterface, true);
    }

    // ------------------------------------------------------------ ShowViewEventScreen
    // Arm the view-event page load.
    void OnlineGameRoomPlayerInfo::ShowViewEventScreen()
    {
        CGS_ASSERT((meSubState == E_SUBSTATE_PAUSE) || (meSubState == E_SUBSTATE_FREE_BURNING),
                   "(meSubState == E_SUBSTATE_PAUSE) || (meSubState == E_SUBSTATE_FREE_BURNING)");

        meSubState = E_SUBSTATE_LOADING_VIEW_EVENT_SCREEN;

        PostActivateCrashNav(mpStateInterface, false);
        PostShowHideHud(mpStateInterface, false);
        PostCarControlChange(mpStateInterface, false);
    }

    // ------------------------------------------------------------ HideViewEventPage
    // Leave the view-event page for driving: drop the route-info panel, unmount the
    // movie, give everything back and reset the main-map view state.
    void OnlineGameRoomPlayerInfo::HideViewEventPage()
    {
        CGS_ASSERT((meSubState == E_SUBSTATE_VIEW_EVENT) ||
                   (meSubState == E_SUBSTATE_LOADING_VIEW_EVENT_COMPONENTS),
                   "(meSubState == E_SUBSTATE_VIEW_EVENT) || "
                   "(meSubState == E_SUBSTATE_LOADING_VIEW_EVENT_COMPONENTS)");

        meSubState = E_SUBSTATE_FREE_BURNING;

        mRouteInfoDisplay.Destruct();

        mpStateInterface->PlayAptMovie(KAC_EMPTY_MOVIE, KI_APT_MOVIE_LEVEL);
        PostActivateCrashNav(mpStateInterface, true);
        PostScreenFlowCommand(mpStateInterface);
        PostShowHideHud(mpStateInterface, true);
        PostCarControlChange(mpStateInterface, true);
        PostHideMainMap(mpStateInterface);
    }

    // ------------------------------------------------------------ UnloadViewEventResources
    // Tear the view-event page down: reset the main-map view state, drop the route-info
    // panel, unmount the movie, and hand the page's resource tuples back to the cache.
    void OnlineGameRoomPlayerInfo::UnloadViewEventResources()
    {
        PostHideMainMap(mpStateInterface);

        mRouteInfoDisplay.Destruct();

        mpStateInterface->PlayAptMovie(KAC_EMPTY_MOVIE, KI_APT_MOVIE_LEVEL);

        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

        if (mpGuiCache != 0)
        {
            mpGuiCache->UnloadResources(maViewEventResourceTuplesToLoad,
                                        static_cast<u32>(miNumViewEventResourcesToLoad));
        }
    }
}

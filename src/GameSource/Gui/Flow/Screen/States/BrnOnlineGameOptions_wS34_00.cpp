// ===================================================================================
// BrnGui::OnlineGameOptions -- stub-wave partfile 00: the lifecycle pair, the option
// table, the load-list input and the request/response handlers.
//   BuildGameOptions
//   ResetGameOptions
//   GetHighlightedOption
//   HandleCarInfoResponseEvent
//   OnLeave
//   RequestPresetEvents
//   HandleControllerInputLoadOptions
//   Update
// Reconstructed from the console image; the assembly arbitrates over the decompiler.
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameOptions.h"

#include <cstddef>                                                        // offsetof
#include "GameShared/GameClasses/Containers/CgsArray.h"                   // Array<CgsID, N>
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStreamBase
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // the state in-queue
#include "GameSource/GameState/BrnGameStateSharedIO.h"                    // GameStateModuleIO::EGameModeType
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiOptionsDataProfile.h"                      // OptionsDataProfile
#include "GameSource/Gui/BrnGuiWorldDataController.h"                     // WorldDataController::GetVehicleList
#include "GameSource/Input/GameInputActions.h"                            // E_GAMEINPUTACTIONS_GUI_*
#include "GameSource/Resource/BrnDLCManager.h"                            // g_DLCFeatureAvailability
#include "SharedClasses/DataLists/VehicleList.h"                          // BrnResource::VehicleList
#include "SharedClasses/DataLists/VehicleListEntry.h"                     // VehicleListEntry::GetUnlockRank

namespace BrnGui
{
    namespace
    {
        namespace GSM = BrnGameState::GameStateModuleIO;

        // The state in-queue concrete type (GuiEventQueue is its pointer-only face).
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        // Out-queue channels: GUI events, view state, internal state.
        const s32 KI_CHANNEL_GUI_OUT        = 40;
        const s32 KI_CHANNEL_VIEW_STATE     = 41;
        const s32 KI_CHANNEL_INTERNAL_STATE = 42;

        // The in-queue event ids Update routes.
        const s32 KI_EVENT_CONTROLLER        = 6;
        const s32 KI_EVENT_DISCONNECTED      = 44;
        const s32 KI_EVENT_IN_GAME           = 50;
        const s32 KI_EVENT_IN_GAME_FAILED    = 51;
        const s32 KI_EVENT_GUI_CACHE         = 64;
        const s32 KI_EVENT_OVERLAY_COMPLETE  = 189;
        const s32 KI_EVENT_LOBBY_PLAYER_LIST = 244;
        const s32 KI_EVENT_CAR_INFO_RESPONSE = 412;

        // The apt movie the screen mounts, and the level it is cleared on.
        const s32 KI_APT_MOVIE_LEVEL = 3;

        // The DLC feature that unlocks the stunt game modes (no recovered name).
        const s32 KI_DLC_FEATURE_ONLINE_STUNT_MODES = 19;   // FLAG name

        // The load list's arrow states (KPC_ARROW_ANIMATION_STATES indices).
        const s32 KI_ARROW_INVISIBLE = 0;
        const s32 KI_ARROW_VISIBLE   = 1;
        const s32 KI_ARROW_ANIMATE   = 2;

        const char KAC_APT_TRANSITION[] = "apt_Transition";

        // Controller event payload: the action id rides in the second word.
        struct ControllerButtonPayload : public CgsModule::Event
        {
            s32 miDevice;     // +0x00
            s32 miButtonId;   // +0x04
        };

        // Event 244 payload view: the lobby roster rows, then the live player count.
        struct GuiEventNetworkLobbyPlayerListPayload : public CgsModule::Event
        {
            u8  maRows[0x1C0];   // +0x000 (eight lobby rows, not read here)
            s32 miNumPlayers;    // +0x1C0
        };

        // Event 412 payload view: the car ids of the lobby's players.
        struct GuiEventCarInfoResponsePayload : public CgsModule::Event
        {
            Array<CgsID, 128> maCarIds;   // +0x000 (count at +0x400)
        };

        // { 4, 193, 12, <list> }, channel 40, 16 bytes: ask for a preset-race list.
        struct GuiEventRequestSpecificPreSetRacesWire : public CgsGui::GuiEvent<193>
        {
            s32 miPresetRaceType;   // +0x0C

            explicit GuiEventRequestSpecificPreSetRacesWire(s32 liPresetRaceType)
                : CgsGui::GuiEvent<193>(
                      static_cast<u32>(sizeof(s32)),
                      static_cast<u32>(offsetof(GuiEventRequestSpecificPreSetRacesWire,
                                                miPresetRaceType)))
                , miPresetRaceType(liPresetRaceType)
            {
            }
        };

        // { 2, 536, 12, <two zero bytes> }, channel 40, 16 bytes: clear the news ticker.
        struct GuiEventTickerClearMessagesWire : public CgsGui::GuiEvent<536>
        {
            u8 maPayload[2];   // +0x0C

            GuiEventTickerClearMessagesWire()
                : CgsGui::GuiEvent<536>(
                      static_cast<u32>(sizeof(maPayload)),
                      static_cast<u32>(offsetof(GuiEventTickerClearMessagesWire, maPayload)))
            {
                maPayload[0] = 0;
                maPayload[1] = 0;
            }
        };

        // { 12, 213, 12, map type, fade time, show }, 24 bytes, posted on the view-state and
        // the internal-state channels: show or hide the sat-nav.
        struct GuiEventShowHideSatNavWire : public CgsGui::GuiEvent<213>
        {
            s32 meMapType;    // +0x0C (0 == the main map)
            f32 mfFadeTime;   // +0x10
            u8  mbShow;       // +0x14
            u8  maPad[3];

            GuiEventShowHideSatNavWire(s32 leMapType, f32 lfFadeTime, bool lbShow)
                : CgsGui::GuiEvent<213>(12, 12)
                , meMapType(leMapType)
                , mfFadeTime(lfFadeTime)
                , mbShow(lbShow ? 1 : 0)
            {
                maPad[0] = maPad[1] = maPad[2] = 0;
            }
        };
    }

    // ================================================================================
    //  BuildGameOptions
    //
    //  The create-match option table: one {title} row per option group, its value rows,
    //  and an E_OPTION_TERMINATOR row after each group. The stunt modes are only offered
    //  when their DLC feature is enabled.
    // ================================================================================
    void OnlineGameOptions::BuildGameOptions()
    {
        typedef CreateMatchOption O;

        // The console clears the table and appends the two game-mode rows twice.
        for (s32 liPass = 0; liPass < 2; ++liPass)
        {
            maOptions.Clear();
            const CreateMatchOption lMode     = { "$ONLINE_GAME_OPTION_MODE",      O::E_OPTION_GAME_MODE };
            const CreateMatchOption lModeRace = { "$ONLINE_GAME_OPTION_MODE_RACE", O::E_OPTION_GAME_MODE_RACE };
            maOptions.Append(lMode);
            maOptions.Append(lModeRace);
        }

        if (BrnResource::g_DLCFeatureAvailability.IsFeatureEnabled(KI_DLC_FEATURE_ONLINE_STUNT_MODES))
        {
            const CreateMatchOption laStuntModes[] =
            {
                { "$ONLINE_GAME_OPTION_MODE_STUNT",              O::E_OPTION_GAME_MODE_STUNT },
                { "$ONLINE_GAME_OPTION_MODE_STUNT_FREE_FOR_ALL", O::E_OPTION_GAME_MODE_STUNT_FREE_FOR_ALL },
                { "$ONLINE_GAME_OPTION_MODE_STUNT_COOP",         O::E_OPTION_GAME_MODE_STUNT_COOP },
            };
            for (u32 luRow = 0; luRow < sizeof(laStuntModes) / sizeof(laStuntModes[0]); ++luRow)
                maOptions.Append(laStuntModes[luRow]);
        }

        const CreateMatchOption laOptions[] =
        {
            { "", O::E_OPTION_TERMINATOR },
            { "$ONLINE_GAME_OPTION_VEHICLE_CHOICE",      O::E_OPTION_VEHICLE_CHOICE },
            { "$ONLINE_GAME_OPTION_VEHICLE_CHOICE_FREE", O::E_OPTION_VEHICLE_CHOICE_FREE },
            { "$ONLINE_GAME_OPTION_VEHICLE_CHOICE_HOST", O::E_OPTION_VEHICLE_CHOICE_HOST },
            { "", O::E_OPTION_TERMINATOR },
            { "$ONLINE_GAME_OPTION_VEHICLE_CLASS",   O::E_OPTION_VEHICLE_CLASS },
            { "$ONLINE_GAME_OPTION_VEHICLE_CLASS_1", O::E_OPTION_VEHICLE_CLASS_1 },
            { "$ONLINE_GAME_OPTION_VEHICLE_CLASS_5", O::E_OPTION_VEHICLE_CLASS_2 },
            { "$ONLINE_GAME_OPTION_VEHICLE_CLASS_4", O::E_OPTION_VEHICLE_CLASS_3 },
            { "$ONLINE_GAME_OPTION_VEHICLE_CLASS_3", O::E_OPTION_VEHICLE_CLASS_4 },
            { "$ONLINE_GAME_OPTION_VEHICLE_CLASS_2", O::E_OPTION_VEHICLE_CLASS_5 },
            { "$ONLINE_GAME_OPTION_VEHICLE_CLASS_6", O::E_OPTION_VEHICLE_CLASS_6 },
            { "", O::E_OPTION_TERMINATOR },
            { "$ONLINE_GAME_OPTION_ROUNDS", O::E_OPTION_ROUNDS },
            { "1", O::E_OPTION_ROUNDS_1 },
            { "2", O::E_OPTION_ROUNDS_2 },
            { "3", O::E_OPTION_ROUNDS_3 },
            { "4", O::E_OPTION_ROUNDS_4 },
            { "5", O::E_OPTION_ROUNDS_5 },
            { "", O::E_OPTION_TERMINATOR },
            { "$ONLINE_GAME_OPTION_ROUNDS", O::E_OPTION_ROUNDS_EVEN },
            { "2",  O::E_OPTION_ROUNDS_2 },
            { "4",  O::E_OPTION_ROUNDS_4 },
            { "6",  O::E_OPTION_ROUNDS_6 },
            { "8",  O::E_OPTION_ROUNDS_8 },
            { "10", O::E_OPTION_ROUNDS_10 },
            { "", O::E_OPTION_TERMINATOR },
            { "$ONLINE_GAME_OPTION_RED_TEAM",       O::E_OPTION_INFINITE_BOOST },
            { "$ONLINE_GAME_OPTION_INFINITE_BOOST", O::E_OPTION_INFINITE_BOOST_ON },
            { "$ONLINE_GAME_OPTION_NORMAL_BOOST",   O::E_OPTION_INFINITE_BOOST_OFF },
            { "", O::E_OPTION_TERMINATOR },
            { "$ONLINE_GAME_OPTION_TRAFFIC", O::E_OPTION_TRAFFIC },
            { "$GENERAL_OPTION_ON",          O::E_OPTION_TRAFFIC_ON },
            { "$GENERAL_OPTION_OFF",         O::E_OPTION_TRAFFIC_OFF },
            { "", O::E_OPTION_TERMINATOR },
            { "$ONLINE_GAME_OPTION_TRAF_CHK", O::E_OPTION_TRAFFIC_CHECKING },
            { "$GENERAL_OPTION_ON",           O::E_OPTION_TRAFFIC_CHECKING_ON },
            { "$GENERAL_OPTION_OFF",          O::E_OPTION_TRAFFIC_CHECKING_OFF },
            { "", O::E_OPTION_TERMINATOR },
            { "$ONLINE_GAME_OPTION_BOOST",    O::E_OPTION_BOOST_TYPE },
            { "$ONLINE_GAME_OPTION_BOOST_B1", O::E_OPTION_BOOST_TYPE_NORMAL },
            { "$ONLINE_GAME_OPTION_BOOST_B2", O::E_OPTION_BOOST_TYPE_DANAGER },
            { "$ONLINE_GAME_OPTION_BOOST_B3", O::E_OPTION_BOOST_TYPE_AGGRESSION },
            { "$ONLINE_GAME_OPTION_BOOST_B4", O::E_OPTION_BOOST_TYPE_STUNT },
            { "", O::E_OPTION_TERMINATOR },
            { "$ONLINE_GAME_OPTION_CR_LIMIT",   O::E_OPTION_RUNNER_CRASH_LIMIT },
            { "$ONLINE_GAME_OPTION_CR_LIMIT_1", O::E_OPTION_RUNNER_CRASH_LIMIT_1 },
            { "$ONLINE_GAME_OPTION_CR_LIMIT_2", O::E_OPTION_RUNNER_CRASH_LIMIT_2 },
            { "$ONLINE_GAME_OPTION_CR_LIMIT_3", O::E_OPTION_RUNNER_CRASH_LIMIT_3 },
            { "$ONLINE_GAME_OPTION_CR_LIMIT_4", O::E_OPTION_RUNNER_CRASH_LIMIT_4 },
            { "$ONLINE_GAME_OPTION_CR_LIMIT_5", O::E_OPTION_RUNNER_CRASH_LIMIT_5 },
            { "$ONLINE_GAME_OPTION_CR_LIMIT_N", O::E_OPTION_RUNNER_CRASH_LIMIT_NEVER },
            { "", O::E_OPTION_TERMINATOR },
            { "$ONLINE_GAME_OPTION_TIME_LIMIT",    O::E_OPTION_TIME_LIMIT },
            { "$ONLINE_GAME_OPTION_TIME_LIMIT_10", O::E_OPTION_TIME_LIMIT_10 },
            { "$ONLINE_GAME_OPTION_TIME_LIMIT_20", O::E_OPTION_TIME_LIMIT_20 },
            { "$ONLINE_GAME_OPTION_TIME_LIMIT_30", O::E_OPTION_TIME_LIMIT_30 },
            { "", O::E_OPTION_TERMINATOR },
        };
        for (u32 luRow = 0; luRow < sizeof(laOptions) / sizeof(laOptions[0]); ++luRow)
            maOptions.Append(laOptions[luRow]);
    }

    // ================================================================================
    //  ResetGameOptions
    //
    //  Back to the default match parameters, keeping the chosen mode and vehicle class.
    //  Road rage starts on two rounds.
    // ================================================================================
    void OnlineGameOptions::ResetGameOptions()
    {
        const s32 leGameMode     = mGameOptions.meGameMode;
        const s32 liVehicleClass = mGameOptions.miVehicleClass;

        mGameOptions.Construct();
        mGameOptions.meGameMode     = leGameMode;
        mGameOptions.miVehicleClass = liVehicleClass;
        mGameOptions.miTimeLimit    = 0;

        if (leGameMode == GSM::E_MODE_ONLINE_ROAD_RAGE)
            mGameOptions.miNumRounds = 2;
        else if (leGameMode == GSM::E_MODE_ONLINE_BURNING_HOME_RUN)
            mGameOptions.miTimeLimit = 0;
    }

    // ================================================================================
    //  GetHighlightedOption
    //
    //  The value the toggle row for leOption currently shows.
    // ================================================================================
    CreateMatchOption::EOption OnlineGameOptions::GetHighlightedOption(CreateMatchOption::EOption leOption)
    {
        const s32 liToggle = mCreateGameToggles.GetIndexFromId(static_cast<u64>(static_cast<s64>(leOption)));
        CGS_ASSERT(liToggle >= 0, "Required toggle not found!");

        return static_cast<CreateMatchOption::EOption>(
            mCreateGameToggles.GetSelectable(liToggle)->GetHighlightedId());
    }

    // ================================================================================
    //  HandleCarInfoResponseEvent
    //
    //  The lobby's cars are known: reset the match parameters (keeping the security
    //  setting) and set the vehicle class to the highest unlock rank among them.
    // ================================================================================
    void OnlineGameOptions::HandleCarInfoResponseEvent(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0, "lpEvent");
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

        CGS_ASSERT(mpGuiCache->GetWorldDataController() != 0, "mpWorldDataController");
        const BrnResource::VehicleList* lpVehicleList =
            mpGuiCache->GetWorldDataController()->GetVehicleList();
        CGS_ASSERT(lpVehicleList != 0, "No vehicle list\n");

        const GuiEventCarInfoResponsePayload* lpResponse =
            static_cast<const GuiEventCarInfoResponsePayload*>(lpEvent);

        s32 liHighestRank = 0;
        for (u32 luCar = 0; luCar < lpResponse->maCarIds.GetLength(); ++luCar)
        {
            const s32 liVehicleIndex = lpVehicleList->GetVehicleIndex(lpResponse->maCarIds.GetItem(luCar));
            const BrnResource::VehicleListEntry* lpVehicleData =
                (liVehicleIndex < 0) ? 0 : lpVehicleList->GetVehicleData(liVehicleIndex);
            // The console streams the car id after both texts.
            CGS_ASSERT(lpVehicleData != 0, "No vehicle data for ");

            const s32 liRank = lpVehicleData->GetUnlockRank();
            if (!(liHighestRank > liRank))
                liHighestRank = liRank;
        }

        const s32 leSecurity = mGameOptions.meSecurity;
        mGameOptions.Construct();
        mGameOptions.meSecurity     = leSecurity;
        mGameOptions.miVehicleClass = liHighestRank;
    }

    // ================================================================================
    //  OnLeave
    // ================================================================================
    void OnlineGameOptions::OnLeave()
    {
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // The console inlines StateInterface::PlayAptMovie: clear the screen's apt level.
        mpStateInterface->PlayAptMovie("", KI_APT_MOVIE_LEVEL);

        mMenuOptions.Clear();
        mCreateGameToggles.Clear();
        mRouteInfoDisplay.Destruct();

        GuiEventTickerClearMessagesWire lClearTicker;
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lClearTicker), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lClearTicker)));

        // Hide the sat-nav: one record on the view-state and the internal-state channels.
        GuiEventShowHideSatNavWire lHideSatNav(0, 0.0f, false);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lHideSatNav), KI_CHANNEL_VIEW_STATE,
            static_cast<s32>(sizeof(lHideSatNav)));
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lHideSatNav), KI_CHANNEL_INTERNAL_STATE,
            static_cast<s32>(sizeof(lHideSatNav)));

        mCreateGameToggles.Unloaded();
        mHelpBar.Destruct();
    }

    // ================================================================================
    //  RequestPresetEvents
    //
    //  Ask the network module for the preset-race list that fits the chosen mode.
    // ================================================================================
    void OnlineGameOptions::RequestPresetEvents()
    {
        s32 liPresetRaceType;
        switch (mGameOptions.meGameMode)
        {
            case GSM::E_MODE_ONLINE_ROAD_RAGE:
                liPresetRaceType = GSM::E_MODE_ONLINE_RACE;
                break;
            case GSM::E_MODE_ONLINE_BURNING_HOME_RUN:
                liPresetRaceType = GSM::E_MODE_ONLINE_BURNING_HOME_RUN;
                break;
            case GSM::E_MODE_ONLINE_FUGITIVE:
            case GSM::E_MODE_ONLINE_FREE_BURN:
            case GSM::E_MODE_ONLINE_MODE_END:
                liPresetRaceType = GSM::E_MODE_STUNT_ATTACK;
                break;
            default:
                liPresetRaceType = GSM::E_MODE_ONLINE_RACE;
                break;
        }

        GuiEventRequestSpecificPreSetRacesWire lRequest(liPresetRaceType);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRequest), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lRequest)));
    }

    // ================================================================================
    //  HandleControllerInputLoadOptions
    //
    //  The load page: up/down scroll the six visible slots over the saved (created) or
    //  the recent (received) list, left/right step through the highlighted slot's rounds,
    //  select loads the slot, cancel goes back, and the shoulders swap the two lists.
    // ================================================================================
    void OnlineGameOptions::HandleControllerInputLoadOptions(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0, "Invalid event sent to OnlineGameOptions::HandleControllerInputLoadOptions");

        const s32 leAction = static_cast<const ControllerButtonPayload*>(lpEvent)->miButtonId;
        s32 liOption = mMenuOptions.GetHighlightedIndex() + miStartItem;

        OptionsDataProfile* lpProfile = mpGuiCache->GetOptionsDataProfile();
        GuiEventNetworkGameParams lParams;

        switch (leAction)
        {
            case E_GAMEINPUTACTIONS_GUI_UP:
            case E_GAMEINPUTACTIONS_GUI_DOWN:
            {
                bool lbChanged = false;
                s32 liNumOptions;
                if (meOptionsToLoad == E_OPTIONS_TO_LOAD_SAVED)
                {
                    liNumOptions = lpProfile->GetNumCreatedOnlineGameOptions();
                }
                else
                {
                    CGS_ASSERT(meOptionsToLoad == E_OPTIONS_TO_LOAD_RECENT,
                               "meOptionsToLoad == E_OPTIONS_TO_LOAD_RECENT");
                    liNumOptions = lpProfile->GetNumReceivedOnlineGameOptions();
                }

                if (leAction == E_GAMEINPUTACTIONS_GUI_UP)
                {
                    if (mMenuOptions.HighlightPrevious())
                    {
                        lbChanged      = true;
                        miCurrentRound = 0;
                        liOption       = mMenuOptions.GetHighlightedIndex() + miStartItem;
                    }
                    else if (miStartItem > 0)
                    {
                        // Scroll the window up one slot.
                        --miStartItem;
                        lbChanged = true;
                        liOption  = mMenuOptions.GetHighlightedIndex() + miStartItem;
                        ShowLoadOptions();
                    }
                    else if (liNumOptions > 1)
                    {
                        // Wrap to the bottom of the list.
                        miStartItem = liNumOptions - KI_MAX_LOAD_OPTIONS;
                        if (miStartItem < 0)
                            miStartItem = 0;
                        liOption  = liNumOptions - 1;
                        lbChanged = true;
                        ShowLoadOptions();
                        mMenuOptions.HighlightIndex(liNumOptions - 1 - miStartItem);
                    }
                }
                else
                {
                    if (mMenuOptions.HighlightNext())
                    {
                        lbChanged      = true;
                        miCurrentRound = 0;
                        liOption       = mMenuOptions.GetHighlightedIndex() + miStartItem;
                    }
                    else if (miStartItem + KI_MAX_LOAD_OPTIONS >= liNumOptions)
                    {
                        // Wrap to the top of the list.
                        if (liNumOptions > 1)
                        {
                            miStartItem = 0;
                            lbChanged   = true;
                            liOption    = 0;
                            ShowLoadOptions();
                            mMenuOptions.HighlightIndex(0);
                        }
                    }
                    else
                    {
                        // Scroll the window down one slot.
                        ++miStartItem;
                        lbChanged = true;
                        liOption  = mMenuOptions.GetHighlightedIndex() + miStartItem;
                        ShowLoadOptions();
                    }
                }

                if (!lbChanged)
                    break;

                TriggerSound(leAction);
                if (meOptionsToLoad != E_OPTIONS_TO_LOAD_SAVED)
                    lpProfile->GetReceivedOnlineGameOptions(liOption, mpGuiCache, &lParams);
                else
                    lpProfile->GetCreatedOnlineGameOptions(liOption, mpGuiCache, &lParams);
                mRouteInfoDisplay.SetInfo(miCurrentRound, &lParams);

                // The arrow on the side the list moved animates; both hide for a single slot.
                const char* lpacUpState;
                const char* lpacDownState;
                if (liNumOptions > 1)
                {
                    const bool lbUp = (leAction == E_GAMEINPUTACTIONS_GUI_UP);
                    lpacUpState   = KPC_ARROW_ANIMATION_STATES[lbUp ? KI_ARROW_ANIMATE : KI_ARROW_VISIBLE];
                    lpacDownState = KPC_ARROW_ANIMATION_STATES[lbUp ? KI_ARROW_VISIBLE : KI_ARROW_ANIMATE];
                }
                else
                {
                    lpacUpState   = KPC_ARROW_ANIMATION_STATES[KI_ARROW_INVISIBLE];
                    lpacDownState = KPC_ARROW_ANIMATION_STATES[KI_ARROW_INVISIBLE];
                }
                mUpArrowAnimator.AddOutputAptViewState(KAC_APT_TRANSITION, lpacUpState, false);
                mDownArrowAnimator.AddOutputAptViewState(KAC_APT_TRANSITION, lpacDownState, false);
                break;
            }

            case E_GAMEINPUTACTIONS_GUI_LEFT:
                // Previous round of the highlighted slot (road rage steps in pairs).
                if (miCurrentRound > 0)
                {
                    if (meOptionsToLoad != E_OPTIONS_TO_LOAD_SAVED)
                        lpProfile->GetReceivedOnlineGameOptions(liOption, mpGuiCache, &lParams);
                    else
                        lpProfile->GetCreatedOnlineGameOptions(liOption, mpGuiCache, &lParams);
                    miCurrentRound -= (lParams.meGameMode == GSM::E_MODE_ONLINE_ROAD_RAGE) ? 2 : 1;
                    mRouteInfoDisplay.SetInfo(miCurrentRound, &lParams);
                }
                break;

            case E_GAMEINPUTACTIONS_GUI_RIGHT:
            {
                // Next round of the highlighted slot (road rage steps in pairs).
                if (meOptionsToLoad != E_OPTIONS_TO_LOAD_SAVED)
                    lpProfile->GetReceivedOnlineGameOptions(liOption, mpGuiCache, &lParams);
                else
                    lpProfile->GetCreatedOnlineGameOptions(liOption, mpGuiCache, &lParams);
                const s32 liStep = (lParams.meGameMode == GSM::E_MODE_ONLINE_ROAD_RAGE) ? 2 : 1;
                if (miCurrentRound < lParams.miNumRounds - liStep)
                {
                    miCurrentRound += liStep;
                    mRouteInfoDisplay.SetInfo(miCurrentRound, &lParams);
                }
                break;
            }

            case E_GAMEINPUTACTIONS_GUI_SELECT:
                // Load the highlighted slot into the screen's parameters.
                if (meOptionsToLoad != E_OPTIONS_TO_LOAD_SAVED)
                    lpProfile->GetReceivedOnlineGameOptions(liOption, mpGuiCache, &mGameOptions);
                else
                    lpProfile->GetCreatedOnlineGameOptions(liOption, mpGuiCache, &mGameOptions);
                ShowGameOptionsScreen();
                TriggerSound(leAction);
                break;

            case E_GAMEINPUTACTIONS_GUI_CANCEL:
                ShowGameOptionsScreen();
                TriggerSound(leAction);
                break;

            case E_GAMEINPUTACTIONS_GUI_LSHOULDER:
            case E_GAMEINPUTACTIONS_GUI_RSHOULDER:
                // Swap to the other list when it has entries.
                if (meOptionsToLoad == E_OPTIONS_TO_LOAD_SAVED &&
                    lpProfile->GetNumReceivedOnlineGameOptions() > 0)
                {
                    miStartItem     = 0;
                    meOptionsToLoad = E_OPTIONS_TO_LOAD_RECENT;
                    ShowLoadOptions();
                    mMenuOptions.HighlightIndex(0);
                    mDownArrowAnimator.AddOutputAptViewState(
                        KAC_APT_TRANSITION,
                        KPC_ARROW_ANIMATION_STATES[(lpProfile->GetNumReceivedOnlineGameOptions() <= 1)
                                                       ? KI_ARROW_INVISIBLE : KI_ARROW_VISIBLE],
                        false);
                    mUpArrowAnimator.AddOutputAptViewState(
                        KAC_APT_TRANSITION, KPC_ARROW_ANIMATION_STATES[KI_ARROW_INVISIBLE], false);
                    mLoadHeaderText.SetText(KAC_RECENT_OPTIONS_STRING_ID);
                }
                else if (meOptionsToLoad == E_OPTIONS_TO_LOAD_RECENT &&
                         lpProfile->GetNumCreatedOnlineGameOptions() > 0)
                {
                    meOptionsToLoad = E_OPTIONS_TO_LOAD_SAVED;
                    miStartItem     = 0;
                    ShowLoadOptions();
                    mMenuOptions.HighlightIndex(0);
                    mDownArrowAnimator.AddOutputAptViewState(
                        KAC_APT_TRANSITION,
                        KPC_ARROW_ANIMATION_STATES[(lpProfile->GetNumCreatedOnlineGameOptions() <= 1)
                                                       ? KI_ARROW_INVISIBLE : KI_ARROW_VISIBLE],
                        false);
                    mUpArrowAnimator.AddOutputAptViewState(
                        KAC_APT_TRANSITION, KPC_ARROW_ANIMATION_STATES[KI_ARROW_INVISIBLE], false);
                    mLoadHeaderText.SetText(KAC_CREATED_OPTIONS_STRING_ID);
                }
                break;

            default:
                break;
        }
    }

    // ================================================================================
    //  Update
    // ================================================================================
    void OnlineGameOptions::Update()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);

        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liEventSize); lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize))
        {
            switch (liEventId)
            {
                case KI_EVENT_CONTROLLER:
                    HandleControllerInput(lpEvent);
                    break;

                case 14:
                case 21:
                    break;

                case KI_EVENT_DISCONNECTED:
                    // Latch the disconnect reason for the popup and leave the screen.
                    CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");
                    mpGuiCache->SetDoDisconnectPopup(lpEvent);
                    SendStateEvent("DISCONNECT");
                    break;

                case KI_EVENT_IN_GAME:
                    HandleInGameEvent(lpEvent);
                    break;

                case KI_EVENT_IN_GAME_FAILED:
                    HandleInGameFailedEvent(lpEvent);
                    break;

                case KI_EVENT_GUI_CACHE:
                    HandleGuiCacheEvent(lpEvent);
                    break;

                case KI_EVENT_OVERLAY_COMPLETE:
                    CGS_ASSERT(lpEvent != 0, "lpOverlayCompleteEvent");
                    break;

                case KI_EVENT_LOBBY_PLAYER_LIST:
                    // An empty lobby means this player is creating the match.
                    CGS_ASSERT(lpEvent != 0, "lpLobbyList");
                    mbIsCreating =
                        static_cast<const GuiEventNetworkLobbyPlayerListPayload*>(lpEvent)->miNumPlayers == 0;
                    break;

                case KI_EVENT_CAR_INFO_RESPONSE:
                    HandleCarInfoResponseEvent(lpEvent);
                    break;

                default:
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    {
                        *CgsDev::Log::gpDebugPrint
                            << "Unexpected event received : " << liEventId
                            << " in "
                            << "..\\..\\..\\GameSource\\Gui/Flow/Screen/States/BrnOnlineGameOptions.cpp"
                            << " at line " << 333 << "\n";
                    }
                    break;
            }
        }

        mRouteInfoDisplay.Update(mpInGuiEventQueue);
        lpInQueue->Clear();

        CheckForCompletedLoads();
        mCreateGameToggles.Update();
        mMenuOptions.Update();

        if (meSubState > E_SUBSTATE_LOADING_COMPONENTS)
            mHelpBar.Update(mpGuiCache->GetTime());
    }
}

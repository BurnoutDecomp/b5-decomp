// BrnGameState::GameStateModule::ProcessGameEvents_Group4 -- the GUI data-request arms of the
// game-event dispatcher (GameStateModule_ProcessGameEvents.cpp routes these ids here).
//
// Each case is one arm of the console's single switch, in source order: the GUI asks the game
// state for progression, rank, rival, landmark, region and road-rule data and gets an action
// back; the street manager, road-rules manager, mugshot manager and image manager take their
// online and gallery events; and the target-event-score family (ids 104, 152, 153, 175, which
// have no name in the image) keeps the profile's stored target scores in step with the server.
//
// lpActionQueue is the dispatcher's output action queue, lpOutput its output buffer. Payloads are
// read through their event structs (BrnGameEvents.h, and the image manager / mugshot manager
// headers for the events those managers own).

#include "GameSource/GameState/BrnGameStateModule.h"

#include <cstring>                                                        // memcpy (case 115)
#include <cstdlib>                                                        // getenv (the case-84 witness)

#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"               // gpDebugPrint (the case-84 witness)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"         // AddEvent
#include "GameShared/GameClasses/Containers/CgsArray.h"                  // Array<ProfileEvent,175>, Array<s64,15>
#include "GameShared/GameClasses/World/CgsWorldMap2D.h"                  // WorldMap2D::GetValue (case 95)
#include "GameSource/GameState/BrnGameStateModuleIO.h"                   // OutputBuffer, PreWorldInputBuffer
#include "GameSource/GameState/BrnGameEvents.h"                          // the request payloads
#include "GameSource/GameState/BrnGameActions.h"                         // the answer records
#include "GameSource/GameState/BrnCgsPlayerName.h"                       // CgsNetwork::PlayerName
#include "GameSource/GameState/SharedIO/BrnGameActionData.h"             // GameStats, LandmarkVariableInfo
#include "GameSource/GameState/SharedIO/BrnTargetEventScore.h"           // TargetEventScore
#include "GameSource/GameState/Progression/BrnProgressionManager.h"      // ProgressionManager
#include "GameSource/GameState/Progression/BrnProfile.h"                 // Profile
#include "GameSource/GameState/Progression/BrnProgressionRivalData.h"    // RivalData
#include "GameSource/GameState/ModeManager/BrnModeManager.h"             // ModeManager
#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager.h" // CountCompletedChallenges
#include "GameSource/GameState/RoadRules/BrnRoadRulesManager.h"          // RoadRulesManager
#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"   // StreetManager
#include "GameSource/GameState/MugshotManager/BrnMugshotManager.h"       // MugshotManager + its two events
#include "GameSource/GameState/ImageManager/BrnGameStateImageManagerBase.h" // GameStateImageManagerBase + its events
#include "GameSource/GameState/CarSelect/BrnCarSelectManager.h"          // CarSelectManager
#include "GameSource/GameState/RichPresenceManager/X360/BrnGameStateRichPresenceManagerX360.h" // OnDistrictChange (case 115)
#include "GameSource/World/AI/Route/BrnRouteMapModuleIO.h"               // RouteMapModuleIO::E_OWNER_GUI (case 84)
#include "SharedClasses/Progression/BrnProgressionData.h"                // ProgressionData
#include "SharedClasses/Progression/BrnRival.h"                          // Rival
#include "SharedClasses/Progression/BrnRace.h"                           // Race (case 89)
#include "SharedClasses/World/BrnWorldRegion.h"                          // WorldRegion (cases 95, 102, 115)

namespace BrnGameState
{

void GameStateModule::ProcessGameEvents_Group4(s32                                           liEventType,
                                               const CgsModule::Event*                       lpEvent,
                                               GameStateModuleIO::GameActionQueue*           lpActionQueue,
                                               const GameStateModuleIO::PreWorldInputBuffer* lpPreWorldInput,
                                               GameStateModuleIO::OutputBuffer*              lpOutput)
{
    (void)lpPreWorldInput;   // no group-4 arm reads the pre-world input

    switch (liEventType)
    {
    case GameStateModuleIO::E_EVENT_CHECK_FOR_COMPLETION:
        mProgressionManager.SendGameCompletionResults(lpActionQueue);
        break;

    case GameStateModuleIO::E_EVENT_PLAYER_ENTERS_RACE_MAP:
    case GameStateModuleIO::E_EVENT_LANDMARK_RACES_REQUEST:
        SendSetLandmarkRacesAction(lpActionQueue);
        break;

    case GameStateModuleIO::E_EVENT_LANDMARK_ROUTE_REQUEST:
    {
        SendRouteRequestAction(reinterpret_cast<const GameStateModuleIO::LandmarkRouteRequestEvent*>(lpEvent),
                               lpActionQueue,
                               BrnAI::RouteMapModuleIO::E_OWNER_GUI);

        // [DIAG] harness witness, not console code: BRN_SATNAV_DIAG, first 32, one line per GUI
        // route question (the satnav_route case counts it).
        static const bool sbSatNavDiag      = (getenv("BRN_SATNAV_DIAG") != 0);
        static s32        siSatNavLinesLeft = 32;
        if (sbSatNavDiag && siSatNavLinesLeft > 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            --siSatNavLinesLeft;
            *CgsDev::Log::gpDebugPrint
                << "[satnav] event 84 -> SendRouteRequestAction owner GUI leg "
                << static_cast<s32>(reinterpret_cast<const GameStateModuleIO::LandmarkRouteRequestEvent*>(lpEvent)->mu16EventID)
                << "\n";
        }
        break;
    }

    case GameStateModuleIO::E_EVENT_ONLINE_ROAD_RULES_PB_RECV:
        mStreetManager.ProcessNetworkHighScoreEvent(
            lpOutput, reinterpret_cast<const GameStateModuleIO::OnlineRoadRulesPersonalBestRecvEvent*>(lpEvent));
        break;

    case GameStateModuleIO::E_EVENT_ONLINE_ROAD_RULES_UPLOADED:
        mStreetManager.ProcessUploadEvent(
            reinterpret_cast<const GameStateModuleIO::OnlineRoadRulesUploadedEvent*>(lpEvent));
        break;

    case GameStateModuleIO::E_EVENT_ONLINE_ROAD_RULES_DOWNLOADED:
        mStreetManager.ProcessDownloadEvent(
            reinterpret_cast<const GameStateModuleIO::OnlineRoadRulesDownloadedEvent*>(lpEvent));
        break;

    case GameStateModuleIO::E_EVENT_EVENT_STATE_REQUEST:
    {
        // Every event the player has discovered, answered as action 179 (the whole 1404-byte
        // array, count word included).
        const BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();

        Array<BrnProgression::ProfileEvent, 175> lDiscoveredEvents;
        lDiscoveredEvents.Construct();

        for (u32 luEvent = 0; luEvent < lpProfile->GetEventCount(); ++luEvent)
        {
            const BrnProgression::ProfileEvent* lpProfileEvent = lpProfile->GetEvent(luEvent);
            if (lpProfileEvent->IsFlagSet(BrnProgression::ProfileEvent::E_FLAG_DISCOVERED))
            {
                lDiscoveredEvents.Append(*lpProfileEvent);
            }
        }

        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lDiscoveredEvents),
                                GameStateModuleIO::E_ACTION_EVENT_STATE_RESPONSE,
                                static_cast<s32>(sizeof(lDiscoveredEvents)));
        break;
    }

    case GameStateModuleIO::E_EVENT_GAME_STATS_REQUEST:
    {
        const s32 liNumChallengesCompleted = mModeManager.GetChallengeManager()->CountCompletedChallenges();

        GameStateModuleIO::GameStats lGameStats;
        mProgressionManager.GetGameStats(&lGameStats, &mStuntManager, liNumChallengesCompleted);

        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lGameStats),
                                GameStateModuleIO::E_ACTION_GAME_STATS_RESPONSE,
                                static_cast<s32>(sizeof(lGameStats)));
        break;
    }

    case GameStateModuleIO::E_EVENT_RANK_INFO_REQUEST:
    {
        const BrnProgression::ProgressionData* lpProgressionData = mProgressionManager.GetProgressionData();
        const s32 liRankCount = static_cast<s32>(lpProgressionData->GetProgressionRankCount());

        const s32 liMarkedMan = static_cast<s8>(
            mProgressionManager.GetProgressionRankForGameMode(GameStateModuleIO::E_MODE_MARKED_MAN));
        const s32 liStuntAttack = static_cast<s8>(
            mProgressionManager.GetProgressionRankForGameMode(GameStateModuleIO::E_MODE_STUNT_ATTACK));
        const s32 liRoadRage = static_cast<s8>(
            mProgressionManager.GetProgressionRankForGameMode(GameStateModuleIO::E_MODE_ROAD_RAGE));
        const s32 liOfflineRace = static_cast<s8>(
            mProgressionManager.GetProgressionRankForGameMode(GameStateModuleIO::E_MODE_OFFLINE_RACE));
        const s32 liPlayerRank = static_cast<s8>(mProgressionManager.GetProgressionRank());

        GameStateModuleIO::RankInfoResponseAction lRankInfo;
        lRankInfo.SetProgressionRanks(liPlayerRank, liRankCount, liOfflineRace, liRoadRage, liStuntAttack, liMarkedMan);

        const BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
        lRankInfo.SetProgressionRankEventWins(
            lpProfile->GetNumRankWinsForGameMode(GameStateModuleIO::E_MODE_OFFLINE_RACE),
            lpProfile->GetNumRankWinsForGameMode(GameStateModuleIO::E_MODE_ROAD_RAGE),
            lpProfile->GetNumRankWinsForGameMode(GameStateModuleIO::E_MODE_STUNT_ATTACK),
            lpProfile->GetNumRankWinsForGameMode(GameStateModuleIO::E_MODE_MARKED_MAN));

        // Stamped after SetProgressionRanks, so that setter's own last-rank assert never sees it.
        if (mProgressionManager.PlayerHasFinishedLastRank())
        {
            lRankInfo.miPlayerRank = GameStateModuleIO::RankInfoResponseAction::KI_PLAYER_HAS_FINISHED_LAST_RANK;
        }

        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRankInfo),
                                GameStateModuleIO::E_ACTION_RANK_INFO_RESPONSE,
                                static_cast<s32>(sizeof(lRankInfo)));
        break;
    }

    case GameStateModuleIO::E_EVENT_LANDMARK_VARIABLE_INFO_REQUEST:
    {
        const GameStateModuleIO::LandmarkInfoRequestEvent* lpLandmarkInfoRequestEvent =
            reinterpret_cast<const GameStateModuleIO::LandmarkInfoRequestEvent*>(lpEvent);

        BrnProgression::Race lRace;
        const u32 luNumRaces =
            mProgressionManager.GetRacesAtLandmark(&lRace, 1, lpLandmarkInfoRequestEvent->mLandmarkIndex, false);
        CGS_ASSERT(luNumRaces == 1,
                   "mProgressionManager.GetRacesAtLandmark( &lRace, 1, lpLandmarkInfoRequestEvent->mLandmarkIndex, false ) == 1");

        GameStateModuleIO::LandmarkVariableInfo lLandmarkVariableInfo;
        lLandmarkVariableInfo.Construct(&lRace, 0);

        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lLandmarkVariableInfo),
                                GameStateModuleIO::E_ACTION_LANDMARK_VARIABLE_INFO_RESPONSE,
                                static_cast<s32>(sizeof(lLandmarkVariableInfo)));
        break;
    }

    case GameStateModuleIO::E_EVENT_PROGRESSION_PROFILE_LOADED:
        OnProfileLoaded(lpOutput, lpActionQueue);
        break;

    case GameStateModuleIO::E_EVENT_CHANGE_WORLD_REGION:
    {
        const WorldRegionChangeEvent* lpWorldRegionChangeEvent =
            reinterpret_cast<const WorldRegionChangeEvent*>(lpEvent);

        GetImageManager()->HandleWorldRegionChangeEvent(lpWorldRegionChangeEvent);

        // The event's region goes out verbatim as action 112.
        GameStateModuleIO::WorldRegionChangeAction lWorldRegionChangeAction;
        static_assert(sizeof(lWorldRegionChangeAction.mNewWorldRegion) == sizeof(WorldRegionChangeEvent),
                      "action 112 carries the event's 8-byte region");
        std::memcpy(&lWorldRegionChangeAction.mNewWorldRegion, lpWorldRegionChangeEvent,
                    sizeof(lWorldRegionChangeAction.mNewWorldRegion));
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lWorldRegionChangeAction),
                                GameStateModuleIO::E_ACTION_WORLD_REGION_CHANGE,
                                static_cast<s32>(sizeof(lWorldRegionChangeAction)));

        // The rich-presence manager publishes the new district on its next update.
        GetRichPresenceManager()->OnDistrictChange(lpWorldRegionChangeEvent->meDistrict);
        break;
    }

    case GameStateModuleIO::E_EVENT_PLAYER_ROUTE_UPDATED:
    {
        const GameStateModuleIO::UpdateGuiRouteAction lUpdateGuiRouteAction;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lUpdateGuiRouteAction),
                                GameStateModuleIO::E_ACTION_UPDATE_GUI_ROUTE,
                                static_cast<s32>(sizeof(lUpdateGuiRouteAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_REGION_FROM_POSITION_REQUEST:
    {
        const GameStateModuleIO::RegionFromPositionRequestEvent* lpRegionRequestEvent =
            reinterpret_cast<const GameStateModuleIO::RegionFromPositionRequestEvent*>(lpEvent);

        // The district map is sampled at the position flattened to (x, z); the permute fills the
        // two unread lanes with x.
        const Vector3& lrPosition = lpRegionRequestEvent->mPosition;
        Vector2 lFlatPosition;
        lFlatPosition.x = lrPosition.x;
        lFlatPosition.y = lrPosition.z;
        lFlatPosition.z = lrPosition.x;
        lFlatPosition.w = lrPosition.x;

        u8 luDistrict = GetDistrictMap()->GetValue(lFlatPosition);
        if (luDistrict == CgsWorld::KU_INVALID_WORLD_MAP_VALUE)
        {
            luDistrict = static_cast<u8>(BrnWorld::E_DISTRICT_INVALID);
        }

        GameStateModuleIO::RegionFromPositionResponseAction lRegionResponse;
        lRegionResponse.mWorldRegion.Construct(static_cast<BrnWorld::EDistrict>(luDistrict));
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRegionResponse),
                                GameStateModuleIO::E_ACTION_REGION_FROM_POSITION_RESPONSE,
                                static_cast<s32>(sizeof(lRegionResponse)));
        break;
    }

    case GameStateModuleIO::E_CAR_CONTROL_CHANGE_REQUEST:
    {
        const GameStateModuleIO::CarControlChangeRequestEvent* lpControlChangeEvent =
            reinterpret_cast<const GameStateModuleIO::CarControlChangeRequestEvent*>(lpEvent);
        CGS_ASSERT(lpControlChangeEvent, "lpControlChangeEvent");

        if (lpControlChangeEvent->mbPlayerShouldHaveControl)
        {
            SetControllerState(E_CONTROLLERSTATE_ACTIVE_GAME_MODE_STATE);
        }
        else
        {
            SetControllerState(E_CONTROLLERSTATE_INACTIVE_GAME_MODE_STATE);
        }
        break;
    }

    case GameStateModuleIO::E_EVENT_GUI_AWARD_SEQUENCE_START:
    {
        // The AI drives the player's car through the award sequence.
        GameStateModuleIO::SetPlayerCarDriverAction lSetPlayerCarDriverAction = {};
        lSetPlayerCarDriverAction.meCarControl  = BrnWorld::E_CAR_CONTROL_AI_MODULE;
        lSetPlayerCarDriverAction.mbIsDriveThru = false;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSetPlayerCarDriverAction),
                                GameStateModuleIO::E_ACTION_SET_PLAYER_CAR_DRIVER,
                                static_cast<s32>(sizeof(lSetPlayerCarDriverAction)));

        const GameStateModuleIO::AwardSequenceStartAction lAwardSequenceStartAction;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAwardSequenceStartAction),
                                GameStateModuleIO::E_ACTION_AWARD_SEQUENCE_START,
                                static_cast<s32>(sizeof(lAwardSequenceStartAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_GUI_AWARD_SEQUENCE_END:
    {
        // The player gets the car back.
        GameStateModuleIO::SetPlayerCarDriverAction lSetPlayerCarDriverAction = {};
        lSetPlayerCarDriverAction.meCarControl  = BrnWorld::E_CAR_CONTROL_ENTITY_MODULE;
        lSetPlayerCarDriverAction.mbIsDriveThru = false;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSetPlayerCarDriverAction),
                                GameStateModuleIO::E_ACTION_SET_PLAYER_CAR_DRIVER,
                                static_cast<s32>(sizeof(lSetPlayerCarDriverAction)));

        const GameStateModuleIO::AwardSequenceEndAction lAwardSequenceEndAction;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAwardSequenceEndAction),
                                GameStateModuleIO::E_ACTION_AWARD_SEQUENCE_END,
                                static_cast<s32>(sizeof(lAwardSequenceEndAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_CAR_UNLOCK_TICKER_CLOSED:
        mCarSelectManager.OnCarUnlockTickerComplete();
        break;

    case GameStateModuleIO::E_EVENT_ROAD_RULE_DATA_REQUEST:
        mRoadRulesManager.OnRoadRulesDataRequest(
            reinterpret_cast<const GameStateModuleIO::RoadRulesDataRequestEvent*>(lpEvent)->mRoadId, lpActionQueue);
        break;

    case 104:
    {
        const GameStateModuleIO::EventDataRequestEvent* lpEventDataRequest =
            reinterpret_cast<const GameStateModuleIO::EventDataRequestEvent*>(lpEvent);
        CGS_ASSERT(lpEventDataRequest, "lpEventDataRequest");

        BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
        CGS_ASSERT(lpProfile, "mProgressionManager.GetProfile()");

        const GameStateModuleIO::TargetEventScore* lpTargetEventScore =
            lpProfile->GetTargetEvent(lpEventDataRequest->mEventId);
        if (lpTargetEventScore != 0)
        {
            GameStateModuleIO::TargetEventDataResponse lResponse;
            lResponse.mEventId = lpTargetEventScore->mEventId;
            lResponse.miScore  = lpTargetEventScore->miScore;
            // The record head opens with the holder's name.
            lResponse.mPlayerName.Construct(reinterpret_cast<const char*>(&lpTargetEventScore->mHead));

            CGS_ASSERT(lpActionQueue, "lpOutputActionQueue");
            lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResponse),
                                    GameStateModuleIO::TargetEventDataResponse::KI_GAME_ACTION_TYPE,
                                    static_cast<s32>(sizeof(lResponse)));
        }
        break;
    }

    case GameStateModuleIO::E_EVENT_ALL_RIVALRY_DATA_REQUEST:
        SendAllRivalryData(lpActionQueue);
        break;

    case GameStateModuleIO::E_EVENT_ONE_RIVALRY_DATA_REQUEST:
    {
        const GameStateModuleIO::RivalriesOneDataRequestEvent* lpRivalRequestEvent =
            reinterpret_cast<const GameStateModuleIO::RivalriesOneDataRequestEvent*>(lpEvent);

        CGS_ASSERT(mProgressionManager.GetProgressionData(), "mProgressionManager.GetProgressionData()");
        BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
        CGS_ASSERT(lpProfile, "mProgressionManager.GetProfile()");

        const BrnProgression::Rival* lpRival =
            mProgressionManager.GetProgressionData()->FindRival(lpRivalRequestEvent->mRivalID);
        // The console streams the rival id between the two halves of this message.
        CGS_ASSERT(lpRival, "GS: Rival with ID  does not exist! \n");

        GameStateModuleIO::RivalryOneInDepthAction lRivalryAction;
        lRivalryAction.mId           = lpRival->GetId();
        lRivalryAction.mCarId        = lpRival->GetCarId();
        lRivalryAction.meCountyIndex = BrnWorld::WorldRegion::DistrictToCounty(lpRival->GetDistrict());

        const BrnProgression::RivalData* lpRivalData = lpProfile->FindRival(lpRivalRequestEvent->mRivalID);
        if (lpRivalData == 0)
        {
            lRivalryAction.miRaceWins       = 0;
            lRivalryAction.miProgressStatus = 0;
        }
        else
        {
            lRivalryAction.miRaceWins = lpRivalData->miEventCount;
            const f32 lfProgressStatus =
                (lpRivalData->meState == BrnProgression::RivalData::E_STATE_BEATEN) ? 100.0f : 0.0f;
            lRivalryAction.miProgressStatus = static_cast<s8>(lfProgressStatus);
        }

        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRivalryAction),
                                GameStateModuleIO::E_ACTION_ONE_RIVALRY_DATA_RESPONSE,
                                static_cast<s32>(sizeof(lRivalryAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_ONLINE_COLLECTABLE:
    {
        const GameStateModuleIO::OnlineNetworkPlayerCollectableEvent* lpCollectableEvent =
            reinterpret_cast<const GameStateModuleIO::OnlineNetworkPlayerCollectableEvent*>(lpEvent);
        CGS_ASSERT(lpCollectableEvent, "lpCollectableEvent");

        // Only a collectable the local player still lacks is passed on, on the output buffer's queue.
        if (!mProgressionManager.GetProfile()->IsStuntElementDone(lpCollectableEvent->meType, lpCollectableEvent->mID))
        {
            GameStateModuleIO::OnlineNetworkPlayerCollectableAction lCollectableAction;
            lCollectableAction.mID              = lpCollectableEvent->mID;
            lCollectableAction.mNetworkPlayerID = lpCollectableEvent->mNetworkPlayerID;
            lCollectableAction.meType           = lpCollectableEvent->meType;
            lpOutput->GetGameActionQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lCollectableAction),
                                                     GameStateModuleIO::E_ACTION_NETWORK_COLLECTABLE,
                                                     static_cast<s32>(sizeof(lCollectableAction)));
        }
        break;
    }

    case GameStateModuleIO::E_EVENT_ONLINE_IMAGE_RECEIVED:
        GetMugshotManager()->ProcessImageReceivedEvent(reinterpret_cast<const OnlineImageReceivedEvent*>(lpEvent));
        break;

    case GameStateModuleIO::E_EVENT_ONLINE_CAPTURE_WINNER_PHOTO_FINISH:
        GetMugshotManager()->ProcessOnlineWin(lpOutput);
        break;

    case GameStateModuleIO::E_EVENT_IMAGE_TO_SAVE:
        GetImageManager()->HandleImageSaveEvent(reinterpret_cast<const ImageToSaveEvent*>(lpEvent), lpOutput,
                                                mModeManager.IsInPostEvent());
        break;

    case GameStateModuleIO::E_EVENT_IMAGE_FILES_SAVED:
        GetImageManager()->HandleImageFilesSavedEvent(reinterpret_cast<const ImageFilesSavedEvent*>(lpEvent));
        break;

    case GameStateModuleIO::E_EVENT_IMAGE_GALLERY_REQUEST:
        GetImageManager()->HandleImageGalleryRequest(reinterpret_cast<const ImageGalleryRequestEvent*>(lpEvent),
                                                     lpOutput);
        break;

    case GameStateModuleIO::E_EVENT_IMAGE_GALLERY_COUNT_REQUEST:
        GetImageManager()->HandleImageGalleryCountRequest(
            reinterpret_cast<const ImageGalleryCountRequestEvent*>(lpEvent), lpOutput);
        break;

    case GameStateModuleIO::E_EVENT_IMAGE_GALLERY_DATA_REQUEST:
        GetImageManager()->HandleImageGalleryDataRequest(
            reinterpret_cast<const ImageGalleryDataRequestEvent*>(lpEvent), lpOutput);
        break;

    case GameStateModuleIO::E_EVENT_ROAD_RULE_INTERACTION_CHANGE:
        mRoadRulesManager.SetSwitchingActive(
            reinterpret_cast<const GameStateModuleIO::RoadRuleInteractionChangeEvent*>(lpEvent)->mbSwitchRoadRulesEnabled);
        break;

    case GameStateModuleIO::E_EVENT_ROAD_RULE_MODE_SWITCH:
        mRoadRulesManager.SetRoadRulesMode(
            lpOutput, reinterpret_cast<const GameStateModuleIO::RoadRuleModeSwitchEvent*>(lpEvent)->mbIsOnline);
        break;

    case GameStateModuleIO::E_EVENT_ONLINE_ROAD_RULES_CONNECT_INFO:
    {
        const GameStateModuleIO::OnlineRoadRulesConnectInfoEvent* lpRRConnectedOnlineEvent =
            reinterpret_cast<const GameStateModuleIO::OnlineRoadRulesConnectInfoEvent*>(lpEvent);
        CGS_ASSERT(lpRRConnectedOnlineEvent, "lpRRConnectedOnlineEvent");
        mStreetManager.ProcessConnectedOnlineEvent(lpOutput, lpRRConnectedOnlineEvent);
        break;
    }

    case GameStateModuleIO::E_EVENT_ROAD_RULE_ROAD_SCORE_REQUEST:
        mStreetManager.ProcessScoreRequestEvent(
            lpOutput, reinterpret_cast<const GameStateModuleIO::RoadRulesScoreRequestEvent*>(lpEvent));
        break;

    case GameStateModuleIO::E_EVENT_IMAGE_FILES_LOADED:
        GetImageManager()->HandleImageFilesLoadedEvent(reinterpret_cast<const ImageFilesLoadedEvent*>(lpEvent),
                                                       lpOutput);
        break;

    case GameStateModuleIO::E_EVENT_ONLINE_IMAGE_SEND_ABORTED:
        GetMugshotManager()->ProcessAbortCaptureEvent(reinterpret_cast<const OnlineImageCaptureAbortedEvent*>(lpEvent));
        break;

    case GameStateModuleIO::E_EVENT_ROAD_RULE_BATCH_DATA_REQUEST:
    {
        GameStateModuleIO::RoadRulesBatchQueryAction lRoadRulesQuery;
        mStreetManager.FillInRoadRulesQuery(&lRoadRulesQuery);
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRoadRulesQuery),
                                GameStateModuleIO::E_ACTION_ROAD_RULES_BATCH_QUERY,
                                static_cast<s32>(sizeof(lRoadRulesQuery)));
        break;
    }

    case GameStateModuleIO::E_EVENT_GUI_SWITCHES_ROAD_RULE_STATE:
    {
        const GameStateModuleIO::GUISwitchRoadRuleStateEvent* lpSwitchEvent =
            reinterpret_cast<const GameStateModuleIO::GUISwitchRoadRuleStateEvent*>(lpEvent);

        switch (lpSwitchEvent->meRoadRuleState)
        {
        case GameStateModuleIO::GUISwitchRoadRuleStateEvent::E_ROAD_RULE_STATE_OFF:
            mRoadRulesManager.SetActiveRoadRule(lpActionQueue, E_ACTIVE_ROAD_RULE_NONE);
            break;

        case GameStateModuleIO::GUISwitchRoadRuleStateEvent::E_ROAD_RULE_STATE_TIME:
            if (mModeManager.IsOnlineGameMode())
            {
                mRoadRulesManager.SetActiveRoadRule(lpActionQueue, E_ACTIVE_ROAD_RULE_ONLINE_TIME);
            }
            else
            {
                mRoadRulesManager.SetActiveRoadRule(lpActionQueue, E_ACTIVE_ROAD_RULE_OFFLINE_TIME);
            }
            break;

        case GameStateModuleIO::GUISwitchRoadRuleStateEvent::E_ROAD_RULE_STATE_CRASH:
            if (mModeManager.IsOnlineGameMode())
            {
                mRoadRulesManager.SetActiveRoadRule(lpActionQueue, E_ACTIVE_ROAD_RULE_ONLINE_CRASH);
            }
            else
            {
                mRoadRulesManager.SetActiveRoadRule(lpActionQueue, E_ACTIVE_ROAD_RULE_OFFLINE_CRASH);
            }
            break;

        default:
            CGS_ASSERT(false, "Unknown road rule");
            break;
        }
        break;
    }

    case GameStateModuleIO::E_EVENT_BUDDY_REMOVED:
    {
        const GameStateModuleIO::BuddyRemovedEvent* lpBuddyRemovedEvent =
            reinterpret_cast<const GameStateModuleIO::BuddyRemovedEvent*>(lpEvent);
        CGS_ASSERT(lpBuddyRemovedEvent, "lpBuddyRemovedEvent");
        mStreetManager.ProcessBuddyRemoved(lpOutput, lpBuddyRemovedEvent);
        break;
    }

    case GameStateModuleIO::E_EVENT_INSTANT_FREEBURN:
    {
        const GameStateModuleIO::InstantFreeburnEvent* lpInstantFreeburnEvent =
            reinterpret_cast<const GameStateModuleIO::InstantFreeburnEvent*>(lpEvent);
        CGS_ASSERT(lpInstantFreeburnEvent, "lpInstantFreeburnEvent");

        // No car unlocks while an instant free-burn is under way.
        mCarSelectManager.SetCarUnlockEnabled(!lpInstantFreeburnEvent->mbIsDoingInstantFreeburn);
        break;
    }

    case 152:
    {
        const GameStateModuleIO::TargetEventScoreEvent* lpTargetEventScore =
            reinterpret_cast<const GameStateModuleIO::TargetEventScoreEvent*>(lpEvent);
        CGS_ASSERT(lpTargetEventScore, "lpTargetEventScore");

        BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
        CGS_ASSERT(lpProfile, "lpProfile");

        if (lpTargetEventScore->mbRemove)
        {
            lpProfile->RemoveTargetEventScore(lpTargetEventScore->mEventId);
        }
        else
        {
            lpProfile->SetTargetEventScore(lpTargetEventScore->mHead, lpTargetEventScore->mEventId,
                                           lpTargetEventScore->miScore);
        }

        GameStateModuleIO::TargetEventScoreChangedResponse lResponse;
        // The record head opens with the holder's name.
        lResponse.mPlayerName.Construct(reinterpret_cast<const char*>(&lpTargetEventScore->mHead));
        lResponse.mbRemoved = lpTargetEventScore->mbRemove;

        CGS_ASSERT(lpActionQueue, "lpOutputActionQueue");
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResponse),
                                GameStateModuleIO::TargetEventScoreChangedResponse::KI_GAME_ACTION_TYPE,
                                static_cast<s32>(sizeof(lResponse)));
        break;
    }

    case 153:
    {
        const GameStateModuleIO::DldScoreboardEvent* lpDldScoreboardEvent =
            reinterpret_cast<const GameStateModuleIO::DldScoreboardEvent*>(lpEvent);
        CGS_ASSERT(lpDldScoreboardEvent, "lpDldScoreboardEvent");

        BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
        CGS_ASSERT(lpProfile, "lpProfile");

        const GameStateModuleIO::TargetEventScore* lpTargetEventScore =
            lpProfile->GetTargetEvent(lpDldScoreboardEvent->mEventId);
        if (lpTargetEventScore != 0)
        {
            GameStateModuleIO::DldScoreboardResponse lResponse;
            // The record head opens with the holder's name.
            lResponse.mPlayerName.Construct(reinterpret_cast<const char*>(&lpTargetEventScore->mHead));

            CGS_ASSERT(lpActionQueue, "lpOutputActionQueue");
            lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResponse),
                                    GameStateModuleIO::DldScoreboardResponse::KI_GAME_ACTION_TYPE,
                                    static_cast<s32>(sizeof(lResponse)));
        }
        break;
    }

    case 175:
    {
        const GameStateModuleIO::UploadedModeScoresEvent* lpUploadedModeScoresEvent =
            reinterpret_cast<const GameStateModuleIO::UploadedModeScoresEvent*>(lpEvent);
        CGS_ASSERT(lpUploadedModeScoresEvent, "lpUploadedModeScoresEvent");

        BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
        CGS_ASSERT(lpProfile, "lpProfile");

        for (u32 luIndex = 0; luIndex < lpUploadedModeScoresEvent->maEventIds.GetLength(); ++luIndex)
        {
            lpProfile->RemoveEventScoreToUpload(lpUploadedModeScoresEvent->maEventIds.GetItem(luIndex));
        }
        break;
    }
    }
}

}

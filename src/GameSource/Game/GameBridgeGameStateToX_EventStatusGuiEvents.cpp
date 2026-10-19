// ARTIST 0x823EE9C8..0x823EF19C: direct status/output-building portion of
// BrnGameModule::BridgeGameStateToGui. The wrapper performs the original input
// assertions and raw GUI-queue append before entering this portion.
#include "GameSource/Game/GameBridgeGameStateToX.h"
#include "GameSource/BurnoutConstants.h"
#include "GameSource/GameState/BrnGameStateModuleIO.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"
#include "GameSource/Gui/Events/BrnGuiEventOnlinePostEvent.h"
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"
#include "GameShared/GameClasses/System/Timer/CgsTime.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnGame
{
namespace
{
    using namespace BrnGameState::GameStateModuleIO;

    // DecFIGS names this inline helper. ARTIST 0x823EEBB4..0x823EEC10:
    // the first valid blue-team lane is the runner in mode 13.
    EActiveRaceCarIndex GetOnlineRunnerRaceCarIndex(
        const ScoringOutputInterface* lpScoringOutputInterface,
        const OnlineScoringOutputInterface* lpOnlineScoringOutputInterface)
    {
        for (EActiveRaceCarIndex leCar = E_ACTIVE_RACE_CAR_INDEX_0;
             leCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; leCar++)
        {
            if (lpScoringOutputInterface->mabValid[leCar] &&
                lpOnlineScoringOutputInterface->maePlayerTeam[leCar] == E_PLAYER_TEAM_BLUE_TEAM)
                return leCar;
        }
        return E_ACTIVE_RACE_CAR_INDEX_INVALID;
    }

    // Existing PC boundary retained pending the below-stage-8 owner/index trace.
    // ARTIST has bare player-indexed reads at 823EEB14, 823EEB74 and 823EEC78.
    // This guard is not asserted to be original behavior.
    bool IsPlayerRaceCarIndexInRange(s32 liIndex)
    {
        return liIndex >= E_ACTIVE_RACE_CAR_INDEX_0 && liIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT;
    }
}

void BridgeGameStateToGui_EventStatus(
    const CgsSystem::TimerStatusInterface* lpTimerStatusInterface,
    const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput,
    CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput)
{
    using namespace BrnGameState::GameStateModuleIO;
    const ScoringOutputInterface* lpScoringOutputInterface = lpGameStateOutput->GetScoringOutputInterface();
    const OnlineScoringOutputInterface* lpOnlineScoringOutputInterface = lpGameStateOutput->GetOnlineScoringOutputInterface();

    BrnGui::GuiEventRaceDistanceRemaining lDistanceRemaining;
    for (EActiveRaceCarIndex leCar = E_ACTIVE_RACE_CAR_INDEX_0;
         leCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; leCar++)
    {
        if (lpScoringOutputInterface->mabValid[leCar] == 1)
        {
            const EGameModeType leMode = lpScoringOutputInterface->meGameModeType;
            lDistanceRemaining.mafDistanceToFinish[leCar] =
                (leMode == E_MODE_ONLINE_FREE_BURN_LOBBY || leMode == E_MODE_ONLINE_SHOWTIME)
                ? static_cast<f32>(lpScoringOutputInterface->maiNumRoadsRuled[leCar])
                : lpScoringOutputInterface->maCarScoreData[leCar].GetDistanceToFinishLive();
            lDistanceRemaining.maCarId[leCar] = lpScoringOutputInterface->maCarIds[leCar];
            lDistanceRemaining.mabPlayerEliminated[leCar] = lpScoringOutputInterface->mabPlayerEliminated[leCar];
            lDistanceRemaining.mabValid[leCar] = true;
            lDistanceRemaining.maiOnlineStuntScore[leCar] = lpScoringOutputInterface->maCarScoreData[leCar].GetOnlineStuntScore();
        }
        else
        {
            lDistanceRemaining.mafDistanceToFinish[leCar] = 0.0f;
            lDistanceRemaining.maCarId[leCar] = 0;
            lDistanceRemaining.mabPlayerEliminated[leCar] = false;
            lDistanceRemaining.mabValid[leCar] = false;
            lDistanceRemaining.maiOnlineStuntScore[leCar] = 0;
        }
    }
    PushGuiEvent(lDistanceRemaining, lpGuiInput); // 239 / 144

    const s32 liPlayer = static_cast<s32>(lpScoringOutputInterface->mePlayerRaceCarIndex);
    BrnGui::GuiEventRaceDistanceToCheckpoint lDistanceToCheckpoint;
    lDistanceToCheckpoint.mfDistanceToCheckpoint = 0.0f;
    if (IsPlayerRaceCarIndexInRange(liPlayer))
        lDistanceToCheckpoint.mfDistanceToCheckpoint = lpScoringOutputInterface->maCarScoreData[liPlayer].GetDistanceToNextCheckpointLive();
    PushGuiEvent(lDistanceToCheckpoint, lpGuiInput); // 240 / 4

    const CgsSystem::TimerStatus* lpGameTimerStatus = lpTimerStatusInterface->GetGameTimerStatus();
    CGS_ASSERT(lpGameTimerStatus != 0, "mTimerStatusInterface.GetGameTimerStatus()");
    const EGameModeType leGameMode = lpScoringOutputInterface->meGameModeType;
    BrnGui::GuiEventCurrentStatus lCurrentStatus;
    const CgsSystem::Time lGameTime = lpGameTimerStatus->GetTime();
    lCurrentStatus.miGameTimeSeconds = lGameTime.GetSeconds();
    lCurrentStatus.mfGameTimeFraction = lGameTime.GetFraction();
    lCurrentStatus.mfGameTimeStep = lpGameTimerStatus->GetCurrentTimeStep();
    lCurrentStatus.mfDistanceToFinishLive = 0.0f;
    if (IsPlayerRaceCarIndexInRange(liPlayer))
        lCurrentStatus.mfDistanceToFinishLive = lpScoringOutputInterface->maCarScoreData[liPlayer].GetDistanceToFinishLive();
    lCurrentStatus.mfDistanceDrivenInCurrentCar = lpScoringOutputInterface->mfDistanceDrivenInCurrentCar;
    for (s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar)
        lCurrentStatus.maePlayerTeam[liCar] = static_cast<s32>(lpOnlineScoringOutputInterface->maePlayerTeam[liCar]);

    // No blanket zero fill: original unused checkpoint-tail bytes are not initialized.
    EActiveRaceCarIndex leRunner = E_ACTIVE_RACE_CAR_INDEX_INVALID;
    if (leGameMode == E_MODE_ONLINE_BURNING_HOME_RUN)
        leRunner = GetOnlineRunnerRaceCarIndex(lpScoringOutputInterface, lpOnlineScoringOutputInterface);
    if (leRunner == E_ACTIVE_RACE_CAR_INDEX_INVALID)
        lCurrentStatus.miNumRemainingCheckpoints = 0;
    else
    {
        CGS_ASSERT(E_ACTIVE_RACE_CAR_INDEX_COUNT > leRunner, "E_ACTIVE_RACE_CAR_INDEX_COUNT > leRunnerActiveRaceCarIndex");
        CGS_ASSERT(E_ACTIVE_RACE_CAR_INDEX_0 <= leRunner, "E_ACTIVE_RACE_CAR_INDEX_0 <= leRunnerActiveRaceCarIndex");
        lCurrentStatus.miNumRemainingCheckpoints = lpScoringOutputInterface->maCarCheckpointData[leRunner]
            .GetAllRemainingCheckpointIndexes(lCurrentStatus.maiRemainingCheckpointIndexes);
    }
    PushGuiEvent(lCurrentStatus, lpGuiInput); // 492 / 120

    const bool lbShowtime = leGameMode == E_MODE_OFFLINE_SHOWTIME || leGameMode == E_MODE_ONLINE_SHOWTIME;
    if (lbShowtime || leGameMode != E_MODE_NONE)
    {
        BrnGui::GuiEventScoreUpdate lScore;
        lScore.meCurrentMedalTarget = static_cast<s32>(lpScoringOutputInterface->meCurrentMedalTarget);
        lScore.mfModeTime = lpScoringOutputInterface->mfModeTimeElapsed;
        lScore.mfDistanceToNextCheckpoint = 0.0f;
        if (IsPlayerRaceCarIndexInRange(liPlayer))
            lScore.mfDistanceToNextCheckpoint = lpScoringOutputInterface->maCarScoreData[liPlayer].GetDistanceToNextCheckpointLive();
        switch (leGameMode)
        {
        case E_MODE_ROAD_RAGE: case E_MODE_STUNT_ATTACK: case E_MODE_ONLINE_FUGITIVE:
        case E_MODE_ONLINE_FREE_BURN: case E_MODE_ONLINE_MODE_END:
            lScore.mfModeTime = lpScoringOutputInterface->mfModeTimeRemaining >= 0.0f
                ? lpScoringOutputInterface->mfModeTimeRemaining : 0.0f;
            break;
        default: break;
        }
        lScore.mfCurrentTargetModeTime = lpScoringOutputInterface->mfCurrentTargetModeTime;
        lScore.mbTimerActive = lpScoringOutputInterface->mbTimerActive;
        PushGuiEvent(lScore, lpGuiInput); // 424 / 20; native padding is not zero-filled

        switch (leGameMode)
        {
        case E_MODE_OFFLINE_SHOWTIME: case E_MODE_ONLINE_SHOWTIME:
        {
            BrnGui::GuiCrashScoreUpdate lScore;
            lScore.miCarsCrashed = lpScoringOutputInterface->miShowtimeCarsCrashed;
            lScore.mfDistanceTravelled = lpScoringOutputInterface->mfShowtimeDistanceTravelled;
            lScore.miComboMultiplier = lpScoringOutputInterface->miShowtimeComboMultiplier;
            lScore.miScoreMultiplier = lpScoringOutputInterface->miShowtimeScoreMultiplier;
            PushGuiEvent(lScore, lpGuiInput); // 434 / 16
            break;
        }
        case E_MODE_ROAD_RAGE:
        {
            BrnGui::GuiRoadRageScoreUpdate lScore;
            lScore.miCurrentTakedowns = lpScoringOutputInterface->miRoadRageNumTakedowns;
            lScore.miTargetTakedowns = lpScoringOutputInterface->miRoadRageTakedownTarget;
            PushGuiEvent(lScore, lpGuiInput); // 426 / 8
            break;
        }
        case E_MODE_PURSUIT:
        {
            BrnGui::GuiPursuitScoreUpdate lScore;
            lScore.miRivalDamage = lpScoringOutputInterface->miPursuitCarDamageLeft;
            PushGuiEvent(lScore, lpGuiInput); // 432 / 4
            break;
        }
        case E_MODE_STUNT_ATTACK: case E_MODE_TRAFFIC_ATTACK: case E_MODE_ONLINE_FUGITIVE:
        case E_MODE_ONLINE_FREE_BURN: case E_MODE_ONLINE_MODE_END:
        {
            BrnGui::GuiAttackScoreUpdate lScore;
            lScore.miCurrentScore = lpScoringOutputInterface->miCurrentScore;
            lScore.mfComboWarningTimeActive = lpScoringOutputInterface->mfComboWarningTimeActive;
            lScore.miTargetScore = lpScoringOutputInterface->miTargetScore;
            lScore.miComboScore = lpScoringOutputInterface->miComboScore;
            lScore.miComboMultiplier = lpScoringOutputInterface->miComboMultiplier;
            lScore.muCurrentStunts = lpScoringOutputInterface->muCurrentStunts;
            lScore.muAllStunts = lpScoringOutputInterface->muAllStunts;
            lScore.mbComboWarningActive = lpScoringOutputInterface->mbComboWarningActive;
            lScore.mbComboInProgress = lpScoringOutputInterface->mbComboInProgress;
            lScore.meStuntToDisplayType = static_cast<s32>(lpScoringOutputInterface->maStunts[0].meStuntType);
            lScore.miStuntToDisplayScore = lpScoringOutputInterface->maStunts[0].miStuntScore;
            PushGuiEvent(lScore, lpGuiInput); // 428 / 40
            break;
        }
        default: break;
        }
    }

    if (lpScoringOutputInterface->mbIsOnlineGameMode && lpScoringOutputInterface->miNumPlayersInGame > 0)
    {
        // Do not call Clear(): this stack instance only initializes its Time members
        // before the original assignments; +2C, unused records/awards and padding remain unwritten.
        BrnGui::GuiEventOnlinePostEvent lPostEvent;
        CarScoreData lCarScoreData;
        for (s32 liTeam = 0; liTeam < E_PLAYER_TEAM_COUNT; ++liTeam)
            lPostEvent.maHeader[liTeam] = static_cast<u32>(lpScoringOutputInterface->maiTeamStuntScores[liTeam]);
        lPostEvent.miNumPlayersInEvent = lpScoringOutputInterface->miNumPlayersInGame;
        lPostEvent.miNumPlayersFinishedEvent = 0;
        lPostEvent.miNumAwardsGiven = 0;
        BrnGui::GuiEventOnlinePostEvent::Record* lpRecord = lPostEvent.maRecords;
        for (EActiveRaceCarIndex leCar = E_ACTIVE_RACE_CAR_INDEX_0;
             leCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; leCar++)
        {
            if (lpScoringOutputInterface->mabValid[leCar])
            {
                lCarScoreData = lpScoringOutputInterface->maCarScoreData[leCar];
                const CgsSystem::Time lFinishTime = lCarScoreData.GetFinishTime();
                lpRecord->miIndex = leCar;
                lpRecord->mfValue04 = static_cast<f32>(lFinishTime.GetSeconds()) + lFinishTime.GetFraction();
                lpRecord->mTime = lFinishTime;
                lpRecord->mfValue10 = lCarScoreData.GetDistanceToFinish();
                lpRecord->muValue14 = static_cast<u32>(lCarScoreData.GetOnlineStuntScore());
                lpRecord->muValue18 = static_cast<u32>(lCarScoreData.GetOnlineFinishPositionScore());
                lpRecord->muValue1C = static_cast<u32>(lCarScoreData.GetOnlineStandingsPosition());
                lpRecord->muValue20 = static_cast<u32>(lpScoringOutputInterface->maiCumulativeScoreData[leCar]);
                lpRecord->mbFlag34 = lCarScoreData.GetTimedOut();
                lpRecord->mbFlag35 = lCarScoreData.GetDisconnected();
                lpRecord->mbFlag36 = lCarScoreData.GetEliminatorRaceCarIndex() != E_ACTIVE_RACE_CAR_INDEX_INVALID;
                lpRecord->muValue24 = static_cast<u32>(lCarScoreData.GetNumEliminations());
                lpRecord->muValue28 = lCarScoreData.GetOnlinePostEventValueC0();
                lpRecord->muValue30 = static_cast<u32>(lCarScoreData.GetCumulativeCheckpoints());
                if (lpRecord->mfValue04 > 0.0f || lpRecord->mbFlag34 || lpRecord->mbFlag35)
                    ++lPostEvent.miNumPlayersFinishedEvent;
                if (static_cast<s32>(lpOnlineScoringOutputInterface->maOnlineAwards[leCar]) != -1)
                {
                    BrnGui::GuiEventOnlinePostEvent::OnlineAward& lrAward = lPostEvent.maOnlineAwards[lPostEvent.miNumAwardsGiven];
                    lrAward.meOnlineAwardID = static_cast<s32>(lpOnlineScoringOutputInterface->maOnlineAwards[leCar]);
                    lrAward.mePlayerActiveRaceCarIndex = leCar;
                    lrAward.miAwardVariable = lpOnlineScoringOutputInterface->maiOnlineAwardVariables[leCar];
                    ++lPostEvent.miNumAwardsGiven;
                }
                ++lpRecord;
            }
        }
        PushGuiEvent(lPostEvent, lpGuiInput); // 318 / 568
    }
}
}

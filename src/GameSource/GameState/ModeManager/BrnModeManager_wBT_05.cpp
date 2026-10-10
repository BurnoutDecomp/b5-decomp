// BrnGameState::ModeManager -- five handlers GameStateModule::ProcessGameEvents calls (group 5):
//   FinishedSplashScreen              case 23  the online splash finished: advance the mode
//   FinishedMapPan                    case 24  the intro map pan finished: advance the mode
//   MarkedManLoaded                   case 21  post action 31 (the mode and the lobby-move flag)
//   RemoteRaceCarHitsCheckpoint       case 141 a remote car went through a checkpoint
//   HandleBurningHomeRunRunnerSwitch  case 155 the network moved the Burning Home Run runner
// Every mode call goes through the GameMode virtual by name (slot 6 GetName, slot 9 HasTimedIntro,
// slot 12 SendEvent).

#include "GameSource/GameState/ModeManager/BrnModeManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT / Begin-Fire-EndAssert
#include "GameShared/GameClasses/Development/CgsStrStream.h"           // CgsDev::StrStream (streamed assert texts)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"       // VariableEventQueue::AddEvent
#include "GameSource/GameState/BrnGameStateModule.h"                   // GameStateModule::GetActiveRaceCarIndex / the two snapshots
#include "GameSource/GameState/BrnGameStateModuleIO.h"                 // OutputBuffer
#include "GameSource/GameState/BrnGameEvents.h"                        // BurningHomeRunSwitchRunnerEvent
#include "GameSource/GameState/BrnGameActions.h"                       // MarkedManLoadedAction (action 31)
#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"    // GameMode virtuals
#include "GameSource/GameState/ModeManager/GameModes/BrnOnlineBurningHomeRunMode.h"
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h" // CarData / GetNextTeamMember
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"

namespace BrnGameState
{

// Only an online mode sitting in its splash state moves on (E_GME_NEXT); anything else ignores it.
void ModeManager::FinishedSplashScreen()
{
    if (mpCurrentGameMode != nullptr && mpCurrentGameMode->GetCurrentState() == GameStateModuleIO::E_GMS_ONLINE_SPLASH)
    {
        mpCurrentGameMode->SendEvent(E_GME_NEXT);
    }
}

// A mode with a timed intro has no map pan, so being told one ended is an error; the mode is
// advanced either way.
void ModeManager::FinishedMapPan()
{
    if (mpCurrentGameMode == nullptr)
    {
        return;
    }

    if (mpCurrentGameMode->HasTimedIntro())
    {
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStrStream << "Should not be told map pan has ended if not using a map pan! Mode is "
                   << mpCurrentGameMode->GetName() << "\n";
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
        CgsDev::Assert::EndAssert();
    }

    mpCurrentGameMode->SendEvent(E_GME_NEXT);
}

// Tell the network side the marked-man data is in: the current mode type, and whether this is a
// move between the two instant-intro lobby modes.
void ModeManager::MarkedManLoaded(GameStateModuleIO::GameActionQueue* lpGameActionQueue)
{
    CGS_ASSERT(lpGameActionQueue != NULL, "lpGameActionQueue != NULL");

    GameStateModuleIO::MarkedManLoadedAction lMarkedManLoadedAction;
    lMarkedManLoadedAction.meGameMode                = meCurrentGameModeType;
    lMarkedManLoadedAction.mbMovingBetweenLobbyModes = IsOnlineModeWithInstantIntro();
    lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lMarkedManLoadedAction),
                                GameStateModuleIO::E_ACTION_MARKED_MAN_LOADED,
                                static_cast<s32>(sizeof(lMarkedManLoadedAction)));
}

// In an online mode other than the two lobby modes, run the landmark trigger for the remote car
// against checkpoint liCheckpointIndex, through the module's last active-car snapshot.
void ModeManager::RemoteRaceCarHitsCheckpoint(BrnNetwork::NetworkPlayerID lNetworkPlayerID, s32 liCheckpointIndex)
{
    if (!IsOnlineGameMode() || GameStateModuleIO::IsOnlineFreeBurnLobby(meCurrentGameModeType))
    {
        return;
    }

    const ::EActiveRaceCarIndex leRemotePlayersRaceCarIndex = mpGameStateModule->GetActiveRaceCarIndex(lNetworkPlayerID);
    CGS_ASSERT(leRemotePlayersRaceCarIndex >= ::E_ACTIVE_RACE_CAR_INDEX_0,
               "leRemotePlayersRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leRemotePlayersRaceCarIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "leRemotePlayersRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

    if (!(liCheckpointIndex < static_cast<s32>(muNumLandmarks)))
    {
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStrStream << "muNumLandmarks=" << muNumLandmarks << "liCheckpointIndex=" << liCheckpointIndex;
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
        CgsDev::Assert::EndAssert();
    }

    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutput =
        mpGameStateModule->GetLastActiveRaceCarInterface();
    const LandmarkIndex lLandmarkIndex(maLandmarkIndices[liCheckpointIndex]);
    RaceCarTriggersLandmark(lpActiveRaceCarOutput,
                            lpActiveRaceCarOutput->GetGlobalRaceCarIndex(leRemotePlayersRaceCarIndex),
                            leRemotePlayersRaceCarIndex, lLandmarkIndex, false);
}

// The network names the new runner. The old runner is the one blue-team car; if the two differ,
// the Burning Home Run mode swaps the teams, both cars' checkpoint records restart over the
// mode's landmarks and both scoring records clear their +0xC0 word.
void ModeManager::HandleBurningHomeRunRunnerSwitch(const GameStateModuleIO::BurningHomeRunSwitchRunnerEvent* lpEvent,
                                                   GameStateModuleIO::OutputBuffer*                         lpOutputBuffer)
{
    CGS_ASSERT(lpEvent, "lpEvent");
    CGS_ASSERT(lpOutputBuffer, "lpOutputBuffer");
    CGS_ASSERT(meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_BURNING_HOME_RUN,
               "meCurrentGameModeType == GsmIO::E_MODE_ONLINE_BURNING_HOME_RUN");

    // The post-increment carries the in-loop range assert.
    ::EActiveRaceCarIndex leOldRunnerActiveRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_INVALID;
    for (::EActiveRaceCarIndex leEnumIndex = ::E_ACTIVE_RACE_CAR_INDEX_0; leEnumIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT;
         leEnumIndex++)
    {
        const CarData* lpCarData = mScoringSystem.GetCarData(leEnumIndex);
        if (lpCarData != NULL && lpCarData->GetTeam() == GameStateModuleIO::E_PLAYER_TEAM_BLUE_TEAM)
        {
            leOldRunnerActiveRaceCarIndex = leEnumIndex;
            break;
        }
    }

    const ::EActiveRaceCarIndex leNewActiveRunnerRaceCarIndex =
        mpGameStateModule->GetActiveRaceCarIndex(lpEvent->mNewRunnerPlayerID);

    CGS_ASSERT(leOldRunnerActiveRaceCarIndex >= ::E_ACTIVE_RACE_CAR_INDEX_0,
               "leOldRunnerActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leOldRunnerActiveRaceCarIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "leOldRunnerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
    CGS_ASSERT(leNewActiveRunnerRaceCarIndex >= ::E_ACTIVE_RACE_CAR_INDEX_0,
               "leNewActiveRunnerRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leNewActiveRunnerRaceCarIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "leNewActiveRunnerRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

    CGS_ASSERT(mScoringSystem.GetNextTeamMember(leOldRunnerActiveRaceCarIndex, GameStateModuleIO::E_PLAYER_TEAM_BLUE_TEAM) ==
                   ::E_ACTIVE_RACE_CAR_INDEX_INVALID,
               "Oh dear, we appear to have two runners in our system, something has gone awefully wrong");

    if (leOldRunnerActiveRaceCarIndex == leNewActiveRunnerRaceCarIndex)
    {
        return;
    }

    mOnlineBurningHomeRun.SwitchBurningHomeRunRunner(leOldRunnerActiveRaceCarIndex, leNewActiveRunnerRaceCarIndex,
                                                     lpOutputBuffer);

    const ::EGlobalRaceCarIndex leOldRunnerGlobalRaceCarIndex =
        mpGameStateModule->GetLastGlobalRaceCarInterface()->GetGlobalRaceCarIndex(leOldRunnerActiveRaceCarIndex);
    const ::EGlobalRaceCarIndex leNewRunnerGlobalRaceCarIndex =
        mpGameStateModule->GetLastGlobalRaceCarInterface()->GetGlobalRaceCarIndex(leNewActiveRunnerRaceCarIndex);

    CGS_ASSERT(leOldRunnerGlobalRaceCarIndex >= ::E_GLOBAL_RACE_CAR_INDEX_0,
               "leOldRunnerGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0");
    CGS_ASSERT(leOldRunnerGlobalRaceCarIndex < ::E_GLOBAL_RACE_CAR_INDEX_COUNT,
               "leOldRunnerGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");
    CGS_ASSERT(leNewRunnerGlobalRaceCarIndex >= ::E_GLOBAL_RACE_CAR_INDEX_0,
               "leNewRunnerGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0");
    CGS_ASSERT(leNewRunnerGlobalRaceCarIndex < ::E_GLOBAL_RACE_CAR_INDEX_COUNT,
               "leNewRunnerGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");

    maCarCheckpointData[leNewRunnerGlobalRaceCarIndex].SetupCheckpoints(static_cast<s32>(muNumLandmarks));
    maCarCheckpointData[leOldRunnerGlobalRaceCarIndex].SetupCheckpoints(static_cast<s32>(muNumLandmarks));

    mScoringSystem.GetCarData(leNewActiveRunnerRaceCarIndex)->GetScoreData()->SetOnlinePostEventValueC0(0);
    mScoringSystem.GetCarData(leOldRunnerActiveRaceCarIndex)->GetScoreData()->SetOnlinePostEventValueC0(0);
}

}

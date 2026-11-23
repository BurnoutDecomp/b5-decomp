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
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
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
    if (mpCurrentGameMode != 0 && mpCurrentGameMode->GetCurrentState() == GameStateModuleIO::E_GMS_ONLINE_SPLASH)
    {
        mpCurrentGameMode->SendEvent(E_GME_NEXT);
    }
}

// A mode with a timed intro has no map pan, so being told one ended is an error; the mode is
// advanced either way.
void ModeManager::FinishedMapPan()
{
    if (mpCurrentGameMode == 0)
    {
        return;
    }

    if (mpCurrentGameMode->HasTimedIntro())
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::StrStream lStrStream(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
        lStrStream << "Should not be told map pan has ended if not using a map pan! Mode is "
                   << mpCurrentGameMode->GetName() << "\n";
        CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer,
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/ModeManager/BrnModeManager.cpp", 2506); // ARTIST 0x82311988
        CgsDev::Assert::EndAssert();
    }

    mpCurrentGameMode->SendEvent(E_GME_NEXT);
}

// Tell the network side the marked-man data is in: the current mode type, and whether this is a
// move between the two instant-intro lobby modes.
void ModeManager::MarkedManLoaded(GameStateModuleIO::GameActionQueue* lpGameActionQueue)
{
    CGS_ASSERT(lpGameActionQueue != NULL, CgsDev::Assert::MessageAt("lpGameActionQueue != NULL", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/ModeManager/BrnModeManager.cpp", 3981));

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
               CgsDev::Assert::MessageAt("leRemotePlayersRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/ModeManager/BrnModeManager.cpp", 2545));
    CGS_ASSERT(leRemotePlayersRaceCarIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT,
               CgsDev::Assert::MessageAt("leRemotePlayersRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/ModeManager/BrnModeManager.cpp", 2546));

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
    CGS_ASSERT(lpEvent, CgsDev::Assert::MessageAt("lpEvent", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/ModeManager/BrnModeManager.cpp", 5165));
    CGS_ASSERT(lpOutputBuffer, CgsDev::Assert::MessageAt("lpOutputBuffer", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/ModeManager/BrnModeManager.cpp", 5166));
    CGS_ASSERT(meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_BURNING_HOME_RUN,
               CgsDev::Assert::MessageAt("meCurrentGameModeType == GsmIO::E_MODE_ONLINE_BURNING_HOME_RUN", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/ModeManager/BrnModeManager.cpp", 5169));

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
               CgsDev::Assert::MessageAt("leOldRunnerActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/ModeManager/BrnModeManager.cpp", 5176));
    CGS_ASSERT(leOldRunnerActiveRaceCarIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT,
               CgsDev::Assert::MessageAt("leOldRunnerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/ModeManager/BrnModeManager.cpp", 5177));
    CGS_ASSERT(leNewActiveRunnerRaceCarIndex >= ::E_ACTIVE_RACE_CAR_INDEX_0,
               CgsDev::Assert::MessageAt("leNewActiveRunnerRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/ModeManager/BrnModeManager.cpp", 5179));
    CGS_ASSERT(leNewActiveRunnerRaceCarIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT,
               CgsDev::Assert::MessageAt("leNewActiveRunnerRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/ModeManager/BrnModeManager.cpp", 5180));

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
               CgsDev::Assert::MessageAt("leOldRunnerGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/ModeManager/BrnModeManager.cpp", 5199));
    CGS_ASSERT(leOldRunnerGlobalRaceCarIndex < ::E_GLOBAL_RACE_CAR_INDEX_COUNT,
               CgsDev::Assert::MessageAt("leOldRunnerGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/ModeManager/BrnModeManager.cpp", 5200));
    CGS_ASSERT(leNewRunnerGlobalRaceCarIndex >= ::E_GLOBAL_RACE_CAR_INDEX_0,
               CgsDev::Assert::MessageAt("leNewRunnerGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/ModeManager/BrnModeManager.cpp", 5202));
    CGS_ASSERT(leNewRunnerGlobalRaceCarIndex < ::E_GLOBAL_RACE_CAR_INDEX_COUNT,
               CgsDev::Assert::MessageAt("leNewRunnerGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/ModeManager/BrnModeManager.cpp", 5203));

    maCarCheckpointData[leNewRunnerGlobalRaceCarIndex].SetupCheckpoints(static_cast<s32>(muNumLandmarks));
    maCarCheckpointData[leOldRunnerGlobalRaceCarIndex].SetupCheckpoints(static_cast<s32>(muNumLandmarks));

    mScoringSystem.GetCarData(leNewActiveRunnerRaceCarIndex)->GetScoreData()->SetOnlinePostEventValueC0(0);
    mScoringSystem.GetCarData(leOldRunnerActiveRaceCarIndex)->GetScoreData()->SetOnlinePostEventValueC0(0);
}

}

// BrnGameState::OnlineBurningHomeRunMode::SwitchBurningHomeRunRunner -- its one caller is
// ModeManager::HandleBurningHomeRunRunnerSwitch (ProcessGameEvents case 155). The old runner goes to
// the red team with full infinite boost, the new runner to the blue team with none; the HUD gets a
// team-change message for each, a local new runner gets a second of invulnerability, and the
// runner counters restart from the new runner's crash count. A switch arriving less than a second
// after the previous one is ignored.

#include "GameSource/GameState/ModeManager/GameModes/BrnOnlineBurningHomeRunMode.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"             // gpDebugPrint, gxMessageFilterFlags
#include "GameShared/GameClasses/Development/CgsStrStream.h"           // StrStreamBase::operator<<
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"       // VariableEventQueue::AddEvent
#include "GameSource/GameState/BrnGameStateModule.h"                   // GameStateModule::GetPlayerActiveRaceCarIndex
#include "GameSource/GameState/BrnGameStateModuleIO.h"                 // OutputBuffer::GetGameActionQueue
#include "GameSource/GameState/BrnGameActions.h"                       // SetBoostAction (170), PlayerInvulnerableAction (111)
#include "GameSource/GameState/ModeManager/BrnModeManager.h"           // GetScoringSystem / GetHUDMessageLogic / GetGameStateModule
#include "GameSource/GameState/ModeManager/Hud/BrnHUDMessageLogic.h"   // HUDMessageLogic::OnlineTeamChange
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h" // SetPlayerTeam / GetCarData

namespace BrnGameState
{
namespace
{
    // Rodata 1.0f: the minimum time between two runner switches, and the new local runner's
    // invulnerability time.
    const f32 KF_RUNNER_SWITCH_MIN_SECONDS        = 1.0f;
    const f32 KF_NEW_RUNNER_INVULNERABLE_SECONDS  = 1.0f;
    // Rodata 100.0f / 0.0f and the two segment counts the two boost records carry.
    const f32 KF_OLD_RUNNER_BOOST_AMOUNT          = 100.0f;
    const f32 KF_NEW_RUNNER_BOOST_AMOUNT          = 0.0f;
    const s32 KI_OLD_RUNNER_BOOST_SEGMENTS        = 5;
    const s32 KI_NEW_RUNNER_BOOST_SEGMENTS        = 1;
}

void OnlineBurningHomeRunMode::SwitchBurningHomeRunRunner(::EActiveRaceCarIndex            leOldRunnerRaceCarIndex,
                                                          ::EActiveRaceCarIndex            leNewRunnerRaceCarIndex,
                                                          GameStateModuleIO::OutputBuffer* lpOutputBuffer)
{
    CGS_ASSERT(leOldRunnerRaceCarIndex >= ::E_ACTIVE_RACE_CAR_INDEX_0,
               "leOldRunnerRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leOldRunnerRaceCarIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "leOldRunnerRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
    CGS_ASSERT(leNewRunnerRaceCarIndex >= ::E_ACTIVE_RACE_CAR_INDEX_0,
               "leNewRunnerRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leNewRunnerRaceCarIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "leNewRunnerRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

    CGS_ASSERT(mpModeManager != NULL, "mpModeManager");
    CGS_ASSERT(mpModeManager->GetScoringSystem() != NULL, "mpModeManager->GetScoringSystem()");
    CGS_ASSERT(mpModeManager->GetHUDMessageLogic() != NULL, "mpModeManager->GetHUDMessageLogic()");

    if (mfTimeSinceRunnerSwitch < KF_RUNNER_SWITCH_MIN_SECONDS)
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "INFO: Ignored switch runner event\n";
        }
        return;
    }

    mpModeManager->GetScoringSystem()->SetPlayerTeam(leOldRunnerRaceCarIndex, GameStateModuleIO::E_PLAYER_TEAM_RED_TEAM);
    mpModeManager->GetScoringSystem()->SetPlayerTeam(leNewRunnerRaceCarIndex, GameStateModuleIO::E_PLAYER_TEAM_BLUE_TEAM);
    mpModeManager->GetHUDMessageLogic()->OnlineTeamChange(leOldRunnerRaceCarIndex);
    mpModeManager->GetHUDMessageLogic()->OnlineTeamChange(leNewRunnerRaceCarIndex);

    const s32 lxAllBoostFlags = GameStateModuleIO::SetBoostAction::KX_SET_INFINITE_BOOST_FLAG |
                                GameStateModuleIO::SetBoostAction::KX_SET_BOOST_AMOUNT |
                                GameStateModuleIO::SetBoostAction::KX_SET_BOOST_SEGMENTS;

    GameStateModuleIO::SetBoostAction lSetBoostAction;
    lSetBoostAction.meRaceCarIndex  = leOldRunnerRaceCarIndex;
    lSetBoostAction.mxFlags         = lxAllBoostFlags;
    lSetBoostAction.mfBoostAmount   = KF_OLD_RUNNER_BOOST_AMOUNT;
    lSetBoostAction.miBoostSegments = KI_OLD_RUNNER_BOOST_SEGMENTS;
    lSetBoostAction.mbInfiniteBoost = true;
    lpOutputBuffer->GetGameActionQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSetBoostAction),
                                                   GameStateModuleIO::E_ACTION_SET_BOOST,
                                                   static_cast<s32>(sizeof(lSetBoostAction)));

    lSetBoostAction.meRaceCarIndex  = leNewRunnerRaceCarIndex;
    lSetBoostAction.mxFlags         = lxAllBoostFlags;
    lSetBoostAction.mfBoostAmount   = KF_NEW_RUNNER_BOOST_AMOUNT;
    lSetBoostAction.miBoostSegments = KI_NEW_RUNNER_BOOST_SEGMENTS;
    lSetBoostAction.mbInfiniteBoost = false;
    lpOutputBuffer->GetGameActionQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSetBoostAction),
                                                   GameStateModuleIO::E_ACTION_SET_BOOST,
                                                   static_cast<s32>(sizeof(lSetBoostAction)));

    if (leNewRunnerRaceCarIndex == mpModeManager->GetGameStateModule()->GetPlayerActiveRaceCarIndex())
    {
        GameStateModuleIO::PlayerInvulnerableAction lInvulnerableAction;
        lInvulnerableAction.mfInvulnerableTime = KF_NEW_RUNNER_INVULNERABLE_SECONDS;
        lpOutputBuffer->GetGameActionQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lInvulnerableAction),
                                                       GameStateModuleIO::E_ACTION_PLAYER_INVULNERABLE,
                                                       static_cast<s32>(sizeof(lInvulnerableAction)));
    }

    const CarData* lpCarData = mpModeManager->GetScoringSystem()->GetCarData(leNewRunnerRaceCarIndex);
    CGS_ASSERT(lpCarData, "lpCarData");
    CGS_ASSERT(lpCarData->GetScoreData(), "lpCarData->GetScoreData()");

    miNumCrashesWhenPlayerBecameRunner = lpCarData->GetScoreData()->GetMarkedManTakedownsFor();
    miNumCrashesAsRunner               = 0;
    mfTimeSinceRunnerSwitch            = 0.0f;
}

}

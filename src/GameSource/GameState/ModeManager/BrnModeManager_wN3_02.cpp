// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/BrnModeManager_wN3_02.cpp
// ============================================================================
// Partfile of the BrnGameState::ModeManager TU (owning header BrnModeManager.h).
// ModeManager::HandleNewHostEvent, GameStateModule::ProcessGameEvents case 140. It only acts in
// Burning Home Run: when this machine has just become host (and is not the first host), and the
// runner's car is disconnected, the new host picks the next runner. Every other mode, the
// free-burn lobby included, ignores the host change here.
// ============================================================================

#include "GameSource/GameState/ModeManager/BrnModeManager.h"

#include "GameSource/GameState/BrnGameEvents.h"         // GameStateModuleIO::OnlineNewHostEvent
#include "GameSource/GameState/BrnGameStateModuleIO.h"  // OutputBuffer::GetGameActionQueue
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnGameState
{

void ModeManager::HandleNewHostEvent(
        const GameStateModuleIO::OnlineNewHostEvent* lpEvent,
        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutput,
        GameStateModuleIO::OutputBuffer* lpOutputBuffer)
{
    if (meCurrentGameModeType != GameStateModuleIO::E_MODE_ONLINE_BURNING_HOME_RUN ||
        !lpEvent->mbIsLocalPlayerNowHost ||
        lpEvent->mbIsFirstHost)
    {
        return;
    }

    // The runner is the one blue-team car. The post-increment carries the in-loop range assert.
    ::EActiveRaceCarIndex leRunnerIndex = ::E_ACTIVE_RACE_CAR_INDEX_0;
    for (; leRunnerIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT; leRunnerIndex++)
    {
        const CarData* lpCarData = mScoringSystem.GetCarData(leRunnerIndex);
        if (lpCarData != NULL && lpCarData->GetTeam() == GameStateModuleIO::E_PLAYER_TEAM_BLUE_TEAM)
        {
            break;
        }
    }
    if (leRunnerIndex >= ::E_ACTIVE_RACE_CAR_INDEX_COUNT)
    {
        return;
    }

    if (leRunnerIndex != ::E_ACTIVE_RACE_CAR_INDEX_INVALID &&
        mScoringSystem.GetPlayerDisconnected(leRunnerIndex))
    {
        mOnlineBurningHomeRun.PickNewBurningHomeRunRunner(lpActiveRaceCarOutput,
                                                          lpOutputBuffer->GetGameActionQueue());
    }
}

}  // namespace BrnGameState

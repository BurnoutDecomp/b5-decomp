// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/GameModes/BrnOnlineBurningHomeRunMode_wN3_01.cpp
// ============================================================================
// Partfile of the BrnGameState::OnlineBurningHomeRunMode TU.
// OnlineBurningHomeRunMode::PickNewBurningHomeRunRunner. Callers:
// ModeManager::SetPlayerDisconnected, ModeManager::HandleNewHostEvent and the mode's own
// PreWorldUpdate (not reconstructed).
// ============================================================================

#include "GameSource/GameState/ModeManager/GameModes/BrnOnlineBurningHomeRunMode.h"

#include <cfloat>                                                   // FLT_MAX

#include "GameSource/GameState/ModeManager/BrnModeManager.h"        // ModeManager::GetScoringSystem
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"
#include "GameSource/GameState/BrnGameActions.h"                    // SwitchBurningHomeRunRunnerAction
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/math/vpu/vector3_operation.h"                          // Dot / operator- over Vector3

namespace BrnGameState
{

// The runner is the one blue-team car. Of the connected red-team cars, the one nearest the
// runner's current position (squared distance, strictly nearer wins, first found on a tie)
// becomes the new runner, announced by network player id as action 167. Nothing is posted when
// no red-team car qualifies.
void OnlineBurningHomeRunMode::PickNewBurningHomeRunRunner(
        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveCarInterface,
        GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    CGS_ASSERT(lpActionQueue != NULL, "lpActionQueue");
    CGS_ASSERT(mpModeManager != NULL, "mpModeManager");
    CGS_ASSERT(mpModeManager->GetScoringSystem() != NULL, "mpModeManager->GetScoringSystem()");

    ScoringSystem* lpScoringSystem = mpModeManager->GetScoringSystem();

    // The post-increment carries the in-loop range assert.
    ::EActiveRaceCarIndex leActiveRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_0;
    for (; leActiveRaceCarIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT; leActiveRaceCarIndex++)
    {
        const CarData* lpCarData = lpScoringSystem->GetCarData(leActiveRaceCarIndex);
        if (lpCarData != NULL && lpCarData->GetTeam() == GameStateModuleIO::E_PLAYER_TEAM_BLUE_TEAM)
        {
            break;
        }
    }
    if (leActiveRaceCarIndex >= ::E_ACTIVE_RACE_CAR_INDEX_COUNT)
    {
        leActiveRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_INVALID;
    }
    CGS_ASSERT(leActiveRaceCarIndex != ::E_ACTIVE_RACE_CAR_INDEX_INVALID,
               "leActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID");

    const Vector3 lRunnerPosition =
        lpActiveCarInterface->GetRaceCarState(leActiveRaceCarIndex)->mTransform.Pos();

    CGS_ASSERT(lpScoringSystem->GetNextTeamMember(leActiveRaceCarIndex,
                                                  GameStateModuleIO::E_PLAYER_TEAM_BLUE_TEAM) ==
                   ::E_ACTIVE_RACE_CAR_INDEX_INVALID,
               "mpModeManager->GetScoringSystem()->GetNextTeamMember( leActiveRaceCarIndex, "
               "GsmIO::E_PLAYER_TEAM_BLUE_TEAM ) == E_ACTIVE_RACE_CAR_INDEX_INVALID");

    // The candidate scan reads through the const lookup.
    const ScoringSystem* lpConstScoringSystem = lpScoringSystem;
    ::EActiveRaceCarIndex leNewRunnerIndex = ::E_ACTIVE_RACE_CAR_INDEX_INVALID;
    f32 lfNearestDistanceSquared = FLT_MAX;
    for (::EActiveRaceCarIndex leIndex = ::E_ACTIVE_RACE_CAR_INDEX_0;
         leIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT;
         leIndex++)
    {
        const CarData* lpCarData = lpConstScoringSystem->GetCarData(leIndex);
        if (lpCarData == NULL ||
            lpCarData->GetScoreData()->GetDisconnected() ||
            lpCarData->GetTeam() != GameStateModuleIO::E_PLAYER_TEAM_RED_TEAM)
        {
            continue;
        }

        const Vector3 lv3Delta =
            lpActiveCarInterface->GetRaceCarState(leIndex)->mTransform.Pos() - lRunnerPosition;
        const f32 lfDistanceSquared = rw::math::vpu::Dot(lv3Delta, lv3Delta);
        if (lfDistanceSquared < lfNearestDistanceSquared)
        {
            lfNearestDistanceSquared = lfDistanceSquared;
            leNewRunnerIndex         = leIndex;
        }
    }

    if (leNewRunnerIndex != ::E_ACTIVE_RACE_CAR_INDEX_INVALID)
    {
        const CarData* lpCarData = lpConstScoringSystem->GetCarData(leNewRunnerIndex);
        CGS_ASSERT(lpCarData != NULL, "lpCarData");

        GameStateModuleIO::SwitchBurningHomeRunRunnerAction lAction;
        lAction.mNewRunnerPlayerID = lpCarData->GetNetworkPlayerID();
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAction),
                                GameStateModuleIO::E_ACTION_SWITCH_BURNING_HOME_RUN_RUNNER,
                                static_cast<s32>(sizeof(GameStateModuleIO::SwitchBurningHomeRunRunnerAction)));
    }
}

}  // namespace BrnGameState

#include "GameSource/GameState/ModeManager/GameModes/BrnOnlineBurningHomeRunMode.h"
#define CGS_ARTIST_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/ModeManager/GameModes/BrnOnlineBurningHomeRunMode.cpp"
namespace BrnGameState
{
// X360: BrnGameState::OnlineBurningHomeRunMode::GetName. Trivial virtual override of GameMode::GetName;
// returns the mode's fixed name string. The virtual/trailing-const shape is from the DWARF
// declaration (the Hex-Rays pseudocode renders it as a plain function and drops const).
const char* OnlineBurningHomeRunMode::GetName() const
{
    return "OnlineBurningHomeRun";
}

// X360 vtable slot 24 (vtbl+96): 0x82C296C8 == `li r3,1; blr` at slot 24 of vtable 0x820D0960,
// against the GameMode base's 0x827E2F38 == `li r3,0; blr`. SetupGameMode @0x8234B158 reads it
// twice and HandleLoadingScreenLoaded @0x8234B8A8 once -- an online burning home run DOES put up a loading
// screen, which is what separates it from every offline mode.
bool OnlineBurningHomeRunMode::HasLoadingScreen() const
{
    return true;
}
}

// FOLDED FROM BrnOnlineBurningHomeRunMode_wBT_05.cpp
// BrnGameState::OnlineBurningHomeRunMode::SwitchBurningHomeRunRunner -- its one caller is
// ModeManager::HandleBurningHomeRunRunnerSwitch (ProcessGameEvents case 155). The old runner goes to
// the red team with full infinite boost, the new runner to the blue team with none; the HUD gets a
// team-change message for each, a local new runner gets a second of invulnerability, and the
// runner counters restart from the new runner's crash count. A switch arriving less than a second
// after the previous one is ignored.

#include "GameSource/GameState/ModeManager/GameModes/BrnOnlineBurningHomeRunMode.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
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
               CGS_ASSERT_MESSAGE_AT("leOldRunnerRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0", CGS_ARTIST_FILE, 242));
    CGS_ASSERT(leOldRunnerRaceCarIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT,
               CGS_ASSERT_MESSAGE_AT("leOldRunnerRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT", CGS_ARTIST_FILE, 243));
    CGS_ASSERT(leNewRunnerRaceCarIndex >= ::E_ACTIVE_RACE_CAR_INDEX_0,
               CGS_ASSERT_MESSAGE_AT("leNewRunnerRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0", CGS_ARTIST_FILE, 245));
    CGS_ASSERT(leNewRunnerRaceCarIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT,
               CGS_ASSERT_MESSAGE_AT("leNewRunnerRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT", CGS_ARTIST_FILE, 246));

    CGS_ASSERT(mpModeManager != NULL, CGS_ASSERT_MESSAGE_AT("mpModeManager", CGS_ARTIST_FILE, 248));
    CGS_ASSERT(mpModeManager->GetScoringSystem() != NULL, CGS_ASSERT_MESSAGE_AT("mpModeManager->GetScoringSystem()", CGS_ARTIST_FILE, 249));
    CGS_ASSERT(mpModeManager->GetHUDMessageLogic() != NULL, CGS_ASSERT_MESSAGE_AT("mpModeManager->GetHUDMessageLogic()", CGS_ARTIST_FILE, 250));

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
    CGS_ASSERT(lpCarData, CGS_ASSERT_MESSAGE_AT("lpCarData", CGS_ARTIST_FILE, 312));
    CGS_ASSERT(lpCarData->GetScoreData(), CGS_ASSERT_MESSAGE_AT("lpCarData->GetScoreData()", CGS_ARTIST_FILE, 313));

    miNumCrashesWhenPlayerBecameRunner = lpCarData->GetScoreData()->GetMarkedManTakedownsFor();
    miNumCrashesAsRunner               = 0;
    mfTimeSinceRunnerSwitch            = 0.0f;
}

}

// FOLDED FROM BrnOnlineBurningHomeRunMode_wN3_01.cpp
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
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
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
    CGS_ASSERT(lpActionQueue != NULL, CGS_ASSERT_MESSAGE_AT("lpActionQueue", CGS_ARTIST_FILE, 344));
    CGS_ASSERT(mpModeManager != NULL, CGS_ASSERT_MESSAGE_AT("mpModeManager", CGS_ARTIST_FILE, 345));
    CGS_ASSERT(mpModeManager->GetScoringSystem() != NULL, CGS_ASSERT_MESSAGE_AT("mpModeManager->GetScoringSystem()", CGS_ARTIST_FILE, 249));

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
               CGS_ASSERT_MESSAGE_AT("leActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID", CGS_ARTIST_FILE, 350));

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
        CGS_ASSERT(lpCarData != NULL, CGS_ASSERT_MESSAGE_AT("lpCarData", CGS_ARTIST_FILE, 400));

        GameStateModuleIO::SwitchBurningHomeRunRunnerAction lAction;
        lAction.mNewRunnerPlayerID = lpCarData->GetNetworkPlayerID();
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAction),
                                GameStateModuleIO::E_ACTION_SWITCH_BURNING_HOME_RUN_RUNNER,
                                static_cast<s32>(sizeof(GameStateModuleIO::SwitchBurningHomeRunRunnerAction)));
    }
}

}  // namespace BrnGameState

#undef CGS_ARTIST_FILE

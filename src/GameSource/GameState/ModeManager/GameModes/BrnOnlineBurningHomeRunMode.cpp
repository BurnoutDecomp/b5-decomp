#include "GameSource/GameState/ModeManager/GameModes/BrnOnlineBurningHomeRunMode.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                            // CGS_ASSERT
#include "GameSource/GameState/BrnGameStateModuleIO.h"                        // PreWorldInputBuffer / OutputBuffer
#include "GameSource/GameState/ModeManager/BrnModeManager.h"                  // SetOnlineLandmarks / SetupOnlineStartingGrid / SetOnlineRaceCars
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"     // GameModeParams
#include "GameSource/GameState/ModeManager/Hud/BrnHUDMessageLogic.h"          // HUDMessageLogic::SetBurningHomeRunRunnerCrashed
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"        // ScoringSystem / CarData
#include "GameSource/GameState/NetworkRoundManager/BrnNetworkRoundManager.h"  // the start events + the round index
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"

namespace BrnGameState
{
namespace
{
    // flt_82001DA0 == 0.5f.
    const f32 KF_ONLINE_BURNING_HOME_RUN_TRAFFIC_DENSITY = 0.5f;

    // `cmpwi r11, 6` @0x8234D0CC and @0x8234D160. With this value in miMaxCrashesBeforeChange the
    // runner role never changes hands and the HUD gets no countdown. NAME IS INFERRED from that
    // behaviour; the console only has the literal.
    const s32 KI_RUNNER_CRASHES_UNLIMITED = 6;
}

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

// X360: BrnGameState::OnlineBurningHomeRunMode::Start (0x82339968). Neither lpStartGameModeParams nor
// lpScoringSystem is read. Unlike the race and road rage this one sets only the clear-traffic flag
// and leaves the two route-finding styles at Construct's value. The inlined block under the
// first-round test is Random::Construct + SetSeed, as in OnlineRaceMode::Start.
void OnlineBurningHomeRunMode::Start(const StartGameModeParams* lpStartGameModeParams,
                                     GameModeParams*            lpGameModeParams,
                                     ScoringSystem*             lpScoringSystem)
{
    (void)lpStartGameModeParams;
    (void)lpScoringSystem;

    const NetworkRoundManager* lpNetworkRoundManager = GetModeManager()->GetNetworkRoundManager();
    const GameStateModuleIO::StartNetworkGameEvent*  lpStartNetworkGameEvent  = lpNetworkRoundManager->GetNetworkGameEvent();
    const GameStateModuleIO::StartNetworkRoundEvent* lpStartNetworkRoundEvent = lpNetworkRoundManager->GetNetworkRoundEvent();

    const s32 liCurrentRound = lpNetworkRoundManager->GetCurrentRound();

    mbIsOnline = true;

    lpGameModeParams->Construct(GameStateModuleIO::E_MODE_ONLINE_BURNING_HOME_RUN);

    lpGameModeParams->SetFlag(GameModeParams::KU_FLAG_CLEAR_NEARBY_TRAFFIC);
    lpGameModeParams->SetTrafficDensityScale(KF_ONLINE_BURNING_HOME_RUN_TRAFFIC_DENSITY);
    lpGameModeParams->SetStartMechanism(E_GAMEMODESTARTMECHANISM_SPIN_WHEELS_AT_LIGHTS);
    lpGameModeParams->SetProgressionRankAsRatio(0.0f);
    lpGameModeParams->mbIsOnline = true;
    lpGameModeParams->SetTrafficLightTriggerId(lpStartNetworkRoundEvent->mLightTriggerID);
    lpGameModeParams->meOnlineBoostStrategy = static_cast<EBoostType_Stub>(lpStartNetworkGameEvent->meBoostType);
    lpGameModeParams->SetOnlineTimeLimit(lpStartNetworkGameEvent->mfTimeLimit);

    CGS_ASSERT(lpStartNetworkGameEvent->mfTimeLimit > 0.0f, "lpStartNetworkGameEvent->GetTimeLimit() > 0.0f");

    if (!lpStartNetworkGameEvent->mbIsTrafficOn)
    {
        lpGameModeParams->SetTrafficDensityScale(0.0f);
    }
    if (!lpStartNetworkGameEvent->mbIsTrafficCheckingOn)
    {
        lpGameModeParams->SetFlag(GameModeParams::KU_FLAG_TRAFFIC_CHECKING_NOT_ALLOWED);
    }

    if (liCurrentRound == 0)
    {
        mRandom.Construct();
        mRandom.SetSeed(lpStartNetworkGameEvent->muRandomSeedForGame);
    }

    PickRandomRunnerAndSetTeams(lpStartNetworkGameEvent, lpGameModeParams);

    lpGameModeParams->mbInfiniteBoost = lpStartNetworkGameEvent->mbRedTeamHaveInfiniteBoost;

    GetModeManager()->SetOnlineLandmarks(lpGameModeParams, lpStartNetworkRoundEvent, liCurrentRound);
    GetModeManager()->SetupOnlineStartingGrid(lpGameModeParams, lpStartNetworkGameEvent->miNumRaceCars, &mRandom, false);
    GetModeManager()->SetOnlineRaceCars(lpGameModeParams, lpStartNetworkGameEvent);

    miNumCrashesWhenPlayerBecameRunner = 0;
    miNumCrashesAsRunner               = 0;
    mfTimeSinceRunnerSwitch            = 0.0f;
    miMaxCrashesBeforeChange           = lpStartNetworkGameEvent->miNumRunnerCrashes;
}

// X360: BrnGameState::OnlineBurningHomeRunMode::PickRandomRunnerAndSetTeams (0x82316158). The draw is
// CgsRandom.h's RandomInt inlined (its "liMax >= liMin" / "luMod > 0" asserts, and the seed's old high
// word modulo the car count, are at 0x823161D8..0x82316260).
void OnlineBurningHomeRunMode::PickRandomRunnerAndSetTeams(const GameStateModuleIO::StartNetworkGameEvent* lpStartNetworkGameEvent,
                                                           GameModeParams* lpGameModeParams)
{
    const s32 liNumberOfRaceCars = lpStartNetworkGameEvent->miNumRaceCars;

    s32 liCar = 0;
    for (; liCar < liNumberOfRaceCars; ++liCar)
    {
        lpGameModeParams->maePlayerTeam[liCar] = static_cast<EPlayerTeam_Stub>(GameStateModuleIO::E_PLAYER_TEAM_RED_TEAM);
    }
    for (; liCar < GameStateModuleIO::KI_MAX_RACE_CARS; ++liCar)
    {
        lpGameModeParams->maePlayerTeam[liCar] = static_cast<EPlayerTeam_Stub>(GameStateModuleIO::E_PLAYER_TEAM_NONE);
    }

    const s32 liRunnerIndex = mRandom.RandomInt(0, liNumberOfRaceCars - 1);
    CGS_ASSERT(liRunnerIndex < liNumberOfRaceCars, "liRunnerIndex < liNumberOfRaceCars");

    lpGameModeParams->maePlayerTeam[liRunnerIndex] = static_cast<EPlayerTeam_Stub>(GameStateModuleIO::E_PLAYER_TEAM_BLUE_TEAM);
}

// X360: BrnGameState::OnlineBurningHomeRunMode::PreWorldUpdate (0x8234CFD0).
//
// The clock adds `lfs f13, 4(TimerStatusInterface)`: the GAME timer's BASE step (the sim status starts
// at +24, the multiplier is +8), so it is the unscaled game step. SwitchBurningHomeRunRunner tests
// this clock against its one-second gate.
void OnlineBurningHomeRunMode::PreWorldUpdate(GameStateModuleIO::OutputBuffer* lpOutput,
                                              const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                                              const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface* lpGlobalRaceCars,
                                              const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars,
                                              bool lbPaused,
                                              const ScoringSystem* lpScoringSystem)
{
    GameMode::PreWorldUpdate(lpOutput, lpInput, lpGlobalRaceCars, lpActiveRaceCars, lbPaused, lpScoringSystem);

    CGS_ASSERT(lpScoringSystem, "lpScoringSystem");

    mfTimeSinceRunnerSwitch += lpInput->GetTimerStatusInterface()->GetGameTimerStatus()->GetBaseTimeStep();

    const ::EActiveRaceCarIndex leRunnerIndex =
        lpScoringSystem->GetNextTeamMember(::E_ACTIVE_RACE_CAR_INDEX_INVALID, GameStateModuleIO::E_PLAYER_TEAM_BLUE_TEAM);
    if (leRunnerIndex == ::E_ACTIVE_RACE_CAR_INDEX_INVALID)
    {
        return;
    }

    const CarData* lpCarData = lpScoringSystem->GetCarData(leRunnerIndex);
    CGS_ASSERT(lpCarData, "lpCarData");
    CGS_ASSERT(lpCarData->GetScoreData(), "lpCarData->GetScoreData()");

    const s32 liCrashes = lpCarData->GetScoreData()->GetMarkedManTakedownsFor() - miNumCrashesWhenPlayerBecameRunner;
    if (liCrashes == miNumCrashesAsRunner)
    {
        return;
    }

    if (liCrashes >= miMaxCrashesBeforeChange && miMaxCrashesBeforeChange != KI_RUNNER_CRASHES_UNLIMITED)
    {
        PickNewBurningHomeRunRunner(lpActiveRaceCars, lpOutput->GetGameActionQueue());
        return;
    }

    // Held back until the runner's crash has finished, so the HUD counts it once.
    if (!lpActiveRaceCars->GetRaceCarState(leRunnerIndex)->mbCrashing)
    {
        CGS_ASSERT(mpModeManager, "mpModeManager");
        CGS_ASSERT(mpModeManager->GetHUDMessageLogic(), "mpModeManager->GetHUDMessageLogic()");

        if (miMaxCrashesBeforeChange != KI_RUNNER_CRASHES_UNLIMITED)
        {
            mpModeManager->GetHUDMessageLogic()->SetBurningHomeRunRunnerCrashed(leRunnerIndex,
                                                                                miMaxCrashesBeforeChange - liCrashes);
        }

        miNumCrashesAsRunner = liCrashes;
    }
}
}

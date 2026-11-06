#include "GameSource/GameState/ModeManager/GameModes/BrnOnlineRoadRageMode.h"

#include "GameSource/GameState/ModeManager/BrnModeManager.h"                  // SetOnlineLandmarks / SetupOnlineStartingGrid / SetOnlineRaceCars
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"     // GameModeParams
#include "GameSource/GameState/NetworkRoundManager/BrnNetworkRoundManager.h"  // the start events + the round index

namespace BrnGameState
{
namespace
{
    // flt_82001DA0 == 0.5f.
    const f32 KF_ONLINE_ROAD_RAGE_TRAFFIC_DENSITY = 0.5f;

    // `stw r28(1),0x840` / `stw r28(1),0x844` @0x823394EC. The console value only; the member is
    // still the ERouteFindingStyle_Stub placeholder, so the enumerator has no name here.
    const s32 KI_ONLINE_ROAD_RAGE_ROUTE_FINDING_STYLE = 1;
}

// X360: BrnGameState::OnlineRoadRageMode::GetName. Trivial virtual override of GameMode::GetName;
// returns the mode's fixed name string. The virtual/trailing-const shape is from the DWARF
// declaration (the Hex-Rays pseudocode renders it as a plain function and drops const).
const char* OnlineRoadRageMode::GetName() const
{
    return "OnlineRoadRage";
}

// X360 vtable slot 24 (vtbl+96): 0x82C296C8 == `li r3,1; blr` at slot 24 of vtable 0x820D0860,
// against the GameMode base's 0x827E2F38 == `li r3,0; blr`. SetupGameMode @0x8234B158 reads it
// twice and HandleLoadingScreenLoaded @0x8234B8A8 once -- an online road rage DOES put up a loading
// screen, which is what separates it from every offline mode.
bool OnlineRoadRageMode::HasLoadingScreen() const
{
    return true;
}

// X360: BrnGameState::OnlineRoadRageMode::Start (0x823394A0). Neither lpStartGameModeParams nor
// lpScoringSystem is read. The grid stays on the event's own light trigger and is always shuffled
// (`li r7,0`), per team block; the inlined block under the first-round test is Random::Construct +
// SetSeed, as in OnlineRaceMode::Start.
void OnlineRoadRageMode::Start(const StartGameModeParams* lpStartGameModeParams,
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

    lpGameModeParams->Construct(GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE);

    lpGameModeParams->SetDefaultPlayerRouteFindingStyle(static_cast<ERouteFindingStyle_Stub>(KI_ONLINE_ROAD_RAGE_ROUTE_FINDING_STYLE));
    lpGameModeParams->SetDefaultAIRouteFindingStyle(static_cast<ERouteFindingStyle_Stub>(KI_ONLINE_ROAD_RAGE_ROUTE_FINDING_STYLE));
    lpGameModeParams->SetFlag(GameModeParams::KU_FLAG_USES_NAVIGATION |
                              GameModeParams::KU_FLAG_HAS_ROUTE |
                              GameModeParams::KU_FLAG_CLEAR_NEARBY_TRAFFIC);   // `oris 0x2000 / ori 0x2800`
    lpGameModeParams->SetTrafficDensityScale(KF_ONLINE_ROAD_RAGE_TRAFFIC_DENSITY);
    lpGameModeParams->SetProgressionRankAsRatio(0.0f);
    lpGameModeParams->mbIsOnline = true;
    lpGameModeParams->SetStartMechanism(E_GAMEMODESTARTMECHANISM_SPIN_WHEELS_AT_LIGHTS);
    lpGameModeParams->SetTrafficLightTriggerId(lpStartNetworkRoundEvent->mLightTriggerID);
    lpGameModeParams->meOnlineBoostStrategy = static_cast<EBoostType_Stub>(lpStartNetworkGameEvent->meBoostType);

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

    GetModeManager()->SetOnlineRoadRageTeamsGameParams(lpGameModeParams, lpStartNetworkGameEvent, liCurrentRound);

    lpGameModeParams->mbInfiniteBoost = lpStartNetworkGameEvent->mbRedTeamHaveInfiniteBoost;

    GetModeManager()->SetOnlineLandmarks(lpGameModeParams, lpStartNetworkRoundEvent, liCurrentRound);
    GetModeManager()->SetupOnlineStartingGrid(lpGameModeParams, lpStartNetworkGameEvent->miNumRaceCars, &mRandom, false);
    GetModeManager()->SetOnlineRaceCars(lpGameModeParams, lpStartNetworkGameEvent);
}
}

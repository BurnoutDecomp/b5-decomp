#include "GameSource/GameState/ModeManager/GameModes/BrnOnlineFreeBurnLobbyMode.h"

#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"      // complete GameModeParams/StartGameModeParams/ScoringSystem
#include "GameSource/GameState/ModeManager/BrnModeManager.h"                   // GetNetworkRoundManager / SetOnlineRaceCars
#include "GameSource/GameState/NetworkRoundManager/BrnNetworkRoundManager.h"   // GetNetworkGameEvent + StartNetworkGameEvent

namespace BrnGameState
{
// X360: BrnGameState::OnlineFreeBurnLobbyMode::GetName. Trivial virtual override of GameMode::GetName;
// returns the mode's fixed name string. The virtual/trailing-const shape is from the DWARF
// declaration (the Hex-Rays pseudocode renders it as a plain function and drops const).
const char* OnlineFreeBurnLobbyMode::GetName() const
{
    return "OnlineFreeBurnLobby";
}

// X360: BrnGameState::OnlineFreeBurnLobbyMode::Start (0x82322338).
//
// Online free-burn-lobby set-up. Pulls the cached StartNetworkGameEvent off the ModeManager's
// NetworkRoundManager, builds a low-stakes free-roam GameModeParams (light traffic, car-select
// allowed, no rivals/rank), copies the per-player network ids, and places the online cars. Raw-offset
// pokes on the OLD GameModeParams layout are de-inlined to the committed named members; the
// first/third params are dropped by Hex-Rays.
void OnlineFreeBurnLobbyMode::Start(const StartGameModeParams* /*lpStartGameModeParams*/,
                                   GameModeParams*             lpGameModeParams,
                                   ScoringSystem*              /*lpScoringSystem*/)
{
    // v5: the cached StartNetworkGameEvent (X360 *(*(this+160)+28004)); see OnlineShowtimeMode::Start.
    const GameStateModuleIO::StartNetworkGameEvent* lpStartNetworkGameEvent =
        GetModeManager()->GetNetworkRoundManager()->GetNetworkGameEvent();

    // `li r28,1` @0x82322348 / `stb r28, 0xAC(r29)` @0x82322360 -> *(this+172) = 1.
    // [!] NAME CORRECTED 2026-08-26 (wave-B fix round): +0xAC is mbIsOnline, not `mbConstructed`.
    // OfflineGameMode::Construct @0x8232FE78 stores 0 into the same byte and OnlineGameMode::
    // Construct @0x8232FEB4 stores 1; no OFFLINE mode's Start touches +0xAC, while all three
    // ONLINE Start bodies re-assert it. See the +160..+179 table in BrnGameMode.h.
    mbIsOnline = true;

    lpGameModeParams->Construct(GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY);

    // The stores, in the console's order after Construct: +0x94 mbIsOnline = 1, muFlags |= 0x400
    // (CAR_SELECT_ALLOWED), +0x30 traffic density 0.5 (flt 0x3F000000), +0x13C mbInfiniteBoost = 0,
    // +0x140 meOnlineBoostStrategy = 0, +0x04 rank ratio 0.0; then the density drops to 0.0 when the
    // start event says traffic is off (event +0xF5 mbIsTrafficOn).
    lpGameModeParams->mbIsOnline = true;
    lpGameModeParams->SetFlag(GameModeParams::KU_FLAG_CAR_SELECT_ALLOWED);
    lpGameModeParams->SetTrafficDensityScale(0.5f);
    lpGameModeParams->mbInfiniteBoost       = false;
    lpGameModeParams->meOnlineBoostStrategy = static_cast<EBoostType_Stub>(0);
    lpGameModeParams->SetProgressionRankAsRatio(0.0f);
    if (!lpStartNetworkGameEvent->mbIsTrafficOn)
    {
        lpGameModeParams->SetTrafficDensityScale(0.0f);
    }

    // The eight teams: event +0x78..+0x94 (maePlayerTeam) -> params +0x118..+0x134 (maePlayerTeam).
    // SetOnlineRaceCars below copies every other per-car run but not the teams.
    for (s32 liPlayer = 0; liPlayer < GameStateModuleIO::KI_MAX_RACE_CARS; ++liPlayer)
    {
        lpGameModeParams->maePlayerTeam[liPlayer] =
            static_cast<EPlayerTeam_Stub>(static_cast<s32>(lpStartNetworkGameEvent->maePlayerTeam[liPlayer]));
    }

    GetModeManager()->SetOnlineRaceCars(lpGameModeParams, lpStartNetworkGameEvent);
}

// X360 vtable slot 23 (vtbl+92), folded leaf 0x827E2F38 == `li r3,0; blr` at slot 23 of vtable
// 0x820D0A68; the GameMode base is 0x82C296C8 == `li r3,1`.
bool OnlineFreeBurnLobbyMode::RequiresStreaming() const
{
    return false;
}
}

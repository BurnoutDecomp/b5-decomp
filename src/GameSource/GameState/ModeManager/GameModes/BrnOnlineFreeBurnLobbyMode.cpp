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

    // muFlags |= 0x400 (CAR_SELECT_ALLOWED) -- X360 v6 | 0x400.
    lpGameModeParams->SetFlag(GameModeParams::KU_FLAG_CAR_SELECT_ALLOWED);

    // Light lobby traffic by default; forced off if the start event says traffic is off
    // (StartNetworkGameEvent::mbIsTrafficOn == byte at v5+245). X360: *(a3+48)=0.5; if(!*(v5+245)) *(a3+48)=0.0.
    f32 lfTrafficDensityScale = 0.5f;
    if (!lpStartNetworkGameEvent->mbIsTrafficOn)
    {
        lfTrafficDensityScale = 0.0f;
    }
    lpGameModeParams->SetTrafficDensityScale(lfTrafficDensityScale);

    lpGameModeParams->mbIsOnline = true;                  // X360 *(a3+148)=1
    lpGameModeParams->SetProgressionRankAsRatio(0.0f);    // X360 *(a3+4)=0.0

    // X360 *(a3+316)=0 / *(a3+320)=0 -- two int tuning fields the lobby zeroes. In the committed
    // (offset-not-faithful) layout these are best-effort-mapped to the pursuit/road-rage tuning ints
    // (LOW confidence on the exact named identity; both are s32 so the writes compile and the
    // zeroing intent is preserved).
    lpGameModeParams->miRoadRageThreshold       = 0;      // X360 *(a3+316) (ambiguous offset)
    lpGameModeParams->miPursuitRivalTotalDamage = 0;      // X360 *(a3+320) (ambiguous offset)

    // Copy the eight per-player network ids (v5 bytes 120..148 == StartNetworkGameEvent::
    // maNetworkPlayerID[8]) into GameModeParams::maNetworkPlayerID[8] (a3+280..308). After the home
    // grow both sides are BrnNetwork::NetworkPlayerID (s32) -- type-matched.
    for (s32 liPlayer = 0; liPlayer < GameStateModuleIO::KI_MAX_RACE_CARS; ++liPlayer)
    {
        lpGameModeParams->maNetworkPlayerID[liPlayer] =
            lpStartNetworkGameEvent->maNetworkPlayerID[liPlayer];
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

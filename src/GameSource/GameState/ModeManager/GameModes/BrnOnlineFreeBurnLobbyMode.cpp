#include "GameSource/GameState/ModeManager/GameModes/BrnOnlineFreeBurnLobbyMode.h"

#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"      // complete GameModeParams/StartGameModeParams/ScoringSystem
#include "GameSource/GameState/ModeManager/BrnModeManager.h"                   // GetNetworkRoundManager / SetOnlineRaceCars
#include "GameSource/GameState/NetworkRoundManager/BrnNetworkRoundManager.h"   // GetNetworkGameEvent + StartNetworkGameEvent
#include "GameShared/GameClasses/Core/CgsAssert.h"                                  // SendEvent default arm

namespace BrnGameState
{
// EGameModeState ids (named file-local, as BrnOnlineFreeBurnMode.cpp does).
enum
{
    KI_GMS_COUNTDOWN      = 0,
    KI_GMS_INTRO          = 1,
    KI_GMS_IN_PROGRESS    = 2,
    KI_GMS_QUIT           = 5,
    KI_GMS_ONLINE_LOADING = 6,
    KI_GMS_ONLINE_SPLASH  = 7
};

// Slot 0. The base GameMode::Construct is called directly and the online byte is stored after it
// (the OnlineGameMode::Construct pair, inlined), then the embedded manager is constructed.
void OnlineFreeBurnLobbyMode::Construct(ModeManager* lpModeManager)
{
    GameMode::Construct(lpModeManager);
    mbIsOnline = true;
    mBurnoutSkillzManager.Construct(lpModeManager);
}

// Slot 2. All six arguments go to the base first; the manager then gets the input buffer, the
// active race-car interface and the output buffer, with lbExitingFreeburnLobby = false.
void OnlineFreeBurnLobbyMode::PreWorldUpdate(GameStateModuleIO::OutputBuffer* lpOutput,
                                             const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                                             const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface* lpGlobalRaceCars,
                                             const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars,
                                             bool lbPaused,
                                             const ScoringSystem* lpScoringSystem)
{
    GameMode::PreWorldUpdate(lpOutput, lpInput, lpGlobalRaceCars, lpActiveRaceCars, lbPaused, lpScoringSystem);
    mBurnoutSkillzManager.PreWorldUpdate(lpInput, lpActiveRaceCars, lpOutput, false);
}

// The online-showtime tick of the lobby's manager (ModeManager::UpdateCurrentMode, mode 16): the
// same manager call as PreWorldUpdate's, with lbExitingFreeburnLobby = true and no base update.
void OnlineFreeBurnLobbyMode::BurnoutSkillzOnlyPreWorldUpdate(
    GameStateModuleIO::OutputBuffer* lpOutput,
    const GameStateModuleIO::PreWorldInputBuffer* lpInput,
    const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface* /*lpGlobalRaceCars*/,
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars,
    bool /*lbPaused*/,
    const ScoringSystem* /*lpScoringSystem*/)
{
    mBurnoutSkillzManager.PreWorldUpdate(lpInput, lpActiveRaceCars, lpOutput, true);
}

// Slot 3. A two-instruction tail call.
void OnlineFreeBurnLobbyMode::PostWorldUpdate(const GameStateModuleIO::PostWorldInputBuffer* lpInput)
{
    mBurnoutSkillzManager.PostWorldUpdate(lpInput);
}

// Slot 19.
void OnlineFreeBurnLobbyMode::PlayerHasSpawned(::EActiveRaceCarIndex leActiveRaceCarIndex)
{
    mBurnoutSkillzManager.SendUpdatePlayerSkillsEvent(leActiveRaceCarIndex, false);
}

// Slot 12. ABORT -> QUIT and RESTART -> ONLINE_LOADING from any state; otherwise NEXT walks
// ONLINE_LOADING -> INTRO -> ONLINE_SPLASH -> COUNTDOWN -> IN_PROGRESS -> QUIT. QUIT ignores every
// event, and OUTRO / RESULTS (never entered by the lobby) fire the assert, whose text is the
// Showtime one in the console's rodata too.
void OnlineFreeBurnLobbyMode::SendEvent(EGameModeEvent leEvent)
{
    if (leEvent == E_GME_ABORT)
    {
        SetCurrentState(KI_GMS_QUIT);
        return;
    }
    if (leEvent == E_GME_RESTART)
    {
        SetCurrentState(KI_GMS_ONLINE_LOADING);
        return;
    }

    switch (meCurrentState)
    {
        case KI_GMS_COUNTDOWN:
            if (leEvent == E_GME_NEXT)
            {
                SetCurrentState(KI_GMS_IN_PROGRESS);
            }
            break;
        case KI_GMS_INTRO:
            if (leEvent == E_GME_NEXT)
            {
                SetCurrentState(KI_GMS_ONLINE_SPLASH);
            }
            break;
        case KI_GMS_IN_PROGRESS:
            if (leEvent == E_GME_NEXT)
            {
                SetCurrentState(KI_GMS_QUIT);
            }
            break;
        case KI_GMS_QUIT:
            break;
        case KI_GMS_ONLINE_LOADING:
            if (leEvent == E_GME_NEXT)
            {
                SetCurrentState(KI_GMS_INTRO);
            }
            break;
        case KI_GMS_ONLINE_SPLASH:
            if (leEvent == E_GME_NEXT)
            {
                SetCurrentState(KI_GMS_COUNTDOWN);
            }
            break;
        default:
            CGS_ASSERT(false, "Should not be in this state in Showtime mode!");
            break;
    }
}

// Slot 20. The score record travels by value; the challenge and active-car indices are the two
// stack-passed arguments, re-stacked for the manager's call.
void OnlineFreeBurnLobbyMode::ProcessNewRoadScore(GameStateModuleIO::OutputBuffer* lpOutput,
                                                  BrnStreetData::ChallengePlayerScoreEntry lScoreEntry,
                                                  BrnStreetData::ScoreType leScoreType,
                                                  BrnStreetData::ChallengeIndex lChallengeIndex,
                                                  ::EActiveRaceCarIndex leActiveRaceCarIndex)
{
    mBurnoutSkillzManager.ProcessNewRoadScore(lpOutput, lScoreEntry, leScoreType, lChallengeIndex,
                                              leActiveRaceCarIndex);
}

// Out of line: the manager's BufferNewRoadScore inlined here.
void OnlineFreeBurnLobbyMode::BufferNewRoadScore(BrnStreetData::ChallengePlayerScoreEntry lChallengeScore,
                                                 BrnStreetData::ScoreType leScoreType,
                                                 BrnStreetData::ChallengeIndex lChallengeIndex)
{
    mBurnoutSkillzManager.BufferNewRoadScore(lChallengeScore, leScoreType, lChallengeIndex);
}

// Slot 21. A two-instruction tail call.
void OnlineFreeBurnLobbyMode::OnEnterRoad(BrnStreetData::RoadIndex lRoadIndex)
{
    mBurnoutSkillzManager.OnEnterRoad(lRoadIndex);
}

// Inlined into ModeManager::SendModeStopMessages.
void OnlineFreeBurnLobbyMode::OnModeEnd(bool lbExitingFreeburnLobby)
{
    mBurnoutSkillzManager.OnModeEnd(lbExitingFreeburnLobby);
}

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

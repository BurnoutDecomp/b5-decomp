#pragma once

#include "types.hpp"

#include "GameSource/GameState/BrnGameStateSharedIO.h" // GameStateModuleIO::EGameModeType
#include "GameSource/GameState/BrnGameEvents.h"        // GameStateModuleIO::StartNetworkGameEvent (SetOnlineRaceCars signature)

namespace BrnProgression
{
// Forward decl: ModeManager::GetProgressionManager hands one out by pointer only (the
// offline modes walk it to resolve landmark AI-section indices). Real owning header is
// GameSource/GameState/Progression/BrnProgressionManager.h, reconstructed by its own TU.
class ProgressionManager;
}

namespace BrnGameState
{
// Forward decls: the accessors below hand out a GameMode / take a GameModeParams only by
// pointer, and each of those types' own headers forward-declares ModeManager, so pointer-only
// forward decls here keep the headers free of an include cycle.
class GameMode;
class GameModeParams;
class NetworkRoundManager;

// MERGED OWNING HEADER for the ModeManager of the game-mode hierarchy (consolidated from the
// CountdownState / IntroState / RaceMode worker contributions and the existing committed slice).
// This is the real home for BrnGameState::ModeManager; BrnGameMode.h forward-declares it and the
// game-mode / state .cpp files #include this header when they actually call into it.
//
// MINIMAL / BOUNDED reconstruction: only the methods the worked slices need are declared, gated
// on the X360 ledger / DWARF. The full ModeManager is large (~190 methods + dozens of by-value
// contained mode/state objects in the DecFIGS DWARF, BrnModeManager.h) and is reconstructed by
// its own TU; this header is then extended toward that shape. No member layout is reconstructed
// here because every caller only ever holds a ModeManager* and calls methods on it -- a
// pointer-only use needs no layout.
class ModeManager
{
public:
    // X360-attested (identity.json, 0x82329B68); PS3-only-absent from the DecFIGS DWARF (build
    // drift), so its shape is recovered from the X360 pseudocode: a non-static member whose body
    // returns the pointer from ScoringSystem::UpdateCumulativeResults; the only caller (SendEvent)
    // discards it, so it is declared `void` here to avoid forking the as-yet-unreconstructed
    // return type. Called by OnlineGameMode::SendEvent.
    void TellGuiToShowOnlineFinalStandings();

    // Pre-race countdown duration for the current mode. X360-attested (identity.json,
    // 0x82327D10), called by CountdownState::OnEnter. Body reads the current mode type and
    // indexes a per-mode time table (plus team-mode bonuses); reconstructed by the ModeManager
    // TU. Returns f32 (the body computes a double, every caller stores it single-precision).
    f32  GetCountdownTimeForMode() const;

    // --- Queries used by the nested GameMode states (IntroState) -------------------------
    // DWARF-attested public accessors, inlined in the X360 build (IntroState's pseudocode renders
    // them as inline field reads: meCurrentGameModeType, mpCurrentGameMode, mbModeDataIsLoading).
    // Declared (not defined) here; the bodies + real members land with the ModeManager TU.
    GameStateModuleIO::EGameModeType GetCurrentGameModeType() const;
    const GameMode* GetCurrentGameMode() const;
    bool            IsWaitingForModeDataToLoad() const;

    // True when the current online mode (free-burn lobby or showtime) starts without a timed
    // intro -- the composite the X360 build inlines everywhere as
    //   (IsOnlineFreeBurnLobby() || IsShowtimeGameMode()) && <mbSplashFlag>
    // (ModeManager field at X360 +0x9508). IntroState::OnEnter uses it to collapse the intro
    // countdown to ~0. The underlying flag's exact member name is unconfirmed in this bounded
    // view; rename when ModeManager is fully reconstructed.
    bool IsOnlineModeWithInstantIntro() const;

    // X360: BrnGameState::ModeManager::SetStartingGrid (0x82328608), called by the offline modes'
    // Start() (e.g. RaceMode::Start) to place the cars on the grid. Declaration shape taken from
    // the DecFIGS DWARF (BrnModeManager.h:486): void SetStartingGrid(GameModeParams*, int32_t,
    // bool) const. The full body/layout is reconstructed by the ModeManager TU.
    void SetStartingGrid(GameModeParams* lpGameModeParams, s32 liCarCount, bool lbPushForwards) const;

    // The owning network round manager (DWARF BrnModeManager.h:482). The online modes' Start()
    // reach the cached StartNetworkGameEvent through it (NetworkRoundManager::GetNetworkGameEvent).
    // Returns by pointer; body + member land with the ModeManager TU.
    const NetworkRoundManager* GetNetworkRoundManager() const;

    // DWARF BrnModeManager.h:483. Places the online race cars on the grid from the start event.
    // Body + layout land with the ModeManager TU.
    void SetOnlineRaceCars(GameModeParams* lpGameModeParams,
                           const GameStateModuleIO::StartNetworkGameEvent* lpStartNetworkGameEvent) const;

    // The owning ProgressionManager. Inlined in the X360 build (OfflineGameMode::
    // SelectRandomDestinations reaches it via the magic-multiply ModeManager indirection); the
    // standalone accessor is declared here for the offline-mode callers. Body + real member land
    // with the ModeManager TU. NOTE: depends on BrnProgression::ProgressionManager whose owning
    // header is not yet reconstructed (forward-declared above, used by pointer only).
    BrnProgression::ProgressionManager* GetProgressionManager() const;

    // Debug-tunable mode flags driven by the mode-manager debug menu (ModeManagerDebugComponent).
    // Declared as named members here (the real home) so the debug component accesses them by name
    // rather than by raw offset; the full ModeManager layout (the X360 places these deep in the
    // ~38KB object) is reconstructed by the ModeManager TU and these settle into their real slots.
    bool mbEndlessStuntRun;
    bool mbWinIfSecond;
    bool mbFinishCurrentEvent;
    s32  miFinishPosition;
};
}

#include "GameSource/GameState/ModeManager/GameModes/BrnOnlineShowtimeMode.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"      // complete GameModeParams/StartGameModeParams/ScoringSystem
#include "GameSource/GameState/ModeManager/BrnModeManager.h"                   // GetNetworkRoundManager / SetOnlineRaceCars
#include "GameSource/GameState/NetworkRoundManager/BrnNetworkRoundManager.h"   // GetNetworkGameEvent + StartNetworkGameEvent

namespace BrnGameState
{
const f32 OnlineShowtimeMode::KF_INTRO_DURATION_SECONDS = 0.0002f;

// EGameModeState (GameStateModuleIO) state-machine ids, X360-attested by the raw 0..7 the
// SetCurrentState pseudocode passes (named file-local, mirroring the committed BrnGameMode.cpp).
enum
{
    KI_GMS_INTRO       = 1,
    KI_GMS_IN_PROGRESS = 2,
    KI_GMS_OUTRO       = 3,
    KI_GMS_RESULTS     = 4,
    KI_GMS_QUIT        = 5
};

// X360: BrnGameState::OnlineShowtimeMode::GetName (0x827E25F0).
const char* OnlineShowtimeMode::GetName() const
{
    return "OnlineShowtime";
}

// X360: BrnGameState::OnlineShowtimeMode::GetIntroDurationSeconds (0x827E2600). Returns the
// near-zero timed-intro constant. DWARF shape is f32 (Hex-Rays widens the FP return to double).
f32 OnlineShowtimeMode::GetIntroDurationSeconds() const
{
    return KF_INTRO_DURATION_SECONDS;
}

// X360: BrnGameState::OnlineShowtimeMode::SendEvent (0x82331330). Drives the Showtime online
// mode's state machine. ABORT -> Quit; RESTART -> Intro; otherwise the per-state flow
// Intro->InProgress->Outro->Results->Quit. SetCurrentState returns void (the pseudocode's
// result/return-result are register artifacts, dropped). The default arm should never be reached.
void OnlineShowtimeMode::SendEvent(EGameModeEvent leEvent)
{
    if (leEvent == E_GME_ABORT)
    {
        SetCurrentState(KI_GMS_QUIT);
        return;
    }
    if (leEvent == E_GME_RESTART)
    {
        SetCurrentState(KI_GMS_INTRO);
        return;
    }

    switch (meCurrentState)
    {
        case KI_GMS_INTRO:
            if (leEvent == E_GME_NEXT || leEvent == E_GME_USER_ACCEPT)
            {
                SetCurrentState(KI_GMS_IN_PROGRESS);
            }
            break;
        case KI_GMS_IN_PROGRESS:
            if (leEvent == E_GME_NEXT)
            {
                SetCurrentState(KI_GMS_OUTRO);
            }
            break;
        case KI_GMS_OUTRO:
            if (leEvent == E_GME_NEXT)
            {
                SetCurrentState(KI_GMS_RESULTS);
            }
            break;
        case KI_GMS_RESULTS:
            if (leEvent == E_GME_USER_ACCEPT || leEvent == E_GME_NEXT)
            {
                SetCurrentState(KI_GMS_QUIT);
            }
            break;
        case KI_GMS_QUIT:
            break;
        default:
            CGS_ASSERT(false, "Should not be in this state in Showtime mode!");
            break;
    }
}

// X360: BrnGameState::OnlineShowtimeMode::Start (0x82322278).
//
// Online showtime mode set-up: mirrors CrashMode::Start (same crash/showtime rules) but pulls the
// per-player network ids from the ModeManager's cached StartNetworkGameEvent and places the online
// race cars via ModeManager::SetOnlineRaceCars. Raw-offset pokes on the OLD GameModeParams layout
// are de-inlined to the committed named members; the first/third params are dropped by Hex-Rays.
void OnlineShowtimeMode::Start(const StartGameModeParams* /*lpStartGameModeParams*/,
                              GameModeParams*             lpGameModeParams,
                              ScoringSystem*              /*lpScoringSystem*/)
{
    // v5 in the pseudocode: the StartNetworkGameEvent the ModeManager cached when the network game
    // started. X360 *(*(this+160)+28004) == the NetworkRoundManager (owned by mpModeManager) cached
    // event. Reconstructed via the two DWARF-attested accessors (ModeManager::GetNetworkRoundManager
    // -> NetworkRoundManager::GetNetworkGameEvent) rather than an invented single accessor.
    const GameStateModuleIO::StartNetworkGameEvent* lpStartNetworkGameEvent =
        GetModeManager()->GetNetworkRoundManager()->GetNetworkGameEvent();

    // *(this+172)=1 -> GameMode base mbConstructed (offset 172, named in BrnGameMode.cpp::Construct).
    mbConstructed = true;

    lpGameModeParams->Construct(GameStateModuleIO::E_MODE_ONLINE_SHOWTIME);

    // Crash/showtime traffic density loaded from .data @0x82005548 (see CrashMode::Start note: bytes
    // not in references/, provably not 0.0f, modelled as a named estimate; INTEGRATOR resolves from XEX).
    static const f32 KF_CRASH_TRAFFIC_DENSITY_SCALE = 2.0f; // .data @0x82005548 (value UNRESOLVED; estimate, not 0.0f)
    lpGameModeParams->SetTrafficDensityScale(KF_CRASH_TRAFFIC_DENSITY_SCALE);

    lpGameModeParams->SetLargeVehicleProbability(1.3f);
    lpGameModeParams->mbIsOnline = true;        // X360 *(a3+148)=1 (mbIsOnline is a public member; no SetIsOnline exists)

    // muFlags |= 0x1042139E (low-word crash mask; high dword 0x82000000 is the data-base artifact).
    lpGameModeParams->SetFlag(
        GameModeParams::KU_FLAG_REMOVE_RIVALS_FROM_WORLD
      | GameModeParams::KU_FLAG_DISABLE_CRASH_CLEAN_UP
      | GameModeParams::KU_FLAG_ENABLE_EASY_CRASHING
      | GameModeParams::KU_FLAG_PLAYER_MUST_BE_CRASHING
      | GameModeParams::KU_FLAG_SET_DIRECTOR_TO_CRASH_MODE_AFTER_INTRO
      | GameModeParams::KU_FLAG_ALLOW_CRASH_PLAY_CONTROLS
      | GameModeParams::KU_FLAG_USE_SHOWTIME_VEHICLE_BEHAVIOUR
      | GameModeParams::KU_FLAG_HARDCORE_TRAFFIC_SWERVING
      | GameModeParams::KU_FLAG_DISABLE_TRAFFIC_RESET
      | GameModeParams::KU_FLAG_DISABLE_ALL_TDS
      | GameModeParams::KU_FLAG_EASY_SMASH_PROPS);

    // Copy the eight per-player network ids from the start event (v5 words 30..37 == bytes 120..148
    // == StartNetworkGameEvent::maNetworkPlayerID[8]) into the mode params
    // (a3+280..308 == GameModeParams::maNetworkPlayerID[8]). After growing the committed home, both
    // sides are BrnNetwork::NetworkPlayerID (typedef s32) -- the assignment is type-matched.
    for (s32 liPlayer = 0; liPlayer < GameStateModuleIO::KI_MAX_RACE_CARS; ++liPlayer)
    {
        lpGameModeParams->maNetworkPlayerID[liPlayer] =
            lpStartNetworkGameEvent->maNetworkPlayerID[liPlayer];
    }

    // Place the online race cars on the grid from the start event (DWARF-attested 2-arg const).
    GetModeManager()->SetOnlineRaceCars(lpGameModeParams, lpStartNetworkGameEvent);
}
}

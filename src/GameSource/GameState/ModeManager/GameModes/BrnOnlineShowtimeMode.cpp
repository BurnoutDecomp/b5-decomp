#include "GameSource/GameState/ModeManager/GameModes/BrnOnlineShowtimeMode.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

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
}

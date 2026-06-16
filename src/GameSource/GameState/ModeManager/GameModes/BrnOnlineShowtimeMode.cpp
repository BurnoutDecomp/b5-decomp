#include "GameSource/GameState/ModeManager/GameModes/BrnOnlineShowtimeMode.h"

namespace BrnGameState
{
const f32 OnlineShowtimeMode::KF_INTRO_DURATION_SECONDS = 0.0002f;

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
}

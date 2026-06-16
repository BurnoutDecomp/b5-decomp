#include "GameSource/GameState/ModeManager/GameModes/BrnBurningRoute.h"

namespace BrnGameState
{
const f32 BurningRouteMode::KF_OUTRO_TIME_SECONDS = 0.0f;

// X360: BrnGameState::BurningRouteMode::GetName.
const char* BurningRouteMode::GetName() const
{
    return "BurningRoute";
}

// X360: BrnGameState::BurningRouteMode::GetOutroTimeout. Returns the fixed constant (0.0). The DWARF
// declaration (virtual / trailing const / f32) is authoritative over the Hex-Rays double.
f32 BurningRouteMode::GetOutroTimeout() const
{
    return KF_OUTRO_TIME_SECONDS;
}
}

#include "GameSource/GameState/ModeManager/GameModes/BrnSurvivor.h"

namespace BrnGameState
{
const f32 SurvivorMode::KF_OUTRO_TIME_SECONDS = 0.0f;

// X360: BrnGameState::SurvivorMode::GetName.
const char* SurvivorMode::GetName() const
{
    return "Survivor";
}

// X360: BrnGameState::SurvivorMode::GetOutroTimeout. Returns the fixed constant (0.0). The DWARF
// declaration (virtual / trailing const / f32) is authoritative over the Hex-Rays double.
f32 SurvivorMode::GetOutroTimeout() const
{
    return KF_OUTRO_TIME_SECONDS;
}
}

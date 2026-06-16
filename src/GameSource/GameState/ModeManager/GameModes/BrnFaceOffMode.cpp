#include "GameSource/GameState/ModeManager/GameModes/BrnFaceOffMode.h"

namespace BrnGameState
{
const f32 FaceOffMode::KF_OUTRO_TIME_SECONDS = 0.0f;

// X360: BrnGameState::FaceOffMode::GetName.
const char* FaceOffMode::GetName() const
{
    return "FaceOff";
}

// X360: BrnGameState::FaceOffMode::GetOutroTimeout. Returns the fixed constant (0.0). The DWARF
// declaration (virtual / trailing const / f32) is authoritative over the Hex-Rays double.
f32 FaceOffMode::GetOutroTimeout() const
{
    return KF_OUTRO_TIME_SECONDS;
}
}

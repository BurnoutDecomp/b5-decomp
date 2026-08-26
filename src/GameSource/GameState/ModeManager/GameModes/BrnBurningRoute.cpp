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

// X360 vtable slot 23 (vtbl+92), folded leaf 0x827E2F38 == `li r3,0; blr` at slot 23 of
// BurningRouteMode's vtable 0x820D06B8; the GameMode base is 0x82C296C8 == `li r3,1`.
// SetupGameMode @0x8234B158 gates the WaitForStreaming path on this.
bool BurningRouteMode::RequiresStreaming() const
{
    return false;
}
}

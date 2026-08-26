#include "GameSource/GameState/ModeManager/GameModes/BrnOnlineBurningHomeRunMode.h"

namespace BrnGameState
{
// X360: BrnGameState::OnlineBurningHomeRunMode::GetName. Trivial virtual override of GameMode::GetName;
// returns the mode's fixed name string. The virtual/trailing-const shape is from the DWARF
// declaration (the Hex-Rays pseudocode renders it as a plain function and drops const).
const char* OnlineBurningHomeRunMode::GetName() const
{
    return "OnlineBurningHomeRun";
}

// X360 vtable slot 24 (vtbl+96): 0x82C296C8 == `li r3,1; blr` at slot 24 of vtable 0x820D0960,
// against the GameMode base's 0x827E2F38 == `li r3,0; blr`. SetupGameMode @0x8234B158 reads it
// twice and HandleLoadingScreenLoaded @0x8234B8A8 once -- an online burning home run DOES put up a loading
// screen, which is what separates it from every offline mode.
bool OnlineBurningHomeRunMode::HasLoadingScreen() const
{
    return true;
}
}

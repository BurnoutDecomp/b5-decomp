#include "GameSource/GameState/ModeManager/GameModes/BrnOnlineRaceMode.h"

namespace BrnGameState
{
// X360: BrnGameState::OnlineRaceMode::GetName. Trivial virtual override of GameMode::GetName;
// returns the mode's fixed name string. The virtual/trailing-const shape is from the DWARF
// declaration (the Hex-Rays pseudocode renders it as a plain function and drops const).
const char* OnlineRaceMode::GetName() const
{
    return "OnlineRace";
}
}

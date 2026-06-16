#include "GameSource/GameState/ModeManager/GameModes/BrnOnlineFreeBurnMode.h"

namespace BrnGameState
{
// X360: BrnGameState::OnlineFreeBurnMode::GetName. Trivial virtual override of GameMode::GetName;
// returns the mode's fixed name string. The virtual/trailing-const shape is from the DWARF
// declaration (the Hex-Rays pseudocode renders it as a plain function and drops const).
const char* OnlineFreeBurnMode::GetName() const
{
    return "OnlineFreeBurn";
}
}

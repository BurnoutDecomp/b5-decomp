#include "GameSource/GameState/ModeManager/GameModes/BrnOnlineFreeBurnLobbyMode.h"

namespace BrnGameState
{
// X360: BrnGameState::OnlineFreeBurnLobbyMode::GetName. Trivial virtual override of GameMode::GetName;
// returns the mode's fixed name string. The virtual/trailing-const shape is from the DWARF
// declaration (the Hex-Rays pseudocode renders it as a plain function and drops const).
const char* OnlineFreeBurnLobbyMode::GetName() const
{
    return "OnlineFreeBurnLobby";
}
}

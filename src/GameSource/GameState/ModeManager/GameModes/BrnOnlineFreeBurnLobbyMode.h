#pragma once

#include "GameSource/GameState/ModeManager/GameModes/BrnOnlineGameMode.h"

namespace BrnGameState
{
// OnlineFreeBurnLobbyMode is a concrete game mode. The base types (OnlineGameMode -> GameMode) are #included from
// their own owning headers rather than forked locally. Only GetName is owned by this TU;
// OnlineFreeBurnLobbyMode's remaining members/methods belong to the BrnOnlineFreeBurnLobbyMode.cpp TU.
class OnlineFreeBurnLobbyMode : public OnlineGameMode
{
public:
    virtual const char* GetName() const;
};
}

#pragma once

#include "GameSource/GameState/ModeManager/GameModes/BrnOnlineGameMode.h"

namespace BrnGameState
{
// OnlineRaceMode is a concrete game mode. The base types (OnlineGameMode -> GameMode) are #included from
// their own owning headers rather than forked locally. Only GetName is owned by this TU;
// OnlineRaceMode's remaining members/methods belong to the BrnOnlineRaceMode.cpp TU.
class OnlineRaceMode : public OnlineGameMode
{
public:
    virtual const char* GetName() const;
};
}

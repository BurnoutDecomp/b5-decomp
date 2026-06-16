#pragma once

#include "GameSource/GameState/ModeManager/GameModes/BrnOnlineGameMode.h"

namespace BrnGameState
{
// OnlineRoadRageMode is a concrete game mode. The base types (OnlineGameMode -> GameMode) are #included from
// their own owning headers rather than forked locally. Only GetName is owned by this TU;
// OnlineRoadRageMode's remaining members/methods belong to the BrnOnlineRoadRageMode.cpp TU.
class OnlineRoadRageMode : public OnlineGameMode
{
public:
    virtual const char* GetName() const;
};
}

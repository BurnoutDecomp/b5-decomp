#pragma once

#include "GameSource/GameState/ModeManager/GameModes/BrnOnlineGameMode.h"

namespace BrnGameState
{
// OnlineBurningHomeRunMode is a concrete game mode. The base types (OnlineGameMode -> GameMode) are #included from
// their own owning headers rather than forked locally. Only GetName is owned by this TU;
// OnlineBurningHomeRunMode's remaining members/methods belong to the BrnOnlineBurningHomeRunMode.cpp TU.
class OnlineBurningHomeRunMode : public OnlineGameMode
{
public:
    virtual const char* GetName() const;
};
}

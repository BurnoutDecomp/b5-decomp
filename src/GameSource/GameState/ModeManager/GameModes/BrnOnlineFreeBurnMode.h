#pragma once

#include "GameSource/GameState/ModeManager/GameModes/BrnOnlineGameMode.h"

namespace BrnGameState
{
// OnlineFreeBurnMode is a concrete game mode. The base types (OnlineGameMode -> GameMode) are #included from
// their own owning headers rather than forked locally. Only GetName is owned by this TU;
// OnlineFreeBurnMode's remaining members/methods belong to the BrnOnlineFreeBurnMode.cpp TU.
class OnlineFreeBurnMode : public OnlineGameMode
{
public:
    virtual const char* GetName() const;
};
}

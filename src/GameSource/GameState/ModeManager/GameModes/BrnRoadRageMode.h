#pragma once

#include "GameSource/GameState/ModeManager/GameModes/BrnOfflineGameMode.h"

namespace BrnGameState
{
// RoadRageMode is a concrete game mode. The base types (OfflineGameMode -> GameMode) are #included from
// their own owning headers rather than forked locally. Only GetName is owned by this TU;
// RoadRageMode's remaining members/methods belong to the BrnRoadRageMode.cpp TU.
class RoadRageMode : public OfflineGameMode
{
public:
    virtual const char* GetName() const;
};
}

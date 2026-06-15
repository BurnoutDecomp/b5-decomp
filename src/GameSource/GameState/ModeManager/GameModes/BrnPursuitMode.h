#pragma once

#include "GameSource/GameState/ModeManager/GameModes/BrnOfflineGameMode.h"

namespace BrnGameState
{
// PursuitMode is a concrete game mode. The base types (OfflineGameMode, GameMode)
// are #included from their own owning headers rather than forked locally. Only
// GetName is owned by this TU; PursuitMode's remaining members/methods belong to
// the BrnPursuitMode.cpp TU.
class PursuitMode : public OfflineGameMode
{
public:
    virtual const char* GetName() const;
};
}

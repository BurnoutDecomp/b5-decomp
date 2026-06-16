#pragma once

#include "types.hpp"
#include "GameSource/GameState/ModeManager/GameModes/BrnOfflineGameMode.h"

namespace BrnGameState
{
// BurningRouteMode is a concrete offline game mode. Bases (OfflineGameMode -> GameMode) are #included
// from their owning headers, never forked. Only GetName and GetOutroTimeout are owned by this
// TU (DWARF-attested); the rest of the mode belongs to BrnBurningRoute.cpp.
class BurningRouteMode : public OfflineGameMode
{
public:
    virtual const char* GetName() const;
    virtual f32         GetOutroTimeout() const;

private:
    // DWARF: BrnBurningRoute.cpp:27. The mode's fixed outro timeout; the X360 GetOutroTimeout body
    // returns 0.0, so this constant is 0.0f for this build (same shape as RaceMode).
    static const f32 KF_OUTRO_TIME_SECONDS;
};
}

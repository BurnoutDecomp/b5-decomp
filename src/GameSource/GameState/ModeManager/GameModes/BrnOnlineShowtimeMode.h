#pragma once

#include "types.hpp"
#include "GameSource/GameState/ModeManager/GameModes/BrnOnlineGameMode.h"

namespace BrnGameState
{
// OnlineShowtimeMode is a concrete online game mode. Bases (OnlineGameMode -> GameMode) are
// #included from their owning headers, never forked. Only GetName and GetIntroDurationSeconds
// are owned by this TU.
class OnlineShowtimeMode : public OnlineGameMode
{
public:
    virtual const char* GetName() const;
    virtual f32         GetIntroDurationSeconds() const;

private:
    // X360 GetIntroDurationSeconds returns 0.0002 (a near-zero timed intro); 0.0002f is the
    // float32 the X360 build returns. Named per the project rule to reverse inlined constants.
    static const f32 KF_INTRO_DURATION_SECONDS;
};
}

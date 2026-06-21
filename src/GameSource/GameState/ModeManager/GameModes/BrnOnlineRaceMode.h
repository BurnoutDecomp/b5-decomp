#pragma once

#include "GameSource/GameState/ModeManager/GameModes/BrnOnlineGameMode.h"

namespace BrnGameState
{
// OnlineRaceMode is a concrete game mode. The base types (OnlineGameMode -> GameMode) are #included from
// their own owning headers rather than forked locally. GetName is owned by the GetName-only sibling TU
// (BrnOnlineRaceMode GetName); this header additionally declares the two virtuals owned by the
// BrnOnlineRaceMode.cpp TU -- PreWorldUpdate and GetOutroTimeout -- as overrides of the GameMode base
// slots (BrnGameMode.h declares both). Their bodies live in BrnOnlineRaceMode.cpp.
class OnlineRaceMode : public OnlineGameMode
{
public:
    virtual const char* GetName() const;

    // X360: BrnGameState::OnlineRaceMode::PreWorldUpdate (0x82330EB0). Overrides GameMode::PreWorldUpdate.
    // Per-frame: runs the base tick, then -- while the race is in progress -- recomputes the post-finish
    // time limit from the winner's finish time. Kept the committed no-arg base shape (the X360 build
    // receives the per-frame world-IO buffers + ScoringSystem as args; the ScoringSystem it needs is
    // reached through the ModeManager instead, see the .cpp).
    virtual void PreWorldUpdate();

    // X360: BrnGameState::OnlineRaceMode::GetOutroTimeout (0x82330FD0). Overrides GameMode::GetOutroTimeout.
    // Returns the post-race outro hold time for an online race, derived from how far behind the race
    // winner the local player finished.
    virtual f32 GetOutroTimeout() const;
};
}

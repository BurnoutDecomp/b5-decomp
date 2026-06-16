#pragma once

#include "types.hpp"
#include "GameSource/GameState/ModeManager/GameModes/BrnOfflineGameMode.h"

namespace BrnGameState
{
// StuntAttackMode is a concrete offline game mode ("Stunt Race"). Bases (OfflineGameMode ->
// GameMode) are #included from their owning headers, never forked. The four X360-attested
// accessors below are owned by this TU; the rest of the mode (Start/PreWorldUpdate/...) lands
// with the BrnStuntAttackMode.cpp TU and extends this header. Overrides bind to the base
// GameMode vtable by signature, so only the owned overrides are declared here (minimal slice).
class StuntAttackMode : public OfflineGameMode
{
public:
    virtual const char* GetName() const;
    virtual f32         GetIntroDurationSeconds() const;
    virtual f32         GetOutroTimeout() const;
    virtual bool        ShouldCountdownEnd() const;

private:
    // DWARF member layout (BrnStuntAttackMode.h:97-100). Named members (no raw-offset access);
    // mbPlayerPointingInStartDirection is the gate ShouldCountdownEnd returns (recomputed each
    // frame by PreWorldUpdate, which lands with the full mode TU).
    Vector3 mStartDir;
    f32     mfCountdownTimer;
    bool    mbPlayerPointingInStartDirection;
    bool    mbNeedToFillBoost;

    // BrnStuntAttackMode.cpp:25 (DWARF). GetIntroDurationSeconds returns 6.0 for this build.
    static const f32 KF_INTRO_TIME_SECONDS;
};
}

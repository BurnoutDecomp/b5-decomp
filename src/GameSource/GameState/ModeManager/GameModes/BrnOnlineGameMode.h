#pragma once

#include "types.hpp"
#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"

namespace BrnGameState
{
// Owning header for the OnlineGameMode base of the online game-mode hierarchy.
// Reconstructed from the DecFIGS DWARF (BrnOnlineGameMode.h: struct OnlineGameMode :
// public GameMode) and the X360 ledger. It derives directly from GameMode (NOT
// OfflineGameMode) and overrides the two GameMode virtuals the X360 build attests for it:
//   - Construct(ModeManager*)  (identity.json 0x8232FE98)
//   - SendEvent(EGameModeEvent) (identity.json 0x8232FED0)
// Virtual/return-type/param shapes are taken from the GameMode base declaration, not the
// Hex-Rays pseudocode (which renders these as int-returning functions).
//
// Deliberately omitted (X360 ledger decides what exists):
//   - GetFrameRateType() const: present in the PS3 DWARF but absent from the X360 ledger,
//     so it is PS3-only drift and is left out.
//   - KF_ONLINE_TRAFFIC_DENSITY: a file-scope const in the original, not a member, and not
//     used by the reconstructed functions; left out of this minimal header.
//
// OnlineGameMode adds no data members of its own that the X360 build attests: the state it
// touches (meCurrentState, mpModeManager, mbConstructed, mbFinalStandingsShown) is all
// GameMode base state (see BrnGameMode.h).
class OnlineGameMode : public GameMode
{
public:
    virtual void Construct(ModeManager* lpModeManager);
    virtual void SendEvent(EGameModeEvent leEvent);
};
}

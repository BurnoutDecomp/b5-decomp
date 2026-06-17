#pragma once

#include "GameSource/GameState/ModeManager/GameModes/BrnOfflineGameMode.h"

namespace BrnGameState
{
// CrashMode is the offline Crash/Showtime game mode. The base chain (OfflineGameMode -> GameMode)
// is #included from the owning headers; meCurrentState / SetCurrentState / EGameModeEvent come from
// the GameMode base. Minimal slice (BrnPursuitMode.h precedent): only the SendEvent override this
// TU reconstructs is declared; the mode's other DWARF methods land with their own functions.
class CrashMode : public OfflineGameMode
{
public:
    virtual void SendEvent(EGameModeEvent leEvent);

    // X360 0x82322210. Builds the offline crash/showtime GameModeParams.
    virtual void Start(const StartGameModeParams* lpStartGameModeParams,
                       GameModeParams* lpGameModeParams,
                       ScoringSystem* lpScoringSystem);
};
}

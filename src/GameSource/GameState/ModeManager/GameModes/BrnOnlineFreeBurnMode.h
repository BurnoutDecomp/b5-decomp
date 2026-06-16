#pragma once

#include "GameSource/GameState/ModeManager/GameModes/BrnOnlineGameMode.h"
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"  // StartGameModeParams/GameModeParams/ScoringSystem

namespace BrnGameState
{
// OnlineFreeBurnMode is a concrete online game mode. Bases (OnlineGameMode -> GameMode) are
// #included from their owning headers, never forked. GetName / Start / SendEvent are owned here.
class OnlineFreeBurnMode : public OnlineGameMode
{
public:
    virtual const char* GetName() const;
    // X360 0x823160C8 -- an unconditional "should never be called" assert stub on this build
    // (the mode is brought up another way). Third param unused; kept for DWARF-shape parity.
    virtual void        Start(const StartGameModeParams* lpStartGameModeParams,
                              GameModeParams* lpGameModeParams,
                              ScoringSystem* lpScoringSystem);
    virtual void        SendEvent(EGameModeEvent leEvent);
};
}

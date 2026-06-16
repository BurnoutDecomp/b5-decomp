#pragma once

#include "GameSource/GameState/ModeManager/GameModeStates/BrnGameModeState.h"

namespace BrnGameState
{
// The online-loading sub-state of a game mode's state machine (still loading == not finished). Reconstructed from the DecFIGS DWARF (derives from GameModeState, no own data members);
// the body is recovered from the X360 pseudocode (OnEnter @ 0x823166B0). OnEnter overrides the
// GameModeState virtual in slot 0; the back-pointer it drives (mpGameMode) lives on the base.
class OnlineLoadingState : public GameModeState
{
public:
    virtual void OnEnter();
};
}

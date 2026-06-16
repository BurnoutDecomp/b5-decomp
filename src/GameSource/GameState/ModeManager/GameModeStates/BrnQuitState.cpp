#include "GameSource/GameState/ModeManager/GameModeStates/BrnQuitState.h"

#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"

namespace BrnGameState
{
// X360: BrnGameState::QuitState::OnEnter (0x823166A0). The X360 body writes a single byte in the
// GameMode latch run (173..178); restored here to the named setter rather than raw-offset
// access. The trailing `return result` is the void-function register artifact and is dropped.
void QuitState::OnEnter()
{
    mpGameMode->SetFinished(true);
}
}

#include "GameSource/GameState/ModeManager/GameModeStates/BrnResultsState.h"

#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"

namespace BrnGameState
{
// X360: BrnGameState::ResultsState::OnEnter (0x82316690). The X360 body writes a single byte in the
// GameMode latch run (173..178); restored here to the named setter rather than raw-offset
// access. The trailing `return result` is the void-function register artifact and is dropped.
void ResultsState::OnEnter()
{
    mpGameMode->SetShowResultsRequest(true);
}
}

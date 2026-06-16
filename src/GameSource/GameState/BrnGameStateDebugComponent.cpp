#include "GameSource/GameState/BrnGameStateDebugComponent.h"
#include "GameSource/GameState/BrnGameStateModule.h"   // BrnGameState::GameStateModule (full def: declares ToggleShowtimeBehaviour)

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x823578F8
//   (BrnGameState::GameStateDebugComponent::ToggleShowtimeCallback)
//
// The "Toggle showtime" debug-menu action. The void* context the menu passes back is this component
// (registered via RegisterFunction(&ToggleShowtimeCallback, this, ...)); it reaches the owning
// game-state module through its back-pointer and toggles showtime behaviour. The X360 body is fully
// inlined to `*(*(this + 12) + 284512) = 1`, i.e. the inlined body of
// GameStateModule::ToggleShowtimeBehaviour() (sets mbToggleShowtimeBehaviour = true). Reconstructed as
// that call so the side effect stays owned by GameStateModule (its own reconstruction pass).
namespace BrnGameState
{
    void GameStateDebugComponent::ToggleShowtimeCallback(void* lpData)
    {
        GameStateDebugComponent* lpThis = static_cast<GameStateDebugComponent*>(lpData);
        lpThis->mpGameStateModule->ToggleShowtimeBehaviour();
    }
}

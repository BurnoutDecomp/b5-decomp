#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"   // CgsDev::DebugComponent (real base)

// BrnGameState::GameStateDebugComponent - the in-game game-state debug menu. Derives from the real
// CgsDev::DebugComponent and registers the game-state debug toggles + action callbacks (toggle
// showtime, clear training flags, play training message, ...) with the debug UI. Recovered from the
// DecFIGS DWARF (GameState/BrnGameStateDebugComponent.h). Incremental: only the slice this pass
// implements (the ToggleShowtime action + its back-pointer) is declared; the render / landmark /
// stunt-breakdown methods and the remaining members are reconstructed by their own passes.
namespace BrnGameState
{
    class GameStateModule;   // pointer member; full definition in BrnGameStateModule.h (the .cpp includes it)

    class GameStateDebugComponent : public CgsDev::DebugComponent
    {
    private:
        // The "Toggle showtime" action callback registered with the debug menu; the void* context the
        // menu passes back is this component (registered via RegisterFunction(..., this, ...)).
        static void ToggleShowtimeCallback(void* lpData);   // @ 0x823578F8

        // First derived member -> lands at offset 12 (vtable ptr + CgsDev::DebugComponent base
        // subobject = 12 bytes), matching the X360 `*(this + 12)` access.
        GameStateModule* mpGameStateModule;
    };
}

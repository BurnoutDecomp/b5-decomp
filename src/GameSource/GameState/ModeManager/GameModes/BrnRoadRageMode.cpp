#include "GameSource/GameState/ModeManager/GameModes/BrnRoadRageMode.h"

namespace BrnGameState
{
// X360: BrnGameState::RoadRageMode::GetName. Trivial virtual override of GameMode::GetName;
// returns the mode's fixed name string. The virtual/trailing-const shape is from the DWARF
// declaration (the Hex-Rays pseudocode renders it as a plain function and drops const).
const char* RoadRageMode::GetName() const
{
    return "RoadRage";
}

// X360 vtable slot 13 (vtbl+52), folded leaf 0x827E2F38 == `li r3,0; blr` at slot 13 of
// RoadRageMode's vtable 0x820D05E8 (the offline base carries GameMode::ShouldExit 0x82315B80
// there instead). Road rage ends on its own takedown/timer conditions -- ShouldFinish, slot 14 --
// never on the shared idle-exit test.
bool RoadRageMode::ShouldExit(const ScoringSystem* lpScoringSystem) const
{
    (void)lpScoringSystem;
    return false;
}

// X360 vtable slot 23 (vtbl+92), folded leaf 0x827E2F38 == `li r3,0; blr` (the base is
// 0x82C296C8 == `li r3,1`). SetupGameMode @0x8234B158 gates the streaming wait on this.
bool RoadRageMode::RequiresStreaming() const
{
    return false;
}
}

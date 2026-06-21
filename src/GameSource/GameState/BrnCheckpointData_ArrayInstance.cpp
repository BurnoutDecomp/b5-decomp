#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/GameState/BrnCheckpointData.h"   // BrnGameState::CheckpointData element home

// Explicit instantiation of the generic Array<T,N> indexed accessor (inline in CgsArray.h) for the
// Array<BrnGameState::CheckpointData, 16> leaf instantiation -- the committed Array_/EventQueue_
// explicit-instantiation pattern.
//
// X360 0x822AE100 (class:BrnGameState catch-all TU, ledger "BrnGameState::CheckpointData,16>:") =
// the bounds-checked indexed accessor of GameModeParams' checkpoint array. The X360 body is the
// "Array used before Construct/Clear" (CgsArray.h:538) + "Array index out of bounds" (539) sequence
// returning &maElements[i] (stride 44 == sizeof(CheckpointData); 16*44 == 704 == the owner+704 count
// offset the callers index). Non-const overload (AddRaceCarToStartingGrid / SetupOpponents /
// TranslateGameActionsToGuiEvents / OnModeStart all index a mutable checkpoint record).
template BrnGameState::CheckpointData& Array<BrnGameState::CheckpointData, 16>::operator[](u32);

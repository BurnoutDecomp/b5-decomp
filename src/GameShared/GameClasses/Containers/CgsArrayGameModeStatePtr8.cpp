// Per-instantiation .cpp for Array<BrnGameState::GameModeState*, 8>. The generic
// Array<T,N> body is fully inline in CgsArray.h, so this TU is just the
// explicit class instantiation (the X360 emits one out-of-line copy per
// using-TU). The method the X360 emitted for this instance:
//   Array<GameModeState*,8>::GetItem @ 0x8231B168
//       (BrnGameState::GameMode::SetCurrentState, ::Construct, ::PreWorldUpdate)
//
// Layout: maElements[8] (8 * 4 = 32B; GameModeState* is a 32-bit pointer on X360)
// + miCount @ +0x20, matching the X360 count word read at *(this+32) in GetItem
// and the 4-byte element stride (return 4*index + base in the asm).
//
// Spelled with full namespace qualification because GameModeState lives in
// BrnGameState (BrnGameModeState.h); the DWARF spells it
// CgsContainers::Array<BrnGameState::GameModeState*,8u>.
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/GameState/ModeManager/GameModeStates/BrnGameModeState.h"

template class Array<BrnGameState::GameModeState*, 8>;

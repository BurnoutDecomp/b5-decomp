// Per-instantiation .cpp for Array<s32,8>. The generic Array<T,N> body (Append /
// FindFirstInstanceOf / GetItem + siblings) is fully inline in CgsArray.h, so this TU is
// just the explicit class instantiation (the X360 emits one out-of-line copy per using-TU):
//   Array<int,8>::Append           @ 0x823183D8  (OnlineRoundResults::SetPosition,
//                                                  ModeManager::SetupOnlineStartingGrid,
//                                                  ModeManager::SendModeStopMessages)
//   Array<int,8>::FindFirstInstanceOf @ 0x823184F8 (ModeManager::SetupOnlineStartingGrid)
//   Array<int,8>::GetItem (non-const) @ 0x8254E1D0 (OnlineRoundResults::GetPosition)
// Layout: maElements[8] (32B) + miCount @ +0x20, matching the X360 result[8]/*(a1+32) count word.
//
// Spelled unqualified to match the committed Array<T,N> container convention (CgsArray.h);
// the DWARF spells the type CgsContainers::Array<int,8u>. int == s32 (types.hpp).
//
// NOTE (generic-body parity gap, NOT re-forked here): the X360 Array<int,8>::GetItem carries
// the unconstructed + out-of-bounds bounds asserts (CgsArray.h:538/539) that the committed
// generic GetItem omits. Fixing that must GROW the shared CgsArray.h body so all Array
// instantiations re-verify; it is deliberately not specialized in this thin instantiation TU.
#include "GameShared/GameClasses/Containers/CgsArray.h"

template class Array<s32, 8>;

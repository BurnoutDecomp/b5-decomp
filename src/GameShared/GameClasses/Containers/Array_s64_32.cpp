// Per-instantiation .cpp for Array<s64,32>. The generic Array<T,N> body (Append,
// GetItem/operator[], GetLength + siblings) is fully inline in CgsArray.h, so this TU
// is just the explicit class instantiation (the X360 emits one out-of-line copy per
// using-TU):
//   Array<s64,32>::Append    @ 0x8235CA20  (BrnGameState::TriggerQueryManager::ProcessPlayerTriggers)
//   Array<s64,32>::GetItem   @ 0x8270C068  (BrnTraffic::TrafficEntityModule::HandleExternalRequests)
//   Array<s64,32>::GetLength @ 0x82709328  (BrnTraffic::TrafficEntityModule::HandleExternalRequests)
//
// Layout (recovered from the asm): maElements[32] (32 * 8 = 0x100 bytes) + miCount @ +0x100.
// Stride 8 (s64); GetLength reads the count word at +0x100.
//
// Spelled unqualified to match the committed Array<T,N> container convention (CgsArray.h).
// s64 == int64_t (types.hpp); a primitive element needs no element_home include.
#include "GameShared/GameClasses/Containers/CgsArray.h"

template class Array<s64, 32>;

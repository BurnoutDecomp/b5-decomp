// Per-instantiation .cpp for Array<s64, 15>. The generic Array<T,N> body
// (Append / GetItem / GetLength + siblings) is fully inline in CgsArray.h, so this TU
// is just the explicit class instantiation (the X360 emits one out-of-line copy per
// using-TU). The three methods the X360 emitted for this instance:
//   Array<s64,15>::Append    @ 0x8254D5A8  (BrnNetwork::EventScoresManager::_UploadEventScoreCallback)
//   Array<s64,15>::GetItem   @ 0x8235F5E8  (BrnGameState::GameStateModule::ProcessGameEvents)
//   Array<s64,15>::GetLength @ 0x8235CEF8  (BrnGameState::GameStateModule::ProcessGameEvents)
//
// Layout: maElements[15] (15 * 8 = 120B; each element is one 64-bit word) + miCount
// @ +0x78, matching the X360 count word read/written at *(a1+0x78), the slwi-by-3
// element addressing (8-byte stride), and the single-dword element copy.
//
// Spelled unqualified to match the committed Array<T,N> container convention (CgsArray.h);
// the DWARF spells the type CgsContainers::Array<__int64,15u>. s64 == int64_t (types.hpp);
// a primitive element needs no element_home include.
#include "GameShared/GameClasses/Containers/CgsArray.h"

template class Array<s64, 15>;

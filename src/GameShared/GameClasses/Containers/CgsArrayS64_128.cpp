// Per-instantiation .cpp for Array<s64,128>. The generic Array<T,N> body (Append +
// Contains + FindFirstInstanceOf + GetItem + GetLength + siblings) is fully inline in
// CgsArray.h, so this TU is just the explicit class instantiation (the X360 emits one
// out-of-line copy per using-TU):
//   Array<__int64,128>::Append              @ 0x8235CC68
//   Array<__int64,128>::GetLength           @ 0x8235CD88
//   Array<__int64,128>::FindFirstInstanceOf @ 0x8235CDE0
//   Array<__int64,128>::Contains            @ 0x8235CE78
//   Array<__int64,128>::GetItem             @ 0x8235F368
//     (reached from BrnGameState::GameStateModule vehicle-selection paths:
//      GetListOfPlayerSelectableVehicles / RequestStreamingForVehicleSelection /
//      ProcessGameEvents / OnlineCarSelectManager::EnterOnlineCarSelect, and
//      BrnGame::BrnGameModule::TranslateGameActionsToGuiEvents)
//
// Layout (recovered from asm): maElements[128] (1024B, stride 8 == sizeof(s64)) + miCount
// @ +0x400 (== 1024), matching the X360 *(a1+0x400) count word. Capacity 128 is attested
// by the `cmplwi ...,0x80` room check in Append (and the `li r4,0x80` capacity literal in
// its out-of-space message); the 8-byte stride by the `slwi r11,r11,3` (Append/GetItem)
// and `ld`/`cmpld`/`addi r11,r11,8` (FindFirstInstanceOf) qword ops.
//
// Spelled unqualified to match the committed Array<T,N> container convention (CgsArray.h);
// the DWARF spells the type CgsContainers::Array<__int64,128u>. __int64 == s64 (types.hpp);
// a primitive element needs no element_home include. The existing CgsArrayInt64.cpp is
// Array<s32,64> (a DIFFERENT type), so a distinct new file is required.
#include "GameShared/GameClasses/Containers/CgsArray.h"

template class Array<s64, 128>;

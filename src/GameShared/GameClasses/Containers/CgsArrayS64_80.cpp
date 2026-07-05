// Per-instantiation .cpp for Array<s64,80>. The generic Array<T,N> body (Append +
// Contains + FindFirstInstanceOf + IsFull + siblings) is fully inline in CgsArray.h, so
// this TU is just the explicit class instantiation (the X360 emits one out-of-line copy
// per using-TU):
//   Array<__int64,80>::Append              @ 0x82318FC0
//   Array<__int64,80>::Contains            @ 0x82325470
//   Array<__int64,80>::FindFirstInstanceOf @ 0x8231AD18
//   Array<__int64,80>::IsFull              @ 0x823254F0
//     (all reached from BrnGameState::ChallengeManager::HandleWorldStunt)
//
// Layout (recovered from asm): maElements[80] (640B, stride 8 == sizeof(s64)) + miCount
// @ +0x280 (== 640), matching the X360 *(this+0x280) count word. Capacity 80 is attested
// by the `cmplwi ...,0x50` room check in Append and the `addi ...,-0x50` full check in
// IsFull; the 8-byte stride by the `slwi r11,r11,3` + `stdx`/`ld r10,0(r26)` qword ops in
// Append and the `ld`/`cmpld`/`addi r11,r11,8` qword scan in FindFirstInstanceOf.
//
// The X360 asserts at CgsArray.h:225/226 (Append unconstructed / out-of-space), :480
// (FindFirstInstanceOf unconstructed), :506 (Contains unconstructed) and :336 (IsFull
// unconstructed) -- all shared generic-body lines, not re-forked here.
//
// Spelled unqualified to match the committed Array<T,N> container convention (CgsArray.h);
// __int64 == s64 (types.hpp). A primitive element needs no element_home include. The
// existing CgsArrayInt80.cpp is Array<s32,80> (a DIFFERENT 4-byte-stride type), so a
// distinct new file is required.
#include "GameShared/GameClasses/Containers/CgsArray.h"

template class Array<s64, 80>;

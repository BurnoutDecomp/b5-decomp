// Per-instantiation .cpp for Array<s64,2000>. The generic Array<T,N> body (Append +
// Contains + FindFirstInstanceOf + siblings) is fully inline in CgsArray.h, so this TU is
// just the explicit class instantiation (the X360 emits one out-of-line copy per using-TU):
//   Array<__int64,2000>::Append              @ 0x8235BD60
//   Array<__int64,2000>::Contains            @ 0x82367938
//   Array<__int64,2000>::FindFirstInstanceOf @ 0x8235EC90
//     (all reached from BrnProgression::Profile Freeburn-challenge tracking:
//      CompleteFreeburnChallenge / HasPlayerCompletedFreeburnChallenge)
//
// Layout (recovered from asm): maElements[2000] (16000B, stride 8 == sizeof(s64)) + miCount
// @ +0x3E80 (== 16000), matching the X360 *(a1+0x3E80) count word. Capacity 2000 is
// attested by the `cmplwi ...,0x7D0` room check in Append; the 8-byte stride by the
// `slwi r11,r11,3` (Append) and `ld`/`cmpld`/`addi r11,r11,8` (FindFirstInstanceOf) qword ops.
//
// Spelled unqualified to match the committed Array<T,N> container convention (CgsArray.h);
// __int64 == s64 (types.hpp). A primitive element needs no element_home include. The existing
// CgsArrayInt64.cpp is Array<s32,64> (a DIFFERENT type), so a distinct new file is required.
#include "GameShared/GameClasses/Containers/CgsArray.h"

template class Array<s64, 2000>;

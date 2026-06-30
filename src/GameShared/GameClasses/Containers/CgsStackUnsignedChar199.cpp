// Per-instantiation .cpp for Stack<u8,199>. The generic Stack<Type,N> body (Push / Pop /
// Peek / operator[] + siblings) is fully inline in CgsStack.h, so this TU just forces the
// out-of-line emission of the four members the X360 attests (under the IDA-truncated symbol
// "char,199>"), all driven by BrnTraffic's static-vehicle free-list:
//   Stack<u8,199>::Peek       @ 0x8271AD18  (TrafficEntityModule::StaticVehicles_Generate)
//   Stack<u8,199>::Pop        @ 0x8271AC68  (TrafficEntityModule::StaticVehicles_Generate)
//   Stack<u8,199>::Push       @ 0x8271ABA8  (StaticVehicles_UpdatePurgatory, Reset)
//   Stack<u8,199>::operator[] @ 0x8270C980  (CgsAlgorithms::Shuffle<Stack<u8,199>>, mangled
//                                            ??$Shuffle@EV?$Stack@E$0MH@@CgsContainers@@@ -- E=u8, 0MH=0xC7=199)
//
// Element type unsigned char: the asm slot stride is exactly 1 byte (operator[] returns
// `idx + this`, Peek returns `this + miLength - 1`, Push stores a single _BYTE), so this is
// NOT a wider record despite Hex-Rays' _BYTE* parameter -- u8 is the real element. The count
// word miLength sits at +200 (maData[199] occupies +0..+198, padded; miLength @ +0xC8). The
// X360 Push !IsFull assert compares miLength against 199. Bodies are header-inline in CgsStack.h.
//
// Stack<u8,199> carries a declared-only Contains() (body links elsewhere), so force just the
// four members this TU attests rather than the whole class.
#include "GameShared/GameClasses/Containers/CgsStack.h"

template void              CgsContainers::Stack<u8, 199>::Push(const u8&);
template void              CgsContainers::Stack<u8, 199>::Pop();
template const u8&         CgsContainers::Stack<u8, 199>::Peek() const;
template u8&               CgsContainers::Stack<u8, 199>::operator[](s32);

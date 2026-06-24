// Per-instantiation .cpp for Array<BrnWorld::PropEntityID, 15>. The generic Array<T,N>
// body (Append / GetItem / IsFull + siblings) is fully inline in CgsArray.h, so this TU
// is just the explicit class instantiation (the X360 emits one out-of-line copy per
// using-TU). The three methods the X360 emitted for this instance:
//   Array<PropEntityID,15>::Append  @ 0x822ADB98  (BrnWorld::PropEntityModule::ChangePropState)
//   Array<PropEntityID,15>::GetItem @ 0x822ADCB8  (BrnWorld::PropEntityModule::PreSceneUpdate)
//   Array<PropEntityID,15>::IsFull  @ 0x822CA390  (BrnWorld::PropEntityModule::ChangePropState)
//
// Layout: maElements[15] (15 * 4 = 60B; PropEntityID is one 32-bit EntityId word) + miCount
// @ +0x3C, matching the X360 count word read/written at *(a1+60) (`lwz 0x3C(r29)`), the
// slwi-by-2 element addressing (4-byte stride), and the single-dword element copy
// (`lwz r10,0(r26); stwx r10,r11,r29` in Append == a one-word PropEntityID copy).
//
// The X360 Append/GetItem/IsFull carry the CgsArray.h bounds/used-before-Construct asserts
// (lines 225/226, 556/557, 336) -- supplied by the committed generic CGS_ASSERT body. The
// streamed "Array container out of space / index out of bounds" dynamic messages are kept as
// the static strings in the generic body (parity note recorded in CgsArrayInt8.cpp). IsFull
// compares miCount == 15 (`addi r11,-0xF; cntlzw; extrwi 1,26`).
//
// Spelled unqualified to match the committed Array<T,N> container convention (CgsArray.h);
// the DWARF spells the type CgsContainers::Array<BrnWorld::PropEntityID,15u>. operator== on
// PropEntityID (additive grow in BrnPropEntityID.h) satisfies the equality-based generic
// members the explicit instantiation forces.
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "SharedClasses/Physics/Props/BrnPropEntityID.h"

template class Array<BrnWorld::PropEntityID, 15>;

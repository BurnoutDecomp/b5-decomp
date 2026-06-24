// Per-instantiation .cpp for Array<BrnWorld::PropEntityID, 30>. The generic Array<T,N>
// body (Append / GetItem / IsFull + siblings) is fully inline in CgsArray.h, so this TU
// is just the explicit class instantiation (the X360 emits one out-of-line copy per
// using-TU). The three methods the X360 emitted for this instance:
//   Array<PropEntityID,30>::Append  @ 0x822ADDC0  (BrnWorld::PropEntityModule::ProcessPotentialContactWithPart,
//                                                   BrnWorld::PropEntityModule::ProcessBrokenProps)
//   Array<PropEntityID,30>::GetItem @ 0x822ADEE0  (BrnWorld::PropEntityModule::PreSceneUpdate)
//   Array<PropEntityID,30>::IsFull  @ 0x822CA3F8  (BrnWorld::PropEntityModule::ProcessPotentialContactWithPart)
//
// Layout: maElements[30] (30 * 4 = 120B; PropEntityID is one 32-bit EntityId word) + miCount
// @ +0x78, matching the X360 count word read/written at *(a1+0x78) (`lwz 0x78(r29)`), the
// slwi-by-2 element addressing (4-byte stride), and the single-dword element copy
// (`lwz r10,0(r26); stwx r10,r11,r29` in Append == a one-word PropEntityID copy).
//
// The X360 Append/GetItem/IsFull carry the CgsArray.h bounds/used-before-Construct asserts
// -- supplied by the committed generic CGS_ASSERT body. The capacity literal is 30 (`cmplwi
// 0x1E` in Append, `addi r11,-0x1E` in IsFull); the streamed dynamic out-of-space/out-of-
// bounds messages are kept as the static strings in the generic body (parity note in
// CgsArrayInt8.cpp).
//
// Spelled unqualified to match the committed Array<T,N> container convention (CgsArray.h);
// the DWARF spells the type CgsContainers::Array<BrnWorld::PropEntityID,30u>. operator== on
// PropEntityID (additive grow in BrnPropEntityID.h) satisfies the equality-based generic
// members the explicit instantiation forces.
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "SharedClasses/Physics/Props/BrnPropEntityID.h"

template class Array<BrnWorld::PropEntityID, 30>;

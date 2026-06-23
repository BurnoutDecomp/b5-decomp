// Per-instantiation .cpp for Array<BrnResource::VehicleListEntry::ELiveryType, 8>. The generic
// Array<T,N> body (Append / FindFirstInstanceOf / GetItem + siblings) is fully inline in
// CgsArray.h, so this TU is just the explicit class instantiation (the X360 emits one out-of-line
// copy of Append per using-TU):
//   Array<VehicleListEntry::ELiveryType,8>::Append @ 0x8235C6A8
//       (callers: BrnProgression::DerivedCarArray::ConstructColourLiveryList,
//                 BrnProgression::DerivedCarArray::ConstructPatternLiveryList)
//
// Layout: maElements[8] (8 * 4 = 32B, ELiveryType is a 4-byte enum) + miCount @ +0x20, matching
// the X360 Append's stwx into maElements[miCount] and the miCount word read/written at +0x20.
//
// The X360 Append carries the two CgsArray.h bounds asserts (used-before-Construct @ line 225,
// out-of-space @ line 226) -- supplied by the committed generic CGS_ASSERT body. The streamed
// "Array container out of space, Length/Capacity" dynamic message is kept as the static string in
// the generic body (parity note recorded in CgsArrayInt8.cpp).
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "SharedClasses/DataLists/VehicleListEntry.h"

template class Array<BrnResource::VehicleListEntry::ELiveryType, 8>;

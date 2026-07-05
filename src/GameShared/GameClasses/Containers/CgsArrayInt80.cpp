// Per-instantiation .cpp for Array<s32,80>. The generic Array<T,N> body (Append + GetItem +
// siblings) is fully inline in CgsArray.h, so this TU is just the explicit class instantiation
// (the X360 emits one out-of-line copy per using-TU):
//   Array<int,80>::Append  @ 0x822ADA78  (BrnWorld::PropZoneManager::LoadProp)
//   Array<int,80>::GetItem @ 0x822AF790  (BrnWorld::PropZoneManager::SendTrafficLightRestoreEvents)
// Layout: maElements[80] (320B) + miCount @ +0x140 (0x140 == 320 == 80*4); capacity N=80 from the
// `cmplwi 0x50` out-of-space guard; 4-byte int stride from the `slwi ...,2` element addressing.
//
// Spelled unqualified to match the committed Array<T,N> container convention (CgsArray.h);
// the DWARF spells the type CgsContainers::Array<int,80u>. int == s32 (types.hpp). A primitive
// element needs no element_home include.
#include "GameShared/GameClasses/Containers/CgsArray.h"

template class Array<s32, 80>;

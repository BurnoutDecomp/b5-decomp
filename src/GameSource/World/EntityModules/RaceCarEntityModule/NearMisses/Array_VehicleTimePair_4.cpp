#include "GameShared/GameClasses/Containers/CgsArray.h"   // Array<T,N>::Append/Erase (inline generic)
#include "GameSource/World/EntityModules/RaceCarEntityModule/NearMisses/BrnNearMissData.h" // BrnWorld::VehicleTimePair (8-byte element)

// Array<BrnWorld::VehicleTimePair, 4>::Append @ 0x822AF0E0
// Array<BrnWorld::VehicleTimePair, 4>::Erase  @ 0x822AF208
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic Array<T,N>::Append / Erase bodies
// are already committed inline in CgsArray.h; this TU is the thin explicit instantiation only
// (do NOT re-define the generic). Both X360 bodies match the generic store-for-store:
//
// Append (0x822AF0E0): asserts the array was Construct/Clear'd (miCount @ +0x20 != the -1
//   sentinel, CgsArray.h:225) then asserts there is room (unsigned miCount >= 4 streams
//   "Array container out of space, Length:.. Capacity:..", CgsArray.h:226). Both collapse to
//   the house CGS_ASSERT. It then writes the 8-byte VehicleTimePair at &maElements[miCount]
//   (slwi r11,miCount,3; add base; stw id@0; stw time@4) and increments miCount. The count
//   word sits at byte 0x20 == 4 * sizeof(VehicleTimePair), confirming the inline maElements[4]
//   buffer end and sizeof(VehicleTimePair) == 8.
//
// Erase (0x822AF208): asserts Construct/Clear'd (CgsArray.h:380) and luIndex < miCount
//   ("Trying to erase an unused element", CgsArray.h:381), decrements miCount, then shifts the
//   tail down one slot (order-preserving): for each slot from luIndex up to the new miCount it
//   copies maElements[i+1] (two 4-byte loads/stores at an 8-byte stride) into maElements[i].
//
// Append is called by NearMissData<4,7>::RememberNearMiss / AddContacted; Erase by
// NearMissData<4,7>::UpdateTimers / RememberNearMiss / AddContacted.
template void Array<BrnWorld::VehicleTimePair, 4>::Append(const BrnWorld::VehicleTimePair&);
template void Array<BrnWorld::VehicleTimePair, 4>::Erase(u32);

// Array<BrnWorld::VehicleTimePair, 4>::operator[] (non-const) @ 0x822B0500
// Thin explicit instantiation (generic body inline in CgsArray.h:38-43). Asserts Construct/Clear'd
// (count @ +0x20 != -1, baked CgsArray.h:538, 'Array used before Construct/Clear was called'),
// bounds-checks (baked CgsArray.h:539, 'Array index out of bounds'), returns (index<<3)+base ==
// &maElements[index]. Called by NearMissData<4,8>::IsRecentNearMiss / ::IsRecentContacted.
template BrnWorld::VehicleTimePair&
Array<BrnWorld::VehicleTimePair, 4>::operator[](u32);

// Array<BrnWorld::VehicleTimePair, 4>::operator[] const @ 0x822B03F8
// Thin explicit instantiation (generic body inline in CgsArray.h:44-49). Const overload; baked
// CgsArray.h:556/557. count @ +0x20 == 4*8 => N=4, stride 8. Called by NearMissData<4,8>::
// UpdateTimers / ::RememberNearMiss / ::AddContacted.
template const BrnWorld::VehicleTimePair&
Array<BrnWorld::VehicleTimePair, 4>::operator[](u32) const;

#include "GameShared/GameClasses/Containers/CgsArray.h"   // Array<T,N>::Append/Erase (inline generic)
#include "GameSource/World/EntityModules/RaceCarEntityModule/NearMisses/BrnNearMissData.h" // BrnWorld::VehicleTimePair (8-byte element)

// Array<BrnWorld::VehicleTimePair, 8>::Append @ 0x822AEF00
// Array<BrnWorld::VehicleTimePair, 8>::Erase  @ 0x822AF028
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic Array<T,N>::Append / Erase bodies
// are already committed inline in CgsArray.h; this TU is the thin explicit instantiation only
// (do NOT re-define the generic). Both X360 bodies match the generic store-for-store and are
// the ,8 sibling of the committed ,4 and ,7 instantiations, differing only in the capacity (8)
// and the count-word offset (0x40 == 8 * sizeof(VehicleTimePair)):
//
// Append (0x822AEF00): asserts the array was Construct/Clear'd (miCount @ +0x40 != the -1
//   sentinel, CgsArray.h:225) then asserts there is room (unsigned miCount >= 8 streams
//   "Array container out of space, Length:.. Capacity:..", CgsArray.h:226). Both collapse to
//   the house CGS_ASSERT. It writes the 8-byte VehicleTimePair at &maElements[miCount]
//   (slwi r11,miCount,3; add base; stw id@0; stw time@4) and increments miCount. The count
//   word sits at byte 0x40 == 8 * sizeof(VehicleTimePair), confirming the inline maElements[8]
//   buffer end and sizeof(VehicleTimePair) == 8.
//
// Erase (0x822AF028): asserts Construct/Clear'd (CgsArray.h:380) and luIndex < miCount
//   ("Trying to erase an unused element", CgsArray.h:381), decrements miCount, then shifts the
//   tail down one slot (order-preserving): for each slot from luIndex up to the new miCount it
//   copies maElements[i+1] (two 4-byte loads/stores at an 8-byte stride) into maElements[i].
//
// Append is called by NearMissData<4,8>::AddCrashed / AddChecked; Erase by
// NearMissData<4,8>::UpdateTimers / AddCrashed / AddChecked.
template void Array<BrnWorld::VehicleTimePair, 8>::Append(const BrnWorld::VehicleTimePair&);
template void Array<BrnWorld::VehicleTimePair, 8>::Erase(u32);

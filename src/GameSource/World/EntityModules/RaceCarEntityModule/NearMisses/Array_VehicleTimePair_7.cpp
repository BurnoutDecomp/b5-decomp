#include "GameShared/GameClasses/Containers/CgsArray.h"   // Array<T,N>::Append/Erase (inline generic)
#include "GameSource/World/EntityModules/RaceCarEntityModule/NearMisses/BrnNearMissData.h" // BrnWorld::VehicleTimePair (8-byte element)

// Array<BrnWorld::VehicleTimePair, 7>::Append @ 0x822AF2C0
// Array<BrnWorld::VehicleTimePair, 7>::Erase  @ 0x822AF3E8
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic Array<T,N>::Append / Erase bodies
// are already committed inline in CgsArray.h; this TU is the thin explicit instantiation only
// (do NOT re-define the generic). Both X360 bodies match the generic store-for-store and are
// identical to the ,4 instantiation except for the capacity (7) and the count-word offset:
//
// Append (0x822AF2C0): asserts the array was Construct/Clear'd (miCount @ +0x38 != the -1
//   sentinel, CgsArray.h:225) then asserts there is room (unsigned miCount >= 7, CgsArray.h:226).
//   It writes the 8-byte VehicleTimePair at &maElements[miCount] (slwi r11,miCount,3; add base;
//   stw id@0; stw time@4) and increments miCount. The count word sits at byte 0x38 == 7 *
//   sizeof(VehicleTimePair), confirming the inline maElements[7] buffer end and
//   sizeof(VehicleTimePair) == 8.
//
// Erase (0x822AF3E8): asserts Construct/Clear'd (CgsArray.h:380) and luIndex < miCount
//   (CgsArray.h:381), decrements miCount, then shifts the tail down one slot (order-preserving).
//
// Append is called by NearMissData<4,7>::AddCrashed / AddTakenDown; Erase by
// NearMissData<4,7>::UpdateTimers / AddCrashed / AddTakenDown.
template void Array<BrnWorld::VehicleTimePair, 7>::Append(const BrnWorld::VehicleTimePair&);
template void Array<BrnWorld::VehicleTimePair, 7>::Erase(u32);

// Array<BrnTraffic::FiredKillZoneInfo, 8>::Append @ 0x8270B548
// Array<BrnTraffic::FiredKillZoneInfo, 8>::Erase  @ 0x8270B670
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic Array<T,N>::Append / ::Erase bodies
// are already committed inline in CgsArray.h; this TU is the thin explicit instantiation only
// (do NOT re-define the generic). Both X360 bodies match the generic store-for-store:
//
//   ::Append  asserts the array was Construct/Clear'd (count @ +0x80 != the -1 sentinel),
//     asserts there is room (unsigned count >= 8 streams "Array container out of space"),
//     then copies the 16-byte FiredKillZoneInfo as two qwords (`ld/std` x2) at &maElements[count]
//     via a 16-byte stride (`slwi r11,r11,4`), and increments the count word.
//   ::Erase   asserts constructed + index < count ("Trying to erase an unused element"), drops
//     the count, then order-preservingly shifts each later element down one slot via a 16-byte
//     two-qword copy -- exactly the generic Erase loop.
//
// The live-element count sits at byte +0x80 == 8 * sizeof(FiredKillZoneInfo), confirming
// sizeof(FiredKillZoneInfo) == 16.

#include "GameShared/GameClasses/Containers/CgsArray.h"  // Array<T,N>::Append / ::Erase (inline generic)
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h" // FiredKillZoneInfo (16-byte element)

template void Array<BrnTraffic::FiredKillZoneInfo, 8>::Append(const BrnTraffic::FiredKillZoneInfo&);
template void Array<BrnTraffic::FiredKillZoneInfo, 8>::Erase(u32);

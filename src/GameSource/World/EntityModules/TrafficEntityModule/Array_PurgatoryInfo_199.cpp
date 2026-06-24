// Array<BrnTraffic::PurgatoryInfo, 199>::Append @ 0x8270A8D8  (TrafficEntityModule::StaticVehicles_RemoveDeadParam)
// Array<BrnTraffic::PurgatoryInfo, 199>::Erase  @ 0x8270AA00  (TrafficEntityModule::StaticVehicles_UpdatePurgatory)
// Array<BrnTraffic::PurgatoryInfo, 199>::GetI   @ 0x82753108  (BrnTraffic::Logger::HashState)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic Array<T,N> bodies are already committed
// inline in CgsArray.h; this TU is the thin explicit instantiation only (the X360 emits one
// out-of-line copy per using-TU -- do NOT re-define the generic). All three X360 bodies match the
// generic store-for-store:
//
//   ::Append  asserts the array was Construct/Clear'd (count word @ +0x31C != the -1 sentinel,
//     CgsArray.h:225) + room (unsigned `cmplwi count,0xC7 / blt` == count >= 199 streams "Array
//     container out of space, ... Capacity: 199", CgsArray.h:226), then copies the 4-byte
//     PurgatoryInfo as two halfwords (`lhz/sth` muIndex@+0, `lhz/sth` muDecisionFramesLeft@+2) at
//     &maElements[count] via a 4-byte stride (`slwi r11,r11,2`), and increments the count word.
//   ::Erase   asserts constructed (CgsArray.h:380) + index < count ("Trying to erase an unused
//     element", CgsArray.h:381), drops the count, then order-preservingly shifts each later element
//     down one slot via a two-halfword copy loop -- exactly the generic Erase loop.
//   ::GetI    is the non-const checked accessor: asserts constructed (CgsArray.h:538) + index <
//     count ("Array index out of bounds", CgsArray.h:539) then returns 4*index + base
//     (`slwi r,a2,2; add` -> 4 * a2 + a1).
//
// The live-count word sits at byte +0x31C == 796 == 199 * sizeof(PurgatoryInfo), confirming
// sizeof(PurgatoryInfo) == 4 (muIndex u16 + muDecisionFramesLeft u16 -- the same 4-byte POD as the
// ,1 / ,400 instantiations). The DWARF spells the type CgsContainers::Array<BrnTraffic::
// PurgatoryInfo,199u>; spelled unqualified to match the committed Array<T,N> convention.

#include "GameShared/GameClasses/Containers/CgsArray.h"  // Array<T,N>::Append / ::Erase / ::operator[] (inline generic)
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h" // PurgatoryInfo (4-byte element)

template void Array<BrnTraffic::PurgatoryInfo, 199>::Append(const BrnTraffic::PurgatoryInfo&);
template void Array<BrnTraffic::PurgatoryInfo, 199>::Erase(u32);
template BrnTraffic::PurgatoryInfo& Array<BrnTraffic::PurgatoryInfo, 199>::GetItem(u32);

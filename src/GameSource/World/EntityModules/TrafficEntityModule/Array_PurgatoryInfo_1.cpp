// Array<BrnTraffic::PurgatoryInfo, 1>::Append  @ 0x8270AAC0  (TrafficEntityModule::KillDyingVehicleEntity)
// Array<BrnTraffic::PurgatoryInfo, 1>::Erase   @ 0x8270ABE8  (TrafficEntityModule::UpdateTrailers_UpdatePurgatory)
// Array<BrnTraffic::PurgatoryInfo, 1>::GetItem @ 0x8270CA28  (TrafficEntityModule::UpdateTrailers_UpdatePurgatory)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic Array<T,N> bodies (Append / Erase /
// GetItem) are already committed inline in CgsArray.h; this TU is the thin explicit
// instantiation only (the X360 emits one out-of-line copy per using-TU). All three X360
// bodies match the generic store-for-store:
//
//   ::Append  asserts the array was Construct/Clear'd (count word @ +0x4 != the -1 sentinel,
//     CgsArray.h:225), then asserts there is room (unsigned count < 1 -- the X360 `cmplwi 1;
//     blt` fires "Array container out of space" once count>=1, CgsArray.h:226), copies the
//     4-byte PurgatoryInfo as two halfwords (`sth muIndex@+0`, `sth muDecisionFramesLeft@+2`)
//     at &maElements[count] (4-byte stride `slwi r11,r11,2`), then increments the count word.
//   ::Erase   asserts constructed (CgsArray.h:380) + index < count ("Trying to erase an unused
//     element", CgsArray.h:381), drops the count, then order-preservingly shifts each later
//     element down one slot via a two-halfword copy -- exactly the generic Erase loop.
//   ::GetItem asserts constructed (CgsArray.h:556) + index < count ("Array index out of bounds",
//     CgsArray.h:557) then returns 4*index + base (`slwi r11,r28,2; add`) -- the generic checked
//     accessor (routed through operator[]).
//
// The live-count word sits at byte +0x4 == 1 * sizeof(PurgatoryInfo), confirming
// sizeof(PurgatoryInfo) == 4. The DWARF spells the type CgsContainers::Array<BrnTraffic::
// PurgatoryInfo,1u>; spelled unqualified to match the committed Array<T,N> convention.
// PurgatoryInfo is a 4-byte POD (implicit memberwise copy-assign == the two-halfword block).

#include "GameShared/GameClasses/Containers/CgsArray.h"  // Array<T,N>::Append / ::Erase / ::GetItem (inline generic)
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h" // PurgatoryInfo (4-byte element)

template void Array<BrnTraffic::PurgatoryInfo, 1>::Append(const BrnTraffic::PurgatoryInfo&);
template void Array<BrnTraffic::PurgatoryInfo, 1>::Erase(u32);
template BrnTraffic::PurgatoryInfo& Array<BrnTraffic::PurgatoryInfo, 1>::GetItem(u32);

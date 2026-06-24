// Array<BrnTraffic::HullChangeInfo, 400>::Append    @ 0x8270ACA8  (TrafficEntityModule::AddPredictedHullChange)
// Array<BrnTraffic::HullChangeInfo, 400>::EraseFast  @ 0x8270ADD0  (TrafficEntityModule::UpdateRaceCarHulls / ::AddPredictedHullChange)
// Array<BrnTraffic::HullChangeInfo, 400>::GetItem    @ 0x8270CE00  (TrafficEntityModule::DEBUGDumpHullPredictions / ::UpdateRaceCarHulls / ::AddPredictedHullChange)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic Array<T,N> bodies are already committed
// inline in CgsArray.h; this TU is the thin explicit instantiation only (the X360 emits one
// out-of-line copy per using-TU -- do NOT re-define the generic). All three X360 bodies match the
// generic store-for-store:
//
//   ::Append    asserts the array was Construct/Clear'd (count word @ +0xC80 != the -1 sentinel,
//     CgsArray.h:225) + room (unsigned `cmplwi count,0x190 / blt` == count >= 400 streams "Array
//     container out of space, ... Capacity: 400", CgsArray.h:226), then copies the 8-byte
//     HullChangeInfo as two dwords (`lwz/stw` @+0, `lwz/stw` @+4) at &maElements[count] via an
//     8-byte stride (`slwi r11,r11,3`), and increments the count word.
//   ::EraseFast asserts constructed (CgsArray.h:405) + index < count ("Trying to erase an unused
//     element", CgsArray.h:406), then overwrites maElements[index] with the LAST live element
//     (`slwi r,count,3; addi -8` -> &maElements[count-1]) via a two-dword copy and drops the
//     count -- exactly the generic EraseFast (unordered, self-copy-when-last is intentional).
//   ::GetItem   is the const/non-const checked accessor: asserts constructed (CgsArray.h:556) +
//     index < count ("Array index out of bounds", CgsArray.h:557) then returns 8*index + base.
//
// The live-count word sits at byte +0xC80 == 3200 == 400 * sizeof(HullChangeInfo), confirming
// sizeof(HullChangeInfo) == 8. The DWARF spells the type CgsContainers::Array<BrnTraffic::
// HullChangeInfo,400u>; spelled unqualified to match the committed Array<T,N> convention.

#include "GameShared/GameClasses/Containers/CgsArray.h"  // Array<T,N>::Append / ::EraseFast / ::operator[] (inline generic)
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h" // HullChangeInfo (8-byte element)

template void Array<BrnTraffic::HullChangeInfo, 400>::Append(const BrnTraffic::HullChangeInfo&);
template void Array<BrnTraffic::HullChangeInfo, 400>::EraseFast(u32);
template BrnTraffic::HullChangeInfo& Array<BrnTraffic::HullChangeInfo, 400>::GetItem(u32);

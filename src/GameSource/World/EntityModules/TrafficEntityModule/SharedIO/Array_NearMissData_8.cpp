#include "GameShared/GameClasses/Containers/CgsArray.h"    // Array<T,N>::Append / ::IsFull (inline generic)
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficToRaceCarInterface.h" // NearMissData (8-byte element)

// Array<BrnTraffic::BrnTrafficIO::NearMissData, 8>::Append @ 0x82709D28
// Array<BrnTraffic::BrnTrafficIO::NearMissData, 8>::IsFull @ 0x8271A280
// (both reached from BrnTraffic::TrafficEntityModule::ProcessNearbyTrafficSceneQueryResults)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic Array<T,N>::Append / ::IsFull bodies
// are already committed inline in CgsArray.h; this TU is the thin explicit instantiation only
// (the X360 emits one out-of-line copy per using-TU -- do NOT re-define the generic). Both X360
// bodies match the generic store-for-store:
//
//   ::Append  asserts the array was Construct/Clear'd (count @ +0x40 != the -1 sentinel,
//     CgsArray.h:225) then asserts there is room (unsigned `cmplwi count,8 / blt` ==
//     count >= 8 streams "Array container out of space", CgsArray.h:226), copies the 8-byte
//     NearMissData as two dwords (`stw r10,0(r11); stw r10,4(r11)`) at &maElements[count] via an
//     8-byte stride (`slwi r11,r11,3`), then increments the count word.
//   ::IsFull  asserts constructed (count @ +0x40 != -1, CgsArray.h:336) then returns
//     `count == 8` (the X360 computes it branchlessly: `addi count,-8; cntlzw; extrwi ...,1,26`,
//     i.e. 1 iff count-8 == 0 -- exactly the generic `(u32)miCount == N`).
//
// The live-element count sits at byte offset 0x40 == 8 * sizeof(NearMissData) == the inline
// maElements[8] buffer end, confirming sizeof(NearMissData) == 8 (muCarId + mfDistance), the same
// 8-byte element as the ,16 instantiation in Array_NearMissData_16.cpp.
template void Array<BrnTraffic::BrnTrafficIO::NearMissData, 8>::Append(const BrnTraffic::BrnTrafficIO::NearMissData&);
template bool Array<BrnTraffic::BrnTrafficIO::NearMissData, 8>::IsFull() const;

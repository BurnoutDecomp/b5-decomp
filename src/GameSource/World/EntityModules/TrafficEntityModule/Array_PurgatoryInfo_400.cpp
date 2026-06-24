// Array<BrnTraffic::PurgatoryInfo, 400>::Erase   @ 0x8270A770  (TrafficEntityModule::UpdateParams_UpdatePurgatoryList)
// Array<BrnTraffic::PurgatoryInfo, 400>::GetI    @ 0x82753000  (BrnTraffic::Logger::HashState)
// Array<BrnTraffic::PurgatoryInfo, 400>::GetItem @ 0x8270C6C8  (TrafficEntityModule::UpdateParams_UpdatePurgatoryList)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic Array<T,N> bodies are already
// committed inline in CgsArray.h; this TU is the thin explicit instantiation only (the X360
// emits one out-of-line copy per using-TU). All three X360 bodies match the generic
// store-for-store:
//
//   ::Erase asserts the array was Construct/Clear'd (count word @ +0x640 != the -1 sentinel,
//     CgsArray.h:380) + index < count ("Trying to erase an unused element", CgsArray.h:381),
//     drops the count, then order-preservingly shifts each later element down one slot via a
//     two-halfword copy -- exactly the generic Erase loop.
//   ::GetI  is the non-const checked accessor: asserts constructed (CgsArray.h:538) + index <
//     count ("Array index out of bounds", CgsArray.h:539) then returns 4*index + base
//     (`slwi r11,r28,2; add`).
//   ::GetItem is the const checked accessor: same body at CgsArray.h:556/557, returns
//     4*index + base. Both are generic checked accessors routed through operator[].
//
// The live-count word sits at byte +0x640 == 400 * sizeof(PurgatoryInfo), confirming
// sizeof(PurgatoryInfo) == 4 (same 4-byte POD as the ,1 / ,200 instantiations). The DWARF
// spells the type CgsContainers::Array<BrnTraffic::PurgatoryInfo,400u>; spelled unqualified
// to match the committed Array<T,N> convention. GetI(non-const) and GetItem(const) are
// distinct member instantiations (538/539 vs 556/557 in the generic body).

#include "GameShared/GameClasses/Containers/CgsArray.h"  // Array<T,N>::Erase / ::operator[] / ::GetItem (inline generic)
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h" // PurgatoryInfo (4-byte element)

template void Array<BrnTraffic::PurgatoryInfo, 400>::Erase(u32);
template BrnTraffic::PurgatoryInfo&       Array<BrnTraffic::PurgatoryInfo, 400>::GetItem(u32);
template const BrnTraffic::PurgatoryInfo& Array<BrnTraffic::PurgatoryInfo, 400>::GetItem(u32) const;

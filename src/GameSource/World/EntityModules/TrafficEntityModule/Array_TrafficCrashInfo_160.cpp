// Array<BrnTraffic::TrafficCrashInfo, 160>::Erase           @ 0x8270B230
// Array<BrnTraffic::TrafficCrashInfo, 160>::operator[] const (GetI) @ 0x8270CF08
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic Array<T,N>::Erase / indexed-accessor
// bodies are already committed inline in CgsArray.h; this TU is the thin explicit instantiation
// only (do NOT re-define the generic). Both X360 bodies match the generic store-for-store:
//
//   ::Erase  asserts the array was Construct/Clear'd (count @ +0xA00 != the -1 sentinel),
//     asserts index < count ("Trying to erase an unused element"), drops the count, then
//     order-preservingly shifts each later element down one slot via a 16-byte four-dword copy
//     (`lwz/stw` x4 at a 16-byte stride) -- exactly the generic Erase loop.
//   GetI (the const indexed accessor) asserts constructed + index < count, streaming
//     "Array index out of bounds. Index: <i>, length: <n>", then returns 16*index + base
//     (== &maElements[index]). This is the generic checked const operator[].
//
// The live-element count sits at byte +0xA00 == 160 * sizeof(TrafficCrashInfo), confirming
// sizeof(TrafficCrashInfo) == 16; the indexed accessor's 16*index arithmetic confirms it again.

#include "GameShared/GameClasses/Containers/CgsArray.h"  // Array<T,N>::Erase / operator[] (inline generic)
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h" // TrafficCrashInfo (16-byte element)

template void Array<BrnTraffic::TrafficCrashInfo, 160>::Erase(u32);
template const BrnTraffic::TrafficCrashInfo& Array<BrnTraffic::TrafficCrashInfo, 160>::operator[](u32) const;

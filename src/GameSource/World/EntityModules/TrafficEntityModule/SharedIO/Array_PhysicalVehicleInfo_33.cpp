#include "GameShared/GameClasses/Containers/CgsArray.h"    // Array<T,N>::Append (inline generic)
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficPhysicalVehicleInfo.h" // PhysicalVehicleInfo (48-byte element)

// Array<BrnTraffic::PhysicalVehicleInfo, 33>::Append @ 0x8270BA88
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic Array<T,N>::Append body is
// already committed inline in CgsArray.h; this TU is the thin explicit instantiation only
// (do NOT re-define the generic). The X360 body matches the generic store-for-store:
//   - asserts the array was Construct/Clear'd (count @ +0x630 != the -1 sentinel)         (CgsArray.h:225)
//   - asserts there is room (unsigned count >= 33 streams "Array container out of space,
//     Length: <n>, Capacity: 33")                                                          (CgsArray.h:226)
//   - copies the 48-byte PhysicalVehicleInfo (six 8-byte `std`s) to &maElements[count]
//     via the 48-byte element stride (slwi/add index math), then increments the count word.
// The live-element count sits at byte offset 0x630 == 33 * sizeof(PhysicalVehicleInfo)
// == the inline maElements[33] buffer end, confirming sizeof(PhysicalVehicleInfo) == 48.
template void Array<BrnTraffic::PhysicalVehicleInfo, 33>::Append(const BrnTraffic::PhysicalVehicleInfo&);

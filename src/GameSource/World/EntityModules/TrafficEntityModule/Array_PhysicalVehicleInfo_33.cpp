// Array<BrnTraffic::PhysicalVehicleInfo, 33>::operator[] (non-const) @ 0x8270BBC8
//   (BrnTrafficEntityModule::UpdateParam_CheckIfNeedToSlow)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic Array<T,N>::operator[] body is
// already committed inline in CgsArray.h; this TU is the thin explicit instantiation only
// (do NOT re-define the generic). The X360 body matches the generic store-for-store:
//
//   operator[]  asserts the array was Construct/Clear'd (count word @ +0x630 != the -1
//     sentinel, "Array used before Construct/Clear was called", CgsArray.h:538), then asserts
//     index < count ("Array index out of bounds. Index: <i>, length: <n>", CgsArray.h:539),
//     then returns 48*index + base (`mulli r,index,48` == &maElements[index]). This is the
//     generic checked NON-const operator[] (the X360 asserts at CgsArray.h:538/539, the
//     non-const line pair).
//
// The live-element count word sits at byte +0x630 == 33 * sizeof(PhysicalVehicleInfo),
// confirming sizeof(PhysicalVehicleInfo) == 48; the accessor's 48*index arithmetic confirms it
// again. PhysicalVehicleInfo is a 48-byte SIMD POD (Vector3Plus + Vector3 + Vector3) homed in
// BrnTrafficPhysicalVehicleInfo.h (its own committed home; do NOT redefine it). The capacity
// 33 == KU_MAX_PHYSICAL_VEHICLES_TO_CACHE.
//
// The DWARF spells the type CgsContainers::Array<BrnTraffic::PhysicalVehicleInfo,33u>; spelled
// unqualified Array<> to match the committed Array<T,N> convention.

#include "GameShared/GameClasses/Containers/CgsArray.h"  // Array<T,N>::operator[] (inline generic)
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficPhysicalVehicleInfo.h" // PhysicalVehicleInfo (48-byte element, canonical home)

template BrnTraffic::PhysicalVehicleInfo& Array<BrnTraffic::PhysicalVehicleInfo, 33>::operator[](u32);

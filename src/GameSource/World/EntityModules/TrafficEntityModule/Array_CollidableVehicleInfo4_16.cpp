// Array<BrnTraffic::CollidableVehicleInfo4, 16>::operator[] const @ 0x8270D260
//   (BrnTrafficEntityModule::Avoidance_GetBestVehicleDirection / ::Reset /
//    DebugComponent::DrawAvoidance -- all read mCachedCollidableList[index])
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic Array<T,N>::operator[] const body is
// already committed inline in CgsArray.h; this TU is the thin explicit instantiation only
// (do NOT re-define the generic). The X360 body matches the generic store-for-store:
//
//   operator[] const  asserts the array was Construct/Clear'd (count word @ +0x800 != the -1
//     sentinel, "Array used before Construct/Clear was called", CgsArray.h:556), then asserts
//     index < count ("Array index out of bounds. Index: <i>, length: <n>", CgsArray.h:557),
//     then returns (index<<7) + base (`slwi r,index,7` == 128*index == &maElements[index]).
//     This is the generic checked CONST operator[] (the X360 asserts at CgsArray.h:556/557,
//     the const line pair).
//
// The live-element count word sits at byte +0x800 == 16 * sizeof(CollidableVehicleInfo4),
// confirming sizeof(CollidableVehicleInfo4) == 128; the accessor's 128*index arithmetic
// confirms it again. CollidableVehicleInfo4 is a 128-byte SIMD struct-of-arrays packet
// (eight 16-byte Vector4 lanes, four vehicles per record -> 16*4 == 64 ==
// KU_MAX_COLLIDABLE_CACHED_TRAFFIC) homed in BrnTrafficEntityModule.h.
//
// The DWARF spells the type CgsContainers::Array<BrnTraffic::CollidableVehicleInfo4,16u>;
// spelled unqualified Array<> to match the committed Array<T,N> convention.

#include "GameShared/GameClasses/Containers/CgsArray.h"  // Array<T,N>::operator[] const (inline generic)
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h" // CollidableVehicleInfo4 (128-byte element)

template const BrnTraffic::CollidableVehicleInfo4& Array<BrnTraffic::CollidableVehicleInfo4, 16>::operator[](u32) const;

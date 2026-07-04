// Array<BrnTraffic::CrashingThingData, 168>::operator[] (non-const) @ 0x8270BE68
//   (BrnTrafficEntityModule::UpdateParams_TryStartSympatheticCrashing / _TryAvoidCrashing)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic Array<T,N>::operator[] body is
// already committed inline in CgsArray.h; this TU is the thin explicit instantiation only
// (do NOT re-define the generic). The X360 body matches the generic store-for-store:
//
//   operator[]  asserts the array was Construct/Clear'd (count word @ +0x1500 != the -1
//     sentinel, "Array used before Construct/Clear was called", CgsArray.h:538), then asserts
//     index < count ("Array index out of bounds. Index: <i>, length: <n>", CgsArray.h:539),
//     then returns 32*index + base (`slwi r,index,5` == &maElements[index]). This is the
//     generic checked NON-const operator[] (the X360 asserts at CgsArray.h:538/539, the
//     non-const line pair).
//
// The live-element count word sits at byte +0x1500 == 168 * sizeof(CrashingThingData),
// confirming sizeof(CrashingThingData) == 32; the accessor's 32*index arithmetic confirms it
// again. CrashingThingData is a 32-byte POD (Vector3 + EntityId + bool, padded to the
// Vector3's 16-byte alignment) homed in BrnTrafficEntityModule.h.
//
// The DWARF spells the type CgsContainers::Array<BrnTraffic::CrashingThingData,168u>; spelled
// unqualified Array<> to match the committed Array<T,N> convention.

#include "GameShared/GameClasses/Containers/CgsArray.h"  // Array<T,N>::operator[] (inline generic)
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h" // CrashingThingData (32-byte element)

template BrnTraffic::CrashingThingData& Array<BrnTraffic::CrashingThingData, 168>::operator[](u32);

// Array<BrnTraffic::CrashingThingData, 168>::Append @ 0x8270BCD8
//   (BrnTraffic::TrafficEntityModule::UpdateParams_BuildListOfCrashingThings)
//
// The generic Array<T,N>::Append body is inline in CgsArray.h; this is the thin explicit
// instantiation. The X360 body (@0x8270BCD8) matches store-for-store: assert constructed
// (count word @ +0x1500 != -1 sentinel, "Array used before Construct/Clear was called",
// CgsArray.h:225), assert count < 168 (X360 streamed the dynamic out-of-space message,
// CgsArray.h:226; cmplwi 0xA8 == N==168), then copies the 32-byte element to &maElements[count]
// (`slwi count,5` == count*0x20 stride) and post-increments count. The +0x1500 count offset ==
// 168*0x20 reconfirms sizeof(CrashingThingData) == 32.
template void Array<BrnTraffic::CrashingThingData, 168>::Append(const BrnTraffic::CrashingThingData&);

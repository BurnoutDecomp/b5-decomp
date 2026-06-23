#include "GameShared/GameClasses/Containers/CgsArray.h"    // Array<T,N>::Append (inline generic)
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficToRaceCarInterface.h" // NearMissData (8-byte element)

// Array<BrnTraffic::BrnTrafficIO::NearMissData, 16>::Append @ 0x82709C00
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic Array<T,N>::Append body is already
// committed inline in CgsArray.h; this TU is the thin explicit instantiation only (do NOT
// re-define the generic). The X360 body matches the generic store-for-store:
//   - asserts the array was Construct/Clear'd (count @ +0x80 != the -1 sentinel)            (CgsArray.h:225)
//   - asserts there is room (unsigned count >= 16 streams "Array container out of space")    (CgsArray.h:226)
//   - writes 2 dwords (the 8-byte NearMissData: muCarId + mfDistance) at &maElements[count]
//     via an 8-byte stride (slwi r11,r11,3), then increments the count word.
// The live-element count sits at byte offset 0x80 == 16 * sizeof(NearMissData) == the inline
// maElements[16] buffer end, confirming sizeof(NearMissData) == 8.
template void Array<BrnTraffic::BrnTrafficIO::NearMissData, 16>::Append(const BrnTraffic::BrnTrafficIO::NearMissData&);

// Array<unsigned char, 16>::Append              @ 0x8270B970
// Array<unsigned char, 16>::Contains            @ 0x8271B6F8
// Array<unsigned char, 16>::FindFirstInstanceOf @ 0x8270D680
// Array<unsigned char, 16>::GetItem             @ 0x8270D710
//   (all in BrnTraffic::TrafficEntityModule::AddVehiclesToTargetList; file name kept from the
//   earlier Array<char,16> spelling)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic Array<T,N> bodies (Append /
// Contains / FindFirstInstanceOf / the checked GetItem accessor) are already committed inline
// in CgsArray.h; this TU is the thin explicit instantiation only (do NOT re-define the
// generic). Mirrors the sibling Array_short_9.cpp in this directory (same caller).
//
// Element type: DWARF _compile/BrnTrafficUnity.cpp:15167..15206 instantiates
// CgsContainers::Array<uint8_t,16u> for AddVehiclesToTargetList's local lAssetIds
// (BrnTrafficEntityModule.cpp:8172). ARTIST compares unsigned (FindFirstInstanceOf 0x8270D680:
// lbzx, then cmplw, no extsb); the IDA names "char,16>::..." are front-truncated
// "unsigned char,16>::...".
//
// Ledger from BURNOUT_X360_ARTIST.XEX: the live-count word sits at byte +0x10 == 16 * 1,
// confirming N == 16 and a 1-byte element (lbz/stb/stbx/lbzx element access, 1-byte stride;
// maElements at this+0, so Append `stbx r10,r29,r11`, GetItem returns `add index,this` ==
// &maElements[index], FindFirstInstanceOf `lbzx r9,index,this`). Out-of-space compare
// `cmplwi r11,0x10` and bounds `cmplw index,count`. GetItem is the non-const T& overload
// (asserts CgsArray.h:556/557 at 0x8270D740/0x8270D7F4).

#include "GameShared/GameClasses/Containers/CgsArray.h"  // Array<T,N> (inline generic)

template void CgsContainers::Array<u8, 16>::Append(const u8&);
template u32  CgsContainers::Array<u8, 16>::FindFirstInstanceOf(const u8&) const;
template u8&  CgsContainers::Array<u8, 16>::GetItem(u32);
template bool CgsContainers::Array<u8, 16>::Contains(const u8&) const;

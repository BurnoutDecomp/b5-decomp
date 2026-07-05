// Array<char, 16>::Append              @ 0x8270B970
// Array<char, 16>::Contains            @ 0x8271B6F8
// Array<char, 16>::FindFirstInstanceOf @ 0x8270D680
// Array<char, 16>::GetItem             @ 0x8270D710
//   (all in BrnTraffic::TrafficEntityModule::AddVehiclesToTargetList)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic Array<T,N> bodies (Append /
// Contains / FindFirstInstanceOf / the checked GetItem accessor) are already committed inline
// in CgsArray.h; this TU is the thin explicit instantiation only (do NOT re-define the
// generic). Mirrors the sibling Array_short_9.cpp in this directory (same caller).
//
// Ledger from BURNOUT_X360_ARTIST.XEX: the live-count word sits at byte +0x10 == 16 * 1,
// confirming N == 16 and sizeof(char) == 1 (lbz/stb/stbx/lbzx element access, 1-byte stride;
// maElements at this+0, so Append `stbx r10,r29,r11`, GetItem returns `add index,this` ==
// &maElements[index], FindFirstInstanceOf `lbzx r9,index,this`). Out-of-space compare
// `cmplwi r11,0x10` and bounds `cmplw index,count`. GetItem is instantiated as the non-const
// T& overload to match the committed convention (see Array_short_9.cpp). `char` is a
// primitive -- no element home include.

#include "GameShared/GameClasses/Containers/CgsArray.h"  // Array<T,N> (inline generic)

template void  Array<char, 16>::Append(const char&);
template s32   Array<char, 16>::FindFirstInstanceOf(const char&) const;
template char& Array<char, 16>::GetItem(u32);
template bool  Array<char, 16>::Contains(const char&) const;

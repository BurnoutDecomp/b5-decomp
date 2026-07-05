// Array<u16, 64>::Append  @ 0x8270B850  (BrnTraffic::TrafficEntityModule::NukeTrafficJams)
// Array<u16, 64>::GetItem @ 0x8270D578  (BrnTraffic::TrafficEntityModule::NukeTrafficJams)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic Array<T,N> bodies
// (Append / operator[] / GetItem) are already committed inline in CgsArray.h; this TU is
// the thin explicit instantiation only (the X360 emits one out-of-line copy per using-TU).
// The live-count word sits at byte +0x80 == 64 * sizeof(u16), confirming the u16 element and
// N==64 (2-byte stride: lhz/sthx element access, slwi index,1). GetItem is the non-const
// overload returning u16&. The DWARF spells the type CgsContainers::Array<unsigned short,64u>;
// spelled unqualified to match the committed Array<T,N> convention. A primitive u16 element
// needs no element_home include.

#include "GameShared/GameClasses/Containers/CgsArray.h"  // Array<T,N>::Append / ::GetItem (inline generic)

template void Array<u16, 64>::Append(const u16&);
template u16& Array<u16, 64>::GetItem(u32);

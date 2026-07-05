// Array<u16, 9>::Append    @ 0x8270AE78  (TrafficEntityModule::UpdateRaceCarHulls / ::Reset)
// Array<u16, 9>::GetLength  @ 0x8270AF98  (TrafficEntityModule::AddVehiclesToTargetList / ::UpdateSerialiser)
// Array<u16, 9>::GetItem    @ 0x8270AFF0  (TrafficEntityModule::AddVehiclesToTargetList / ::UpdateSerialiser / ...)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic Array<T,N> bodies (Append / GetLength /
// GetItem) are already committed inline in CgsArray.h; this TU is the thin explicit instantiation
// only (do NOT re-define the generic). The live-count word sits at byte +0x14 == align4(9 * 2),
// confirming sizeof(u16) == 2 (lhz/sthx element access, 2-byte stride). GetItem is instantiated as
// the non-const T& overload to match the committed Array<T,N>::GetItem convention (see
// Array_PurgatoryInfo_1.cpp). The DWARF spells the type CgsContainers::Array<unsigned short,9u>;
// spelled unqualified to match the committed convention. u16 is a primitive -- no element home.

#include "GameShared/GameClasses/Containers/CgsArray.h"  // Array<T,N>::Append / ::GetLength / ::GetItem (inline generic)

template void Array<u16, 9>::Append(const u16&);
template u32  Array<u16, 9>::GetLength() const;
template u16& Array<u16, 9>::GetItem(u32);

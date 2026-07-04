// GameSource/Replays/Array_PropLoadedZoneRecord_9_operator_index.cpp
//
// BrnReplays::BrnReplayArray<PropLoadedZoneRecord, 9>::operator[] (non-const) @ 0x822AA058
//   (BrnReplays::PropSerialiserFrame::RemoveLoadedZone / ::GetZone)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Thin explicit instantiation of the generic
// BrnReplayArray<T,N>::operator[] committed inline in BrnReplayArray.h (do NOT re-define).
// X360 store-for-store: assert index < muLength (muLength is a u8 @ +0x5E8 == 9*168),
// streaming "Array index out of bounds: <i> Length: <n>\n" (BrnReplayArray.h:93) collapsed to
// one CGS_ASSERT, then return `168*index + base` (`mulli r11,index,0xA8; add r29`) ==
// &maElements[index]. The 168*index stride and the 0x5E8 count offset fix sizeof(T)==168
// and N==9.
//
// The 168-byte element is the ALREADY-COMMITTED BrnReplays::PropLoadedZoneRecord
// (BrnReplayPropSerialiserFrame.h: s32 miZoneId + s32 mPad04 + u8 maZeroed[0xA0] = 168 bytes,
// == the 0xA8 stride). The loaded-zone array sits at the PropSerialiserFrame base, so its
// count byte at frame offset 0x5E8 coincides with the committed mbZonesLoaded flag that
// RemoveAllLoadedZones clears -- pinning the element type. Reuse the committed struct; do
// NOT coin a duplicate.

#include "types.hpp"
#include "GameSource/Replays/BrnReplayArray.h"                            // BrnReplayArray<T,N>::operator[] (inline generic)
#include "GameSource/Replays/Serialisers/BrnReplayPropSerialiserFrame.h"  // BrnReplays::PropLoadedZoneRecord (committed)

template BrnReplays::PropLoadedZoneRecord&
BrnReplays::BrnReplayArray<BrnReplays::PropLoadedZoneRecord, 9>::operator[](u8);

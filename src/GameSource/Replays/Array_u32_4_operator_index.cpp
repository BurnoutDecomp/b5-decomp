// GameSource/Replays/Array_u32_4_operator_index.cpp
//
// BrnReplays::BrnReplayArray<u32, 4>::operator[] (non-const) @ 0x822AA250
//   (BrnReplays::PropSerialiserFrame::IsCellActive)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Thin explicit instantiation of the generic
// BrnReplayArray<T,N>::operator[] committed inline in BrnReplayArray.h (do NOT re-define).
// X360 store-for-store: assert index < muLength (muLength is a u8 @ +0x10 == 4*sizeof(u32)),
// streaming "Array index out of bounds: <i> Length: <n>\n" (BrnReplayArray.h:93) collapsed to
// one CGS_ASSERT, then return `4*index + base` (`slwi r11,index,2; add r29`) ==
// &maElements[index]. The 4*index stride and the 0x10 count offset fix sizeof(T)==4 (u32)
// and N==4.

#include "types.hpp"
#include "GameSource/Replays/BrnReplayArray.h"  // BrnReplayArray<T,N>::operator[] (inline generic)

template u32& BrnReplays::BrnReplayArray<u32, 4>::operator[](u8);

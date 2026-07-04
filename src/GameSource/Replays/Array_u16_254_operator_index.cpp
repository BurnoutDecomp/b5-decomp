// GameSource/Replays/Array_u16_254_operator_index.cpp
//
// BrnReplays::BrnReplayArray<u16, 254>::operator[] (non-const) @ 0x822AA638
//   (BrnWorld::PropEntityModule::ReplayUpdatePropsInScene / ::RenderReplayProp)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BrnReplayArray<T,N>::operator[]
// body is committed inline in BrnReplayArray.h; this TU is the thin explicit instantiation
// only (do NOT re-define the generic). The X360 body matches store-for-store: assert
// index < muLength (muLength is a u8 @ +0x1FC == 254*sizeof(u16)), streaming
// "Array index out of bounds: <i> Length: <n>\n" (BrnReplayArray.h:93) which collapses to
// one CGS_ASSERT, then return `2*index + base` (`slwi r11,index,1; add r29`) ==
// &maElements[index]. The 2*index stride and the 0x1FC count offset both fix sizeof(T)==2
// (u16) and N==254.

#include "types.hpp"
#include "GameSource/Replays/BrnReplayArray.h"  // BrnReplayArray<T,N>::operator[] (inline generic)

template u16& BrnReplays::BrnReplayArray<u16, 254>::operator[](u8);

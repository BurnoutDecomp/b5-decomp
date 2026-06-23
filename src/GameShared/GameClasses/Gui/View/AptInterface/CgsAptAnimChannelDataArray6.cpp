// Per-instantiation .cpp for Array<CgsGui::AnimChannelData,6>. The generic Array<T,N> body
// (Append / GetItem / operator[] + siblings) is fully inline in CgsArray.h, so this TU is
// just the explicit member instantiations the X360 emits out-of-line for this element type:
//   Array<AnimChannelData,6>::Append  @ 0x8284D338  (called by AnimData::AddAnimationChannel)
//   Array<AnimChannelData,6>::GetItem @ 0x8284D830  (called by AnimData::GetChannelData)
//
// Byte-parity check against the X360 pseudocode for this instantiation:
//   element stride 24 (sizeof AnimChannelData == 0x18: 6 dwords), capacity 6, miCount @
//   +0x90 (144 == 6*24) -> matches the X360's `result[36]`/`*(a1+144)` count word.
//   Append: unconstructed(-1) + out-of-space(>= 6) asserts (CgsArray.h:225/226), the 6-dword
//   element copy into `&v2[6 * count]` (the `do { *v14++ = *v13++; } while(--v15)` loop, 6
//   iterations), and the post-increment of the count.
//   GetItem: unconstructed(-1) + out-of-bounds(>= count) asserts (CgsArray.h:538/539), then
//   returns the element address `24 * index + this` == &maElements[index].
//
// Spelled unqualified Array<...> to match the committed container convention
// (CgsArrayInt8.cpp / CgsAptAnimChannelDataArray2.cpp); the DWARF spells the type
// CgsContainers::Array<CgsGui::AnimChannelData,6u>.
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAnimData.h"

// Pin the X360-proven element stride: Append @ 0x8284D338 copies 6 dwords into
// `&v2[6 * count]` and GetItem @ 0x8284D830 returns `24 * index + this`, so
// sizeof(AnimChannelData) must be 24 (0x18).
static_assert(sizeof(CgsGui::AnimChannelData) == 24, "AnimChannelData stride 24 (X360 0x8284D338/0x8284D830)");

template void                  Array<CgsGui::AnimChannelData, 6>::Append(const CgsGui::AnimChannelData&);
template CgsGui::AnimChannelData& Array<CgsGui::AnimChannelData, 6>::GetItem(u32);

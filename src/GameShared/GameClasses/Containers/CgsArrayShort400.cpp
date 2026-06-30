// Per-instantiation .cpp for the u16/extent-400 fixed-capacity containers the BrnTraffic
// param allocator + nearest-lane search drive. The IDA-truncated ledger key "short,400>"
// conflates the two distinct generics this single using-TU emits out-of-line, both
// element=unsigned short(u16), extent N=400:
//
//   Array<u16,400>  (CgsArray.h, inline body):
//     Append              @ 0x82752EE0  (TrafficData::FindNearestLaneForPoint)
//     FindFirstInstanceOf @ 0x82753210  (TrafficData::FindNearestLaneForPoint)
//     GetItem             @ 0x827532A8  (TrafficData::FindNearestLaneForPoint)
//   Stack<u16,400>  (CgsStack.h, inline body):
//     Contains            @ 0x8270A830  (TrafficEntityModule::UpdateParams_UpdatePurgatoryList -- the
//                                         body asserts CgsStack.h:132 "Stack used before...", so this is
//                                         Stack::Contains, NOT Array::Contains: linear key-scan over miLength)
//     Pop                 @ 0x8271AA48  (TrafficEntityModule::TryAllocateParamId)
//     Push                @ 0x8271A988  (UpdateParams_UpdatePurgatoryList, Reset)
//     operator[]          @ 0x8270C7D0  (CgsAlgorithms::Shuffle<Stack<u16,400>>, mangled
//                                         ??$Shuffle@GV?$Stack@G$0BJA@@CgsContainers@@@ -- 0BJA=0x190=400)
//
// Element type unsigned short: 2-byte slot stride (`2*idx + base`), count word at +800
// (Array maElements[400]==800B -> miCount @ +0x320; Stack maData[400] -> miLength @ +0x320).
// The X360 Append/Push out-of-space asserts compare against 400/0x190. Bodies header-inline.
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/Containers/CgsStack.h"

template class Array<u16, 400>;

// Stack<u16,400> attests four members for this TU (Contains/Push/Pop/operator[]); force just
// those rather than the whole class (Grow/Construct/etc. are not part of this using-TU).
template bool       CgsContainers::Stack<u16, 400>::Contains(const u16&) const;
template void       CgsContainers::Stack<u16, 400>::Push(const u16&);
template void       CgsContainers::Stack<u16, 400>::Pop();
template u16&       CgsContainers::Stack<u16, 400>::operator[](s32);

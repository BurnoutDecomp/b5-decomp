// Per-instantiation .cpp for the u16/extent-1 fixed-capacity containers the BrnTraffic
// trailer-id allocator drives. The IDA-truncated ledger key "short,1>" conflates the two
// distinct generics this single using-TU (BrnTraffic::TrafficEntityModule) emits out-of-line,
// both element=unsigned short(u16), extent N=1:
//
//   Array<u16,1>  (CgsArray.h, inline body):
//     Contains            @ 0x8271A908  (TrafficEntityModule::UpdateParams)
//     FindFirstInstanceOf @ 0x8270C630  (Array<u16,1>::Contains)
//   Stack<u16,1>  (CgsStack.h, inline body):
//     Peek                @ 0x8271AF38  (TrafficEntityModule::TryAllocateTrailerId)
//     Pop                 @ 0x8271AE88  (TrafficEntityModule::TryAllocateTrailerId)
//     Push                @ 0x8271ADC8  (UpdateTrailers_UpdatePurgatory, Reset)
//     operator[]          @ 0x8270CB30  (CgsAlgorithms::Shuffle<Stack<u16,1>>, mangled
//                                         ??$Shuffle@GV?$Stack@G$00@CgsContainers@@@)
//
// Element type is unsigned short: the X360 Push/Stack ops use a 2-byte slot stride
// (`2*idx + base`, _WORD store of *a2) and the count word sits at +4 (Array, maElements[1]==2B
// padded to 4) / +4 (Stack maData[1]). Both bodies are header-inline; this TU forces emission.
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/Containers/CgsStack.h"

template class Array<u16, 1>;

// Stack<u16,1> carries a declared-only Contains() (its body links from other call sites), so
// force just the four members this TU attests rather than the whole class.
template void                CgsContainers::Stack<u16, 1>::Push(const u16&);
template void                CgsContainers::Stack<u16, 1>::Pop();
template const u16&          CgsContainers::Stack<u16, 1>::Peek() const;
template u16&                CgsContainers::Stack<u16, 1>::operator[](s32);

#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"
#include "GameShared/GameClasses/Containers/CgsRingBuffer.h"

// Explicit-instantiation TU for the trivial 2-byte (u16 / unsigned short) element type.
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the X360 build emits a per-instantiation
// out-of-line body for each template method, modelled here as the generic inline bodies
// in CgsBaseEventQueue.h / CgsRingBuffer.h (one body covers all element types). The
// element type is the builtin u16, so no element-home header is needed.
//
// The ledger class TU "short>" spans two distinct u16-element generics:
//   * CgsModule::BaseEventQueue<unsigned short>  (AddEvent / GetEvent)
//   * CgsContainers::RingBuffer<u16>             (Push -- the ring FIFO used by the
//                                                 StuntModeScoring/CrashModeScoring
//                                                 mRecentPropSet = FixedRingBuffer<u16,64>)
// u16 is a typedef for unsigned short on this target, so these are the same instantiations
// the X360 spells with the literal "unsigned short".

// CgsModule::BaseEventQueue<unsigned short>::AddEvent  @ 0x825A3148
// Appends UNCONDITIONALLY (the two asserts are non-gating tripwires): asserts
// "mpEvents != NULL" (CgsBaseEventQueue.h:312) and "...Reached Max length" (:313),
// then stores *a2 at mpEvents[miLength] (stride 2 == sizeof(u16)), bumps miLength,
// returns true.
template bool CgsModule::BaseEventQueue<unsigned short>::AddEvent(const unsigned short&);

// CgsModule::BaseEventQueue<unsigned short>::GetEvent(s32) const  @ 0x82709A00
// Checked indexed accessor: asserts "mpEvents != NULL" (:272), "liIndex < GetLength()"
// (:274) and "liIndex >= 0" (:275), then returns &mpEvents[liIndex] (X360 `2*a2 + *a1`,
// stride 2 == sizeof(u16)). The DWARF gives the real const T& return.
template const unsigned short& CgsModule::BaseEventQueue<unsigned short>::GetEvent(s32) const;

// CgsContainers::RingBuffer<u16>::Push  @ 0x823190E0
// Circular append: stores *a2 at mpData[miWritePos] (stride 2), advances miWritePos
// (wrapping at miMaxLength) and miLength; when full clamps miLength and advances
// miReadPos modulo miMaxLength, asserting read pos == write pos (CgsRingBuffer.h:137).
template void CgsContainers::RingBuffer<u16>::Push(const u16*);

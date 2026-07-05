// Per-instantiation .cpp for Set<u16, 72>. The generic Set<T,N> body (Insert / Find / Contains /
// GetItem / GetLength + siblings) is fully inline in CgsSet.h, so this TU is just the explicit
// class instantiation (the X360 emits one out-of-line copy of each member per using-TU). The
// five members the X360 emitted out-of-line for this instance (all under BrnTraffic's
// TrafficEntityModule active-hull / kill-zone bookkeeping):
//   Set<u16,72>::GetLength @ 0x8270A570  (UpdateDensity / PostPhysicsUpdate)
//   Set<u16,72>::GetItem   @ 0x8270A5C8  (SpawnShowtimeTraffic)  -- NOTE: X360 bounds-checks the
//                                          live count muLength, not the capacity N; see below
//   Set<u16,72>::Find      @ 0x8270C598  (Contains / SetDifference<72u,72u> / FireKillZone / ...)
//   Set<u16,72>::Contains  @ 0x8271A888  (Insert / KillOutOfAreaTraffic / UpdateEventStarts)
//   Set<u16,72>::Insert    @ 0x8272C768  (SetDifference<72u,72u> / RecalculateActiveHulls)
//
// LAYOUT (X360 element addressing + count word, authoritative):
//   maElements[72]   +0x00  (72 * 2 = 144 bytes; u16 element -- 2-byte stride, lhz/sthx)
//   muLength         +0x90  (count word read/written at *(this+0x90), `lwz 0x90(r31)`)
// matching the slwi-by-1 element addressing (2-byte stride) and the single-halfword element copy
// (Insert: `lhz r10,0(other); slwi r11,muLength,1; sthx r10,r11,this` -- maElements[muLength]=*a2).
// Capacity N == 72 (Insert out-of-space gate `cmplwi muLength,0x48`).
//
// FIDELITY NOTE (GetItem @ 0x8270A5C8): the X360 GetItem bounds-checks luIndex against the LIVE
// COUNT muLength, whereas the committed generic Set<T,N>::GetItem (CgsSet.h:99) checks luIndex < N.
// Returned address + assert message ('Set index out of bounds') are identical; only the assert
// predicate differs. Not forked here -- the generic GetItem is shared with the already-committed
// Set<u16,15> / Set<BrnWorld::PropEntityID,32> instantiations.
//
// Spelled unqualified to match the committed Set<T,N> / Array<T,N> container convention (CgsSet.h);
// the DecFIGS DWARF spells the type CgsContainers::Set<uint16_t,72u>. u16 is a plain primitive
// whose built-in operator== satisfies the equality-based generic Find/Contains.
//
// (The IDA-truncated demangled prefix 'short,72>' also collides with Array<u16,72>::Append
// @ 0x8270CBD8 -- that member carries CgsArray.h asserts and is instantiated in
// CgsArrayUnsignedShort72.cpp, a separate TU.)
#include "GameShared/GameClasses/Containers/CgsSet.h"

template class Set<u16, 72>;

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
// The member TEMPLATE SetDifference<Capacity1,Capacity2> is NOT instantiated by the explicit
// `template class` below (a class instantiation never instantiates nested member templates), so
// the one out-of-line copy the X360 emits for this instance is forced separately:
//   Set<u16,72>::SetDifference<72u,72u> @ 0x8272C810  (BrnTraffic::TrafficEntityModule::RecalculateActiveHulls)
// The asm (this=dest at +0x90 count, r4=lA, r5=lB) walks lA, Clear()s the dest (result[0x90]=0),
// and for each lA element not found in lB inserts it -- exactly the generic CgsSet.h body; the
// binary calls Find(lB,elem) directly because Contains() (which forwards to Find) is inlined.
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

// Force the out-of-line copy of the member template SetDifference<72u,72u> @ 0x8272C810
// (the class instantiation above does not emit nested member templates). Body is the generic
// CgsSet.h SetDifference<Capacity1,Capacity2>. Spelled GLOBAL scope (::Set), matching committed CgsSet.h.
template void Set<u16, 72>::SetDifference<72, 72>(const Set<u16, 72>& lA, const Set<u16, 72>& lB);

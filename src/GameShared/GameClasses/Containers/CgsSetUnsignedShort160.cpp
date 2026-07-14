// Per-instantiation .cpp for Set<u16, 160>. The generic Set<T,N> body (Insert / Erase / Find /
// Contains + the SetDifference<Cap1,Cap2> member template) is fully inline in CgsSet.h, so this
// TU is just the explicit instantiation (the X360 emits one out-of-line copy of each used member
// per using-TU). The one member the X360 emitted out-of-line for this instance:
//   Set<u16,160>::SetDifference<160u,160u> @ 0x827C8608
//     (BrnWorld::CrashModule::HandleNetworkCrashingTraffic)
//
// LAYOUT (X360 element addressing + count word, authoritative from the asm):
//   maElements[160]  +0x00   (160 * 2 = 320 bytes; u16 element -- 2-byte stride, lhz/sthx)
//   muLength         +0x140  (count word read/written at *(this+0x140); `stw r30,0x140(r25)` is
//                             the leading Clear(), `lwz r11,0x140(r28)` reads lA.GetLength())
// matching the generic Set<T,N> layout (maElements[N] @ +0x00, muLength @ +N*sizeof(T)) for N==160.
//
// The asm walks lA (r28) by 2-byte element stride (r27 += 2), and for each element calls
// Find on lB (r24) -- returning -1/KU_INVALID when absent -- then Insert into the result set
// (r25). That is exactly the inline SetDifference: Clear(); keep each lA element lB !Contains
// (Contains folds to Find != KU_INVALID). The four guarded asserts ("Set used before
// Construct/Clear was called" x3 at CgsSet.h lines 227/257/332 + "Set index out of bounds" at
// line 258) are the inlined GetLength / operator[] / Contains constructed-checks and bounds
// check, all faithful to the committed generic bodies.
//
// Spelled unqualified to match the committed Set<T,N> / Array<T,N> container convention
// (CgsSet.h); the DecFIGS DWARF spells the type CgsContainers::Set<uint16_t,160u>. u16 is a plain
// primitive whose built-in operator== satisfies the equality-based generic Find/Contains.
//
// `template class Set<u16,160>` instantiates the non-template members; the member *template*
// SetDifference must be instantiated explicitly (an explicit class instantiation does not reach
// member templates) -- that is the ledger-key function for this TU.
#include "GameShared/GameClasses/Containers/CgsSet.h"

template class Set<u16, 160>;
template void Set<u16, 160>::SetDifference<160u, 160u>(const Set<u16, 160>& lA,
                                                       const Set<u16, 160>& lB);

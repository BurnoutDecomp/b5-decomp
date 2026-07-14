// Per-instantiation .cpp for Set<uint16_t, 15>. The generic Set<T,N> body (Insert / Erase /
// Find / Contains + siblings) is fully inline in CgsSet.h, so this TU is just the explicit
// class instantiation (the X360 emits one out-of-line copy of each member per using-TU).
// The four members the X360 emitted out-of-line for this instance:
//   Set<uint16_t,15>::Contains @ 0x822AF930  (called by Insert)
//   Set<uint16_t,15>::Erase    @ 0x822CA5A8  (PropEntityModule::UnloadZone)
//   Set<uint16_t,15>::Find     @ 0x822AF898  (Contains / Erase / SetDifference<15u,15u>)
//   Set<uint16_t,15>::Insert   @ 0x822CA500  (PropEntityModule::GenerateTargetList / LoadZone / SetDifference)
//
// LAYOUT (X360 element addressing + count word, authoritative):
//   maElements[15]   +0x00  (15 * 2 = 30 bytes; uint16_t element -- 2-byte stride, lhz/sthx)
//   muLength         +0x20  (count word read/written at *(this+0x20); +0x20 = 30 padded to u32 align)
// Capacity N == 15 (Insert out-of-space gate `cmplwi muLength,0xF`).
//
// Spelled unqualified to match the committed Set<T,N> / Array<T,N> container convention
// (CgsSet.h); the DecFIGS DWARF spells the type CgsContainers::Set<uint16_t,15u>. uint16_t is a
// plain primitive whose built-in operator== satisfies the equality-based generic Find/Contains.
#include "GameShared/GameClasses/Containers/CgsSet.h"

template class Set<u16, 15>;

// SetDifference is a member-function TEMPLATE (parameterised on the two source-set
// capacities), so the explicit `template class Set<u16,15>` above does NOT emit it -- it
// needs its own explicit instantiation. The X360 emitted one out-of-line copy for the
// <15u,15u> pairing (both operands Set<u16,15>):
//   Set<u16,15>::SetDifference<15,15> @ 0x822E5440  (BrnWorld::PropEntityModule::UpdateInstanceStreaming)
// The generic body (CgsSet.h SetDifference) is faithful: Clear() then, for each element of
// lA, Insert it when lB does not Contain it. In the emitted asm Contains is inlined, so the
// CgsSet.h:332 constructed-assert is followed by a direct Find call (Contains == Find !=
// KU_INVALID); GetLength (:227) and operator[]/GetItem (:257/:258) asserts all match.
template void Set<u16, 15>::SetDifference<15, 15>(const Set<u16, 15>& lA, const Set<u16, 15>& lB);

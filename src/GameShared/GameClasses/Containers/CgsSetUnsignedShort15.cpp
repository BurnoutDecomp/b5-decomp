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

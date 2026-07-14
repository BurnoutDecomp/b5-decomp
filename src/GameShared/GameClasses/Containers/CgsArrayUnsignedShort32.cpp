// Per-instantiation .cpp for Array<u16, 32>. The generic Array<T,N> body (AppendArray / Append /
// operator[] / GetLength + siblings) is fully inline in CgsArray.h, so this TU is just the explicit
// class instantiation (the X360 emits one out-of-line copy of each used member per using-TU). The
// member the X360 emitted out-of-line for this instance is:
//   Array<u16,32>::AppendArray<32> @ 0x82374258  (called by BrnGameState::TriggerQueryManager::PreWorldUpdate)
// which walks the source array by index and Appends each element into this one.
//
// NOTE ON THE MANGLED NAME: IDA demangles this instantiation's members to the truncated prefix
// 'short,32>' (e.g. short_32_::Append). The asserts disambiguate: AppendArray<32> streams the
// CgsArray.h messages ('Array used before Construct/Clear was called' :245/:336; 'Array container
// out of space appending an array' :246), so it is the Array<u16,32> instantiation.
//
// LAYOUT (X360 element addressing + count word, authoritative from the AppendArray<32> asm
// @ 0x82374258):
//   maElements[32]   +0x00  (32 * 2 = 64 bytes; u16 element -- 2-byte stride)
//   miCount          +0x40  (count word read at *(this+0x40), `lwz r11,0x40(r29)` / `lwz 0x40(r30)`)
// The out-of-space gate compares (source.miCount + this.miCount) against 0x20 == 32 == capacity N.
//
// Spelled unqualified to match the committed Array<T,N> container convention (CgsArray.h); the
// DecFIGS DWARF spells the type CgsContainers::Array<uint16_t,32u>. u16 is a plain primitive whose
// built-in operator== satisfies the equality-based generic members.
#include "GameShared/GameClasses/Containers/CgsArray.h"

template class Array<u16, 32>;

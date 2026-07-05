// Per-instantiation .cpp for Array<u16, 72>. The generic Array<T,N> body (Append / operator[]
// / GetLength + siblings) is fully inline in CgsArray.h, so this TU is just the explicit class
// instantiation (the X360 emits one out-of-line copy of each used member per using-TU). The one
// member the X360 emitted out-of-line for this instance:
//   Array<u16,72>::Append @ 0x8270CBD8  (called by SetDifference<72u,72u> AppendSet path)
//
// NOTE ON THE MANGLED NAME: IDA demangles this instantiation's members to the truncated prefix
// 'short,72>' -- the same truncation it produces for Set<u16,72>. The asserts disambiguate: this
// member streams the CgsArray.h messages ('Array used before Construct/Clear was called' :225;
// 'Array container out of space, Length: <len>, Capacity: <72>' :226), so it is the Array<u16,72>
// instantiation. The sibling Set<u16,72> members (Contains/Find/GetItem/GetLength/Insert) live in
// CgsSetUnsignedShort72.cpp.
//
// LAYOUT (X360 element addressing + count word, authoritative):
//   maElements[72]   +0x00  (72 * 2 = 144 bytes; u16 element -- 2-byte stride, lhz/sthx)
//   miCount          +0x90  (count word read/written at *(this+0x90), `lwz 0x90(r29)`)
// matching the slwi-by-1 element addressing (2-byte stride) and the single-halfword element copy
// (Append: `lhz r10,0(a2); slwi r11,muCount,1; sthx r10,r11,this` -- maElements[miCount]=*a2 then
// post-increment). Capacity N == 72 (Append out-of-space gate `cmplwi miCount,0x48`).
//
// Spelled unqualified to match the committed Array<T,N> / Set<T,N> container convention (CgsArray.h);
// the DecFIGS DWARF spells the type CgsContainers::Array<uint16_t,72u>. u16 is a plain primitive
// whose built-in operator== satisfies the equality-based generic Contains/FindFirstInstanceOf.
#include "GameShared/GameClasses/Containers/CgsArray.h"

template class Array<u16, 72>;

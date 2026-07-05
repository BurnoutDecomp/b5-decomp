// ===== TARGET FILE: b5-decomp/src/GameSource/GameState/Array_short_8.cpp =====
// (Combined per-instantiation .cpp. Both generic bodies already live inline in the
//  committed CgsArray.h / CgsStack.h; this TU only forces out-of-line emission.)
//
// Explicit instantiation(s) of the generic fixed-capacity containers for the leaf
// instantiations Array<u16,8> and Stack<u16,8>. The IDA-truncated ledger key "short,8>"
// conflates the two distinct generics that share this using-TU. Both element = unsigned
// short (u16), extent N=8; all bodies are header-inline, so this .cpp just forces emission.
// Mirrors the sibling Array_short_25.cpp / Array_short_256.cpp in this same directory.
//
//   Array<u16,8>  (CgsArray.h, inline body; top-level class -- NO namespace):
//     Append   @ 0x8235DFA8  -- CgsArray.h:225/226 asserts
//     GetItem  @ 0x82360000  -- CgsArray.h:556/557 asserts (routes via operator[])
//   Stack<u16,8>  (CgsStack.h, inline body; namespace CgsContainers):
//     IsFull   @ 0x8235E818  -- CgsStack.h:169 assert
//     Push     @ 0x823694D0  -- CgsStack.h:98/169/99 asserts
//     Pop      @ 0x82369590  -- CgsStack.h:121/177/122 asserts
//     Peek     @ 0x82369640  -- CgsStack.h:149/177/150 asserts
//
// Element type unsigned short: the asm loads/stores each element with lhz/sthx (halfword)
// and indexes with a 2-byte stride (slwi ...,1). Both containers embed maElements/maData[8]
// (== 16 bytes) followed by the live-count word at +0x10 (Array miCount / Stack miLength),
// initialised to the -1 (Array) / 0x7FFFFFFF (Stack) sentinel until Construct/Clear runs.
// The Append/Push out-of-space asserts compare the count against 8, fixing N=8. A primitive
// element needs no element_home include.
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/Containers/CgsStack.h"

// --- Array<u16,8> (CgsArray.h defines Array as an UNQUALIFIED top-level class) ---
template void Array<u16, 8>::Append(const u16&);
template u16& Array<u16, 8>::GetItem(u32);

// --- Stack<u16,8> (namespace CgsContainers) ---
template bool        CgsContainers::Stack<u16, 8>::IsFull() const;
template void        CgsContainers::Stack<u16, 8>::Push(const u16&);
template void        CgsContainers::Stack<u16, 8>::Pop();
template const u16&  CgsContainers::Stack<u16, 8>::Peek() const;

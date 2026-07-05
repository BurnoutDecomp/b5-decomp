// Explicit instantiation(s) of the generic Array<T,N> container methods (all inline in
// CgsArray.h) for the Array<u16, 25> leaf instantiation -- the committed Array_/EventQueue_
// explicit-instantiation pattern (mirrors the sibling Array_short_256.cpp).
//
// Element type is the X360-authoritative 16-bit word: the asm loads/stores the element with
// lhz/sthx (halfword) and indexes with a 2-byte stride (slwi ...,1). Capacity N=25 (Append
// out-of-space `cmplwi 0x19`); the live-count word sits at +0x34, initialised to the -1
// sentinel until Construct/Clear runs. A primitive element needs no element_home include.
//
//   X360 0x82318590 = Array<u16,25>::GetLength (const)     -- CgsArray.h:336 assert
//   X360 0x823185E8 = Array<u16,25>::GetItem   (non-const) -- CgsArray.h:538/539 asserts
//   X360 0x8279BB90 = Array<u16,25>::Append                -- CgsArray.h:225/226 asserts
//
#include "GameShared/GameClasses/Containers/CgsArray.h"

template u32  Array<u16, 25>::GetLength() const;
template u16& Array<u16, 25>::GetItem(u32);
template void Array<u16, 25>::Append(const u16&);
